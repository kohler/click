// -*- c-basic-offset: 4 -*-
/*
 * clickfs.cc -- the Click filesystem
 * Eddie Kohler
 *
 * Copyright (c) 2002 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "modulepriv.hh"
#include "proclikefs.h"

#include <click/router.hh>
#include <click/error.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/spinlock.h>
#include <linux/locks.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

static struct file_operations click_dir_file_ops;
static struct inode_operations click_dir_inode_ops;
static struct file_operations click_handler_file_ops;
static struct inode_operations click_handler_inode_ops;
static struct dentry_operations click_dentry_ops;
static struct proclikefs_file_system *clickfs;

static spinlock_t click_config_lock;
extern atomic_t click_config_generation;


/******************************** Quicksort **********************************/

static int
click_qsort_partition(void *base_v, size_t size, int left, int right,
		      int (*compar)(const void *, const void *),
		      int &split_left, int &split_right)
{
    if (size >= 64) {
	printk("<1>click_qsort_partition: elements too large!\n");
	return -E2BIG;
    }
    
    uint8_t pivot[64], tmp[64];
    uint8_t *base = reinterpret_cast<uint8_t *>(base_v);

    // Dutch national flag algorithm
    int middle = left;
    memcpy(&pivot[0], &base[size * ((left + right) / 2)], size);

    // loop invariant:
    // base[i] < pivot for all left_init <= i < left
    // base[i] > pivot for all right < i <= right_init
    // base[i] == pivot for all left <= i < middle
    while (middle <= right) {
	int cmp = compar(&base[size * middle], &pivot[0]);
	if (cmp < 0) {
	    memcpy(&tmp[0], &base[size * left], size);
	    memcpy(&base[size * left], &base[size * middle], size);
	    memcpy(&base[size * middle], &tmp[0], size);
	    left++;
	    middle++;
	} else if (cmp > 0) {
	    memcpy(&tmp[0], &base[size * right], size);
	    memcpy(&base[size * right], &base[size * middle], size);
	    memcpy(&base[size * middle], &tmp[0], size);
	    right--;
	} else
	    middle++;
    }

    // afterwards, middle == right + 1
    // so base[i] == pivot for all left <= i <= right
    split_left = left - 1;
    split_right = right + 1;
}

static void
click_qsort_subroutine(void *base, size_t size, int left, int right, int (*compar)(const void *, const void *))
{
    // XXX recursion
    if (left < right) {
	int split_left, split_right;
	click_qsort_partition(base, size, left, right, compar, split_left, split_right);
	click_qsort_subroutine(base, size, left, split_left, compar);
	click_qsort_subroutine(base, size, split_right, right, compar);
    }
}

static void
click_qsort(void *base, size_t n, size_t size, int (*compar)(const void *, const void *))
{
    click_qsort_subroutine(base, size, 0, n - 1, compar);
}


/*************************** Inode constants ********************************/

#define CSE_NULL			0xFFFFU
#define CSE_FAKE			1
#define CSE_HANDLER_CONFLICT		2
#define CSE_SUBDIR_CONFLICTS_CALCULATED	4

// NB: inode number 0 is reserved for the system.
#define INO_DIRTYPE(ino)		((ino) >> 28)
#define INO_ELEMENTNO(ino)		((int)((ino) & 0xFFFFU) - 1)
#define INO_HANDLERNO(ino)		((((ino) & 0xFFFFU) ? 0 : Router::FIRST_GLOBAL_HANDLER) + (((ino) >> 16) & 0x7FFFU))
#define INO_DT_H			0x1U /* handlers only */
#define INO_DT_N			0x2U /* names; >= 2 -> has names */
#define INO_DT_HN			0x3U /* handlers + names */
#define INO_DT_GLOBAL			0x4U /* handlers + names + all #s */
#define INO_DT_HAS_H(ino)		(INO_DIRTYPE((ino)) != INO_DT_N)
#define INO_DT_HAS_N(ino)		(INO_DIRTYPE((ino)) >= INO_DT_N)
#define INO_DT_HAS_U(ino)		(INO_DIRTYPE((ino)) == INO_DT_GLOBAL)

#define INO_MKHANDLER(e, hi)		((((hi) & 0x7FFFU) << 16) | (((e) + 1) & 0xFFFFU) | 0x80000000U)
#define INO_MKHDIR(e)			((INO_DT_H << 28) | (((e) + 1) & 0xFFFFU))
#define INO_MKHNDIR(e)			((INO_DT_HN << 28) | (((e) + 1) & 0xFFFFU))
#define INO_GLOBALDIR			(INO_DT_GLOBAL << 28)
#define INO_ISHANDLER(ino)		(((ino) & 0x80000000U) != 0)

#define INO_NLINK_GLOBAL_HANDLER	1
#define INO_NLINK_LOCAL_HANDLER		2

#define INODE_INFO(inode)		(*((ClickInodeInfo *)(&(inode)->u)))

struct ClickInodeInfo {
    struct proclikefs_inode_info padding;
    uint32_t config_generation;
};

inline bool
inode_out_of_date(struct inode *inode)
{
    return INO_ELEMENTNO(inode->i_ino) >= 0
	&& INODE_INFO(inode).config_generation != atomic_read(&click_config_generation);
}


/*************************** sorted_elements ********************************/

// NB: Assume that no global handlers have names that conflict with element
// *numbers*.

struct ClickSortedElement {
    String name;
    uint16_t elementno;
    uint16_t skip;
    uint16_t flags;
    uint16_t sorted_index;
};

static ClickSortedElement *sorted_elements;
static int nsorted_elements, sorted_elements_cap;
static uint32_t sorted_elements_generation;

static int
grow_sorted_elements(int min_size)
{
    if (sorted_elements_cap >= min_size)
	return 0;
    int new_cap = (sorted_elements_cap ? sorted_elements_cap : 128);
    while (new_cap < min_size)
	new_cap *= 2;
    // cheat on memory: bad me!
    ClickSortedElement *nse = (ClickSortedElement *)(new uint8_t[sizeof(ClickSortedElement) * new_cap]);
    if (!nse)
	return -ENOMEM;
    memcpy(nse, sorted_elements, sizeof(ClickSortedElement) * sorted_elements_cap);
    for (int i = sorted_elements_cap; i < new_cap; i++)
	new((void *)&nse[i]) String();
    delete[] ((uint8_t *)sorted_elements);
    sorted_elements = nse;
    sorted_elements_cap = new_cap;
    return 0;
}

static void
free_sorted_elements()
{
    for (int i = 0; i < sorted_elements_cap; i++)
	sorted_elements[i].name.~String();
    delete[] ((uint8_t *)sorted_elements);
    sorted_elements = 0;
    nsorted_elements = sorted_elements_cap = 0;
}

static int
sorted_element_compar(const void *v1, const void *v2)
{
    const ClickSortedElement *a = reinterpret_cast<const ClickSortedElement *>(v1);
    const ClickSortedElement *b = reinterpret_cast<const ClickSortedElement *>(v2);
    return String::compare(a->name, b->name);
}

static int
prepare_sorted_elements()
{
    // config lock must be held!
    nsorted_elements = 0;
    if (!click_router || click_router->nelements() == 0) {
	sorted_elements_generation = atomic_read(&click_config_generation);
	return 0;
    }
    else if (grow_sorted_elements(click_router->nelements()) < 0)
	return -ENOMEM;

    // initialize sorted_elements with info from the current router
    int nelem = click_router->nelements();
    for (int i = 0; i < nelem; i++) {
	sorted_elements[i].name = click_router->ename(i);
	sorted_elements[i].elementno = i;
	sorted_elements[i].skip = 0;
	sorted_elements[i].flags = 0;
    }

    // sort sorted_elements
    click_qsort(&sorted_elements[0], nelem, sizeof(ClickSortedElement), sorted_element_compar);

    // add new sorted_elements for intermediate directories
    int n = nelem;
    for (int i = 0; i < nelem; i++) {
	String name = sorted_elements[i].name;
	String last_name = (i ? sorted_elements[i-1].name : String());
	int slash = name.find_left('/');
	while (slash >= 0
	       && (name.substring(0, slash) != last_name.substring(0, slash)
		   || (last_name.length() > slash && last_name[slash] != '/'))) {
	    if (n >= sorted_elements_cap && grow_sorted_elements(n + 1) < 0)
		return -ENOMEM;
	    sorted_elements[n].name = name.substring(0, slash);
	    sorted_elements[n].elementno = n;
	    sorted_elements[n].skip = 0;
	    sorted_elements[n].flags = CSE_FAKE;
	    n++;
	    slash = name.find_left('/', slash + 1);
	}
    }

    // resort sorted_elements if necessary
    if (n != nelem)
	click_qsort(&sorted_elements[0], n, sizeof(ClickSortedElement), sorted_element_compar);

    // calculate 'skip'
    for (int i = 0; i < n - 1; i++) {
	const String &name = sorted_elements[i].name;
	int length = name.length();
	int j = i + 1;
	while (j < n && sorted_elements[j].name.length() > length
	       && sorted_elements[j].name[length] == '/'
	       && sorted_elements[j].name.substring(0, length) == name) {
	    sorted_elements[i].skip++;
	    j++;
	}
    }

    // calculate 'sorted_index'
    for (int i = 0; i < n; i++)
	sorted_elements[ sorted_elements[i].elementno ].sorted_index = i;

    // done
    nsorted_elements = n;
    sorted_elements_generation = atomic_read(&click_config_generation);
    return 0;
}

static int
string_compar(const void *v1, const void *v2)
{
    const String *a = reinterpret_cast<const String *>(v1);
    const String *b = reinterpret_cast<const String *>(v2);
    return String::compare(*a, *b);
}

static void
calculate_handler_conflicts(int parent_eindex)
{
    // configuration lock must be held!
    
    // no conflicts if no router
    assert(parent_eindex < nsorted_elements);
    int parent_sindex = (parent_eindex < 0 ? -1 : sorted_elements[parent_eindex].sorted_index);
    if (!click_router)
	return;
    if (parent_sindex >= 0 && (sorted_elements[parent_sindex].flags & CSE_SUBDIR_CONFLICTS_CALCULATED))
	return;
    if (parent_sindex >= 0 && sorted_elements[parent_sindex].skip == 0) {
	sorted_elements[parent_sindex].flags |= CSE_SUBDIR_CONFLICTS_CALCULATED;
	return;
    }

    // find the relevant handler indexes and names
    Vector<int> hindexes;
    click_router->element_handlers(parent_eindex, hindexes);
    Vector<String> names;
    for (int i = 0; i < hindexes.size(); i++) {
	const Router::Handler &h = click_router->handler(hindexes[i]);
	if (h.visible())
	    names.push_back(h.name());
    }

    // sort names
    if (names.size())
	click_qsort(&names[0], names.size(), sizeof(String), string_compar);

    // run over the arrays, marking conflicts
    int sindex = parent_sindex + 1;
    int last_sindex = (sindex == 0 ? nsorted_elements : sindex + sorted_elements[parent_sindex].skip);
    int hindex = 0;
    while (sindex < last_sindex && hindex < names.size()) {
	int compare = String::compare(sorted_elements[sindex].name, names[hindex]);
	if (compare == 0) {	// there is a conflict
	    sorted_elements[sindex].flags |= CSE_HANDLER_CONFLICT;
	    sindex += sorted_elements[sindex].skip + 1;
	    hindex++;
	} else if (compare < 0)
	    sindex += sorted_elements[sindex].skip + 1;
	else
	    hindex++;
    }

    // mark subdirectory as calculated
    if (parent_sindex >= 0)
	sorted_elements[parent_sindex].flags |= CSE_SUBDIR_CONFLICTS_CALCULATED;
}


/*************************** Inode operations ********************************/

static void
calculate_inode_nlink(struct inode *inode)
{
    // must be called with config_lock held
    unsigned long ino = inode->i_ino;
    int elementno = INO_ELEMENTNO(ino);
    inode->i_nlink = 2;
    if (INO_DIRTYPE(ino) != INO_DT_H) {
	if (INO_DT_HAS_U(ino) && click_router)
	    inode->i_nlink += click_router->nelements();
	if (INO_DT_HAS_N(ino)) {
	    int first_sindex = (elementno < 0 ? 0 : sorted_elements[elementno].sorted_index + 1);
	    if (elementno >= 0
		&& !(sorted_elements[first_sindex - 1].flags & CSE_SUBDIR_CONFLICTS_CALCULATED))
		calculate_handler_conflicts(elementno);
	    int last_sindex = (elementno < 0 ? nsorted_elements : first_sindex + sorted_elements[first_sindex - 1].skip);
	    while (first_sindex < last_sindex) {
		if (!(sorted_elements[first_sindex].flags & CSE_HANDLER_CONFLICT) || INO_DIRTYPE(ino) == INO_DT_N)
		    inode->i_nlink++;
		first_sindex += sorted_elements[first_sindex].skip + 1;
	    }
	}
    }
}

static struct inode *
click_inode(struct super_block *sb, unsigned long ino)
{
    struct inode *inode = iget(sb, ino);
    if (!inode)
	return 0;

    spin_lock(&click_config_lock);
    
    int elementno = INO_ELEMENTNO(ino);
    if (atomic_read(&click_config_generation) != sorted_elements_generation)
	prepare_sorted_elements();
    INODE_INFO(inode).config_generation = sorted_elements_generation;
    
    if (INO_ISHANDLER(ino)) {
	int hi = INO_HANDLERNO(ino);
	if (Router::handler_ok(click_router, hi)) {
	    const Router::Handler &h = Router::handler(click_router, hi);
	    inode->i_mode = S_IFREG | (h.read_visible() ? click_mode_r : 0) | (h.write_visible() ? click_mode_w : 0);
	    inode->i_uid = inode->i_gid = 0;
	    inode->i_op = &click_handler_inode_ops;
	    inode->i_nlink = (elementno < 0 ? INO_NLINK_GLOBAL_HANDLER : INO_NLINK_LOCAL_HANDLER);
	} else {
	    iput(inode);
	    inode = 0;
	}
    } else if (elementno >= 0 && (!click_router || elementno >= nsorted_elements)) {
	// invalid directory
	iput(inode);
	inode = 0;
    } else {
	inode->i_mode = click_mode_dir;
	inode->i_uid = inode->i_gid = 0;
	inode->i_op = &click_dir_inode_ops;
	calculate_inode_nlink(inode);
    }

    spin_unlock(&click_config_lock);
    return inode;
}



/*************************** Directory operations ****************************/

extern "C" {

static struct dentry *
click_dir_lookup(struct inode *dir, struct dentry *dentry)
{
    if (inode_out_of_date(dir))
	return reinterpret_cast<struct dentry *>(ERR_PTR(-ENOENT));
    
    unsigned long ino = dir->i_ino;
    int elementno = INO_ELEMENTNO(ino);
    String dentry_name = String::stable_string(reinterpret_cast<const char *>(dentry->d_name.name), dentry->d_name.len);
    struct inode *inode = 0;

    int error = -ENOENT;
    int first_sindex, last_sindex, offset;

    // lock the configuration
    spin_lock(&click_config_lock);
    
    // delimit boundaries of search region
    if (elementno < 0)
	first_sindex = offset = 0, last_sindex = nsorted_elements - 1;
    else {
	int sindex = sorted_elements[elementno].sorted_index;
	offset = sorted_elements[sindex].name.length() + 1;
	first_sindex = sindex + 1;
	last_sindex = first_sindex + sorted_elements[sindex].skip - 1;
    }

    // look for numbers
    if (INO_DT_HAS_U(ino) && dentry_name.length() && dentry_name[0] >= '1' && dentry_name[0] <= '9') {
	int eindex = dentry_name[0] - '0';
	for (int i = 1; i < dentry_name.length(); i++)
	    if (dentry_name[i] >= '0' && dentry_name[i] <= '9')
		eindex = (eindex * 10) + dentry_name[i] - '0';
	    else
		goto number_failed;
	eindex--;
	if (!click_router || eindex >= click_router->nelements())
	    goto number_failed;
	inode = click_inode(dir->i_sb, INO_MKHDIR(eindex));
	goto found;
    }
    
  number_failed:
    // look for handlers
    if (INO_DT_HAS_H(ino)) {
	int hi = Router::find_handler(click_router, elementno, dentry_name);
	if (hi >= 0) {
	    const Router::Handler &h = Router::handler(click_router, hi);
	    if (h.visible()) {
		inode = click_inode(dir->i_sb, INO_MKHANDLER(elementno, hi));
		goto found;
	    }
	}
    }

    // look for names
    if (INO_DT_HAS_N(ino)) {
	// binary search
	while (first_sindex <= last_sindex) {
	    int mid = (first_sindex + last_sindex) >> 1;

	    // a sort of strcmp(dentry_name, sorted_elements[mid].name)
	    const String &x = sorted_elements[mid].name;
	    int min_length = (x.length() - offset < dentry_name.length() ? x.length() - offset : dentry_name.length());
	    const char *ds = dentry_name.data(), *xs = x.data() + offset;
	    const char *de = ds + min_length;
	    int cmp = 0;
	    do {
		cmp = ((unsigned char)*ds++) - ((unsigned char)*xs++);
	    } while (cmp == 0 && ds < de);
	    if (cmp == 0)
		cmp = dentry_name.length() - (x.length() - offset);

	    if (cmp == 0) {
		// found it
		inode = click_inode(dir->i_sb, INO_MKHNDIR(sorted_elements[mid].elementno));
		goto found;
	    } else if (cmp < 0)
		last_sindex = mid - 1;
	    else
		first_sindex = mid + 1;
	}
    }

    // first_sindex, last_sindex destroyed here
    spin_unlock(&click_config_lock);
    return reinterpret_cast<struct dentry *>(ERR_PTR(-ENOENT));

  found:
    spin_unlock(&click_config_lock);
    if (inode) {
	dentry->d_op = &click_dentry_ops;
	d_add(dentry, inode);
	return 0;
    } else
	return reinterpret_cast<struct dentry *>(ERR_PTR(-EINVAL));
}

static int
click_dir_revalidate(struct dentry *dentry)
{
    struct inode *inode = dentry->d_inode;
    if (!inode)
	return -EINVAL;
    else if (INODE_INFO(inode).config_generation != atomic_read(&click_config_generation)) {
	if (INO_ELEMENTNO(inode->i_ino) >= 0) // not a global directory
	    return -EIO;
	spin_lock(&click_config_lock);
	if (atomic_read(&click_config_generation) != sorted_elements_generation)
	    prepare_sorted_elements();
	INODE_INFO(inode).config_generation = sorted_elements_generation;
	calculate_inode_nlink(inode);
	spin_unlock(&click_config_lock);
    }
    return 0;
}

#ifdef LINUX_2_2
#define DO_FILLDIR(dirent, name, namelen, ino, mode)	do { \
	if (filldir(dirent, name, namelen, f_pos, ino) < 0) { \
	    filp->f_pos = f_pos; return 0; \
	} } while (0)
#else
#define DO_FILLDIR(dirent, name, namelen, ino, mode)	do { \
	if (filldir(dirent, name, namelen, f_pos, ino, mode) < 0) { \
	    filp->f_pos = f_pos; return 0; \
	} } while (0)
#endif

static int
click_dir_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
    // File positions:
    // 0		.
    // 1		..
    // 2-0xFFFF		handlers
    // 0x10000-0x1FFFF  numbers
    // 0x20000-0x2FFFF	names

#define RD_HOFF		2
#define RD_UOFF		0x10000
#define RD_NOFF		0x20000
#define RD_XOFF		0x30000
    
    struct inode *inode = filp->f_dentry->d_inode;
    if (inode_out_of_date(inode))
	return -ENOENT;

    unsigned long ino = inode->i_ino;
    int elementno = INO_ELEMENTNO(ino);
    uint32_t f_pos = filp->f_pos;
    
    // '.' and '..'
    if (f_pos == 0) {
	DO_FILLDIR(dirent, ".", 1, ino, DT_DIR);
	f_pos++;
    }
    if (f_pos == 1) {
	DO_FILLDIR(dirent, "..", 2, filp->f_dentry->d_parent->d_inode->i_ino, DT_DIR);
	f_pos++;
    }

    // handler names
    if (f_pos < RD_UOFF && INO_DT_HAS_H(ino)) {
	Vector<int> hi;
	Router::element_handlers(click_router, elementno, hi);
	while (f_pos >= RD_HOFF && f_pos < hi.size() + RD_HOFF) {
	    const Router::Handler &h = Router::handler(click_router, hi[f_pos - RD_HOFF]);
	    if (h.visible())
		DO_FILLDIR(dirent, h.name().data(), h.name().length(), INO_MKHANDLER(elementno, hi[f_pos - RD_HOFF]), DT_REG);
	    f_pos++;
	}
    }

    // figure out edges of directory
    int first_sindex, last_sindex;
    if (elementno < 0)
	first_sindex = 0, last_sindex = nsorted_elements;
    else {
	int sindex = sorted_elements[elementno].sorted_index;
	first_sindex = sindex + 1;
	last_sindex = first_sindex + sorted_elements[sindex].skip;
    }

    // subdirectory numbers
    if (f_pos < RD_UOFF)
	f_pos = RD_UOFF;
    if (f_pos >= RD_NOFF)
	/* do nothing */;
    else if (INO_DIRTYPE(ino) == INO_DT_GLOBAL) {
	char buf[10];
	int nelem = (click_router ? click_router->nelements() : 0);
	while (f_pos >= RD_UOFF && f_pos < RD_UOFF + nelem) {
	    int elem = f_pos - RD_UOFF;
	    sprintf(buf, "%d", elem + 1);
	    DO_FILLDIR(dirent, buf, strlen(buf), INO_MKHDIR(elem), DT_DIR);
	    f_pos++;
	}
    }

    // subdirectory names
    if (f_pos < RD_NOFF)
	f_pos = RD_NOFF;
    if (f_pos >= RD_XOFF)
	/* do nothing */;
    else if (INO_DT_HAS_N(ino)) {
	bool include_conflicts = (INO_DIRTYPE(ino) == INO_DT_N);
	int i, j;
	int parent_length = (first_sindex == 0 ? 0 : sorted_elements[first_sindex - 1].name.length() + 1);
	for (i = first_sindex, j = RD_NOFF; i < last_sindex; i += sorted_elements[i].skip + 1, j++)
	    if (f_pos == j) {
		if (!(sorted_elements[i].flags & CSE_HANDLER_CONFLICT) || include_conflicts)
		    DO_FILLDIR(dirent, sorted_elements[i].name.data() + parent_length, sorted_elements[i].name.length() - parent_length, INO_MKHNDIR(sorted_elements[i].elementno), DT_DIR);
		f_pos++;
	    }
    }

    filp->f_pos = RD_XOFF;
    return 1;
}

} // extern "C"


/*************************** Superblock operations ***************************/

static struct super_operations click_superblock_ops;

extern "C" {

static void
click_read_inode(struct inode *inode)
{
    unsigned long ino = inode->i_ino;

    // XXX can do better for some handlers, particularly 'config'
    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
    
    inode->i_blocks = 0;
    inode->i_blksize = 1024;
    inode->i_op = 0;
    inode->i_mode = 0;
    inode->i_uid = 0;
    inode->i_gid = 0;
    inode->i_nlink = 1;
    inode->i_size = 0;

    // Why don't we fill out the inode more completely? This is weird.
    proclikefs_read_inode(inode);
}

static void
click_write_inode(struct inode *)
{
}

static void
click_put_inode(struct inode *inode)
{
    // Delete inodes when they're unused, since we can recreate them easily.
    if (inode->i_count == 1)
	inode->i_nlink = 0;
}

static struct super_block *
click_read_super(struct super_block *sb, void * /* data */, int)
{
    lock_super(sb);
    
    sb->s_blocksize = 1024;
    sb->s_blocksize_bits = 10;
    //sb->s_magic = PROC_SUPER_MAGIC;
    sb->s_op = &click_superblock_ops;
    struct inode *root_inode = click_inode(sb, INO_GLOBALDIR);
    if (!root_inode)
	goto out_no_root;
    sb->s_root = d_alloc_root(root_inode, 0);
    if (!sb->s_root)
	goto out_no_root;
    // XXX options
    
    unlock_super(sb);
    proclikefs_read_super(sb);
    return sb;

  out_no_root:
    printk("<1>click_read_super: get root inode failed\n");
    iput(root_inode);
    sb->s_dev = 0;
    unlock_super(sb);
    return 0;
}

static void
click_reread_super(struct super_block *sb)
{
    lock_super(sb);
    if (sb->s_root) {
	struct inode *old_inode = sb->s_root->d_inode;
	sb->s_root->d_inode = click_inode(sb, INO_GLOBALDIR);
	iput(old_inode);
	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;
	sb->s_op = &click_superblock_ops;
    } else
	printk("<1>silly click_reread_super\n");
    unlock_super(sb);
}

static void
click_delete_dentry(struct dentry *dentry)
{
    d_drop(dentry);
}

} // extern "C"


/*************************** Handler operations ******************************/

struct HandlerStringInfo {
    int next;
    int flags;
};

static String *handler_strings = 0;
static HandlerStringInfo *handler_strings_info = 0;
static int handler_strings_cap = 0;
static int handler_strings_free = -1;
static spinlock_t handler_strings_lock;

#define FILP_STRINGNO(filp)		(reinterpret_cast<int>((filp)->private_data))
#define FILP_READ_STRINGNO(filp)	FILP_STRINGNO(filp)
#define FILP_WRITE_STRINGNO(filp)	FILP_STRINGNO(filp)

static int
increase_handler_strings()
{
    // must be called with handler_strings_lock held

    if (handler_strings_cap < 0)	// in process of cleaning up module
	return -1;
    
    int new_cap = (handler_strings_cap ? 2*handler_strings_cap : 16);
    String *new_strs = new String[new_cap];
    if (!new_strs)
	return -1;
    HandlerStringInfo *new_infos = new HandlerStringInfo[new_cap];
    if (!new_infos) {
	delete[] new_strs;
	return -1;
    }
    
    for (int i = 0; i < handler_strings_cap; i++)
	new_strs[i] = handler_strings[i];
    for (int i = handler_strings_cap; i < new_cap; i++)
	new_infos[i].next = i + 1;
    new_infos[new_cap - 1].next = handler_strings_free;
    memcpy(new_infos, handler_strings_info, sizeof(HandlerStringInfo) * handler_strings_cap);

    delete[] handler_strings;
    delete[] handler_strings_info;
    handler_strings_free = handler_strings_cap;
    handler_strings_cap = new_cap;
    handler_strings = new_strs;
    handler_strings_info = new_infos;

    return 0;
}

static int
next_handler_string(const Router::Handler *h)
{
    spin_lock(&handler_strings_lock);
    if (handler_strings_free < 0)
	increase_handler_strings();
    int hs = handler_strings_free;
    if (hs >= 0) {
	handler_strings_free = handler_strings_info[hs].next;
	handler_strings_info[hs].flags = h->flags() | HANDLER_NEED_READ;
    }
    spin_unlock(&handler_strings_lock);
    return hs;
}

static void
free_handler_string(int hs)
{
    spin_lock(&handler_strings_lock);
    if (hs >= 0 && hs < handler_strings_cap) {
	handler_strings[hs] = String();
	handler_strings_info[hs].next = handler_strings_free;
	handler_strings_free = hs;
    }
    spin_unlock(&handler_strings_lock);
}

static const Router::Handler *
find_handler(int eindex, int handlerno)
{
    if (Router::handler_ok(click_router, handlerno))
	return &Router::handler(click_router, handlerno);
    else
	return 0;
}

extern "C" {

static int
handler_open(struct inode *inode, struct file *filp)
{
    spin_lock(&click_config_lock);

    bool reading = (filp->f_flags & O_ACCMODE) != O_WRONLY;
    bool writing = (filp->f_flags & O_ACCMODE) != O_RDONLY;
    
    int retval = 0;
    int stringno = -1;
    const Router::Handler *h;
    
    if ((reading && writing)
	|| (filp->f_flags & O_APPEND)
	|| (writing && !(filp->f_flags & O_TRUNC)))
	retval = -EACCES;
    else if (inode_out_of_date(inode))
	retval = -EIO;
    else if (!(h = find_handler(INO_ELEMENTNO(inode->i_ino), INO_HANDLERNO(inode->i_ino))))
	retval = -EIO;
    else if ((reading && !h->read_visible())
	     || (writing && !h->write_visible()))
	retval = -EPERM;
    else if ((stringno = next_handler_string(h)) < 0)
	retval = -ENOMEM;
    else {
	handler_strings[stringno] = String();
	retval = 0;
    }

    spin_unlock(&click_config_lock);
    
    if (retval < 0 && stringno >= 0) {
	free_handler_string(stringno);
	stringno = -1;
    }
    filp->private_data = reinterpret_cast<void *>(stringno);
    return retval;
}

static ssize_t
handler_read(struct file *filp, char *buffer, size_t count, loff_t *store_f_pos)
{
    loff_t f_pos = *store_f_pos;
    int stringno = FILP_READ_STRINGNO(filp);
    if (stringno < 0 || stringno >= handler_strings_cap)
	return -EIO;

    // (re)read handler if necessary
    if (handler_strings_info[stringno].flags & (HANDLER_REREAD | HANDLER_NEED_READ)) {
	spin_lock(&click_config_lock);
	int retval;
	const Router::Handler *h;
	struct inode *inode = filp->f_dentry->d_inode;
	if (inode_out_of_date(inode)
	    || !(h = find_handler(INO_ELEMENTNO(inode->i_ino), INO_HANDLERNO(inode->i_ino))))
	    retval = -EIO;
	else if (!h->read_visible())
	    retval = -EPERM;
	else {
	    int eindex = INO_ELEMENTNO(inode->i_ino);
	    Element *e = (eindex >= 0 ? click_router->element(eindex) : 0);
	    handler_strings[stringno] = h->call_read(e);
	    retval = (handler_strings[stringno].out_of_memory() ? -ENOMEM : 0);
	}
	spin_unlock(&click_config_lock);
	if (retval < 0)
	    return retval;
	handler_strings_info[stringno].flags &= ~HANDLER_NEED_READ;
    }

    const String &s = handler_strings[stringno];
    if (f_pos + count > s.length())
	count = s.length() - f_pos;
    if (copy_to_user(buffer, s.data() + f_pos, count) > 0)
	return -EFAULT;
    *store_f_pos += count;
    return count;
}

static ssize_t
handler_write(struct file *filp, const char *buffer, size_t count, loff_t *store_f_pos)
{
    loff_t f_pos = *store_f_pos;
    int stringno = FILP_WRITE_STRINGNO(filp);
    if (stringno < 0 || stringno >= handler_strings_cap)
	return -EIO;
    String &s = handler_strings[stringno];
    int old_length = s.length();

#ifdef LARGEST_HANDLER_WRITE
    if (f_pos + count > LARGEST_HANDLER_WRITE
	&& !(handler_strings_info[stringno].flags & HANDLER_WRITE_UNLIMITED))
	return -EFBIG;
#endif

    if (f_pos + count > old_length) {
	s.append_fill(0, f_pos + count - old_length);
	if (s.out_of_memory())
	    return -ENOMEM;
    }

    int length = s.length();
    if (f_pos > length)
	return -EFBIG;
    else if (f_pos + count > length)
	count = length - f_pos;

    char *data = s.mutable_data();
    if (f_pos > old_length)
	memset(data + old_length, 0, f_pos - old_length);

    if (copy_from_user(data + f_pos, buffer, count) > 0)
	return -EFAULT;

    *store_f_pos += count;
    return count;
}

static int
handler_flush(struct file *filp)
{
    bool writing = (filp->f_flags & O_ACCMODE) != O_RDONLY;
    int stringno = FILP_WRITE_STRINGNO(filp);
    int retval = 0;

#ifdef LINUX_2_2
    int f_count = filp->f_count;
#else
    int f_count = atomic_read(&filp->f_count);
#endif
    
    if (writing && f_count == 1
	&& stringno >= 0 && stringno < handler_strings_cap) {
	spin_lock(&click_config_lock);
	
	struct inode *inode = filp->f_dentry->d_inode;
	const Router::Handler *h;
	
	if (inode_out_of_date(inode)
	    || !(h = find_handler(INO_ELEMENTNO(inode->i_ino), INO_HANDLERNO(inode->i_ino)))
	    || !h->write_visible())
	    retval = -EIO;
	else if (handler_strings[stringno].out_of_memory())
	    retval = -ENOMEM;
	else {
	    int eindex = INO_ELEMENTNO(inode->i_ino);
	    Element *e = (eindex >= 0 ? click_router->element(eindex) : 0);
	    String context_string = "In write handler `" + h->name() + "'";
	    if (e)
		context_string += String(" for `") + e->declaration() + "'";
	    ContextErrorHandler cerrh(click_logged_errh, context_string + ":");
	    retval = h->call_write(handler_strings[stringno], e, &cerrh);
	}

	spin_unlock(&click_config_lock);
    }

    return retval;
}

static int
handler_release(struct inode *, struct file *filp)
{
    // free handler string
    int stringno = FILP_READ_STRINGNO(filp);
    if (stringno >= 0)
	free_handler_string(stringno);
    return 0;
}

static int
handler_ioctl(struct inode *inode, struct file *filp,
	      unsigned command, unsigned long address)
{
    spin_lock(&click_config_lock);

    int retval;
    Element *e;
    
    if (inode_out_of_date(inode))
	retval = -EIO;
    else if (!click_router)
	retval = -EINVAL;
    else if (!(e = click_router->element(INO_ELEMENTNO(inode->i_ino))))
	retval = -EIO;
    else if (click_router->initialized())
	retval = e->llrpc(command, reinterpret_cast<void *>(address));
    else
	retval = e->Element::llrpc(command, reinterpret_cast<void *>(address));

    spin_unlock(&click_config_lock);
    return retval;
}

} // extern "C"


/*********************** Initialization and termination **********************/

int
init_clickfs()
{
    static_assert(sizeof(((struct inode *)0)->u) >= sizeof(ClickInodeInfo));
    
    click_superblock_ops.read_inode = click_read_inode;
    click_superblock_ops.write_inode = click_write_inode;
    click_superblock_ops.put_inode = click_put_inode;
    click_superblock_ops.delete_inode = proclikefs_delete_inode;
    click_superblock_ops.put_super = proclikefs_put_super;
    // XXX statfs

    click_dentry_ops.d_delete = click_delete_dentry;

#ifdef LINUX_2_4
    click_dir_file_ops.owner = THIS_MODULE;
    click_dir_file_ops.read = generic_read_dir;
#endif
    click_dir_file_ops.readdir = click_dir_readdir;
    click_dir_inode_ops.lookup = click_dir_lookup;
    click_dir_inode_ops.revalidate = click_dir_revalidate;
    click_dir_inode_ops.default_file_ops = &click_dir_file_ops;

#ifdef LINUX_2_4
    click_handler_file_ops.owner = THIS_MODULE;
#endif
    click_handler_file_ops.read = handler_read;
    click_handler_file_ops.write = handler_write;
    click_handler_file_ops.ioctl = handler_ioctl;
    click_handler_file_ops.open = handler_open;
    click_handler_file_ops.flush = handler_flush;
    click_handler_file_ops.release = handler_release;

#ifdef LINUX_2_2
    click_handler_inode_ops.default_file_ops = &click_handler_file_ops;
#endif

    spin_lock_init(&handler_strings_lock);
    sorted_elements_generation = 0; // click_config_generation starts at 1

    clickfs = proclikefs_register_filesystem("click", click_read_super, click_reread_super);
    if (!clickfs) {
	printk("<1>click: could not initialize clickfs!\n");
	return -EINVAL;
    } else
	return 0;
}

void
cleanup_clickfs()
{
    proclikefs_unregister_filesystem(clickfs);

    // clean up handler_strings
    spin_lock(&handler_strings_lock);
    delete[] handler_strings;
    delete[] handler_strings_info;
    handler_strings = 0;
    handler_strings_info = 0;
    handler_strings_cap = -1;
    handler_strings_free = -1;
    spin_unlock(&handler_strings_lock);
    
    free_sorted_elements();
}

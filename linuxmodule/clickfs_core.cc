// -*- c-basic-offset: 4 -*-
/*
 * proc_dir.cc -- the Click filesystem
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

static struct file_operations click_dir_ops;
static struct inode_operations click_dir_inode_ops;
static struct dentry_operations click_dentry_ops;
static struct proclikefs_file_system *clickfs;

static spinlock_t click_config_lock;
extern atomic_t click_config_generation;


// SORT ROUTINES

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


/*************************** Click superblock ********************************/

#define CSE_NULL			0xFFFFU
#define CSE_FAKE			1
#define CSE_HANDLER_CONFLICT		2
#define CSE_SUBDIR_CONFLICTS_CALCULATED	4

// NB: inode number 0 is reserved for the system.
#define INO_ELEMENTNO(ino)		((int)((ino) & 0xFFFFU) - 1)
#define INO_DIRTYPE(ino)		((ino) >> 28)
#define INO_HANDLERNO(ino)		(((ino) >> 16) & 0x7FFFU)
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

#define LOCK_CONFIG			spin_lock(&click_config_lock)
#define UNLOCK_CONFIG			spin_unlock(&click_config_lock)

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
    if (!current_router || current_router->nelements() == 0) {
	sorted_elements_generation = atomic_read(&click_config_generation);
	return 0;
    }
    else if (grow_sorted_elements(current_router->nelements()) < 0)
	return -ENOMEM;

    // initialize sorted_elements with info from the current router
    int nelem = current_router->nelements();
    for (int i = 0; i < nelem; i++) {
	sorted_elements[i].name = current_router->ename(i);
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
    if (!current_router)
	return;
    if (parent_sindex >= 0 && (sorted_elements[parent_sindex].flags & CSE_SUBDIR_CONFLICTS_CALCULATED))
	return;
    if (parent_sindex >= 0 && sorted_elements[parent_sindex].skip == 0) {
	sorted_elements[parent_sindex].flags |= CSE_SUBDIR_CONFLICTS_CALCULATED;
	return;
    }

    // find the relevant handler indexes and names
    Vector<int> hindexes;
    current_router->element_handlers(parent_eindex, hindexes);
    Vector<String> names;
    for (int i = 0; i < hindexes.size(); i++) {
	const Router::Handler &h = current_router->handler(hindexes[i]);
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



// INODE OPERATIONS

static struct inode *
click_inode(struct super_block *sb, unsigned long ino)
{
    struct inode *inode = iget(sb, ino);
    if (!inode)
	return 0;

    LOCK_CONFIG;
    int elementno = INO_ELEMENTNO(ino);
    if (atomic_read(&click_config_generation) != sorted_elements_generation)
	prepare_sorted_elements();
    INODE_INFO(inode).config_generation = sorted_elements_generation;
    
    if (INO_ISHANDLER(ino)) {
	int hi = INO_HANDLERNO(ino);
	if (elementno < 0)
	    hi += Router::FIRST_GLOBAL_HANDLER;
	if (Router::handler_ok(current_router, hi)) {
	    const Router::Handler &h = Router::handler(current_router, hi);
	    inode->i_mode = (h.read_visible() ? proc_click_mode_r : 0) | (h.write_visible() ? proc_click_mode_w : 0);
	    inode->i_uid = inode->i_gid = 0;
	    inode->i_op = 0;
	    inode->i_nlink = (elementno < 0 ? INO_NLINK_GLOBAL_HANDLER : INO_NLINK_LOCAL_HANDLER);
	} else {
	    iput(inode);
	    inode = 0;
	}
    } else if (elementno >= 0 && (!current_router || elementno >= nsorted_elements)) {
	// invalid directory
	iput(inode);
	inode = 0;
    } else {
	inode->i_mode = proc_click_mode_dir;
	inode->i_uid = inode->i_gid = 0;
	inode->i_op = &click_dir_inode_ops;
	// set nlink
	inode->i_nlink = 2;
	if (INO_DIRTYPE(ino) != INO_DT_H) {
	    if (INO_DT_HAS_U(ino) && current_router)
		inode->i_nlink += current_router->nelements();
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

    UNLOCK_CONFIG;
    return inode;
}


// DIRECTORY OPERATIONS

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
    LOCK_CONFIG;
    
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
	if (!current_router || eindex >= current_router->nelements())
	    goto number_failed;
	inode = click_inode(dir->i_sb, INO_MKHDIR(eindex));
	goto found;
    }
    
  number_failed:
    // look for handlers
    if (INO_DT_HAS_H(ino)) {
	int hi = Router::find_handler(current_router, elementno, dentry_name);
	if (hi >= 0) {
	    const Router::Handler &h = Router::handler(current_router, hi);
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
    UNLOCK_CONFIG;
    return reinterpret_cast<struct dentry *>(ERR_PTR(-ENOENT));

  found:
    UNLOCK_CONFIG;
    if (inode) {
	dentry->d_op = &click_dentry_ops;
	d_add(dentry, inode);
	return 0;
    } else
	return reinterpret_cast<struct dentry *>(ERR_PTR(-EINVAL));
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
	Router::element_handlers(current_router, elementno, hi);
	while (f_pos >= RD_HOFF && f_pos < hi.size() + RD_HOFF) {
	    const Router::Handler &h = Router::handler(current_router, hi[f_pos - RD_HOFF]);
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
	int nelem = (current_router ? current_router->nelements() : 0);
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


// SUPERBLOCK OPERATIONS

static struct super_operations click_superblock_ops;

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
click_read_super(struct super_block *s, void * /* data */, int)
{
    lock_super(s);
    
    s->s_blocksize = 1024;
    s->s_blocksize_bits = 10;
    s->s_magic = PROC_SUPER_MAGIC;
    s->s_op = &click_superblock_ops;
    struct inode *root_inode = click_inode(s, INO_GLOBALDIR);
    if (!root_inode)
	goto out_no_root;
    s->s_root = d_alloc_root(root_inode, 0);
    if (!s->s_root)
	goto out_no_root;
    // XXX options
    //parse_options(data, &root_inode->i_uid, &root_inode->i_gid);
    
    unlock_super(s);
    proclikefs_read_super(s);
    return s;

  out_no_root:
    printk("click_read_super: get root inode failed\n");
    iput(root_inode);
    s->s_dev = 0;
    unlock_super(s);
    return 0;
}


static void
click_delete_dentry(struct dentry *dentry)
{
    d_drop(dentry);
}

/*
int proc_statfs(struct super_block *sb, struct statfs *buf, int bufsiz)
{
	struct statfs tmp;

	tmp.f_type = PROC_SUPER_MAGIC;
	tmp.f_bsize = PAGE_SIZE/sizeof(long);
	tmp.f_blocks = 0;
	tmp.f_bfree = 0;
	tmp.f_bavail = 0;
	tmp.f_files = 0;
	tmp.f_ffree = 0;
	tmp.f_namelen = NAME_MAX;
	return copy_to_user(buf, &tmp, bufsiz) ? -EFAULT : 0;
}
*/



void
init_clickfs()
{
    static_assert(sizeof(((struct inode *)0)->u) >= sizeof(ClickInodeInfo));
    
#ifdef LINUX_2_4
    click_dir_ops.owner = THIS_MODULE;
    click_dir_ops.read = generic_read_dir;
#endif
    click_dir_ops.readdir = click_dir_readdir;
    click_dir_inode_ops.lookup = click_dir_lookup;
    click_dir_inode_ops.default_file_ops = &click_dir_ops;

    click_superblock_ops.read_inode = click_read_inode;
    click_superblock_ops.write_inode = click_write_inode;
    click_superblock_ops.put_inode = click_put_inode;
    click_superblock_ops.delete_inode = proclikefs_delete_inode;
    click_superblock_ops.put_super = proclikefs_put_super;

    click_dentry_ops.d_delete = click_delete_dentry;

    sorted_elements_generation = 0;
    
    clickfs = proclikefs_register_filesystem("click", click_read_super, 0);
}

void
cleanup_clickfs()
{
    proclikefs_unregister_filesystem(clickfs);

    free_sorted_elements();
}



#if 0


/*
 * The standard rules, copied from fs/namei.c:permission().
 */
static int standard_permission(struct inode *inode, int mask)
{
	int mode = inode->i_mode;

	if ((mask & S_IWOTH) && IS_RDONLY(inode) &&
	    (S_ISREG(mode) || S_ISDIR(mode) || S_ISLNK(mode)))
		return -EROFS; /* Nobody gets write access to a read-only fs */
	else if ((mask & S_IWOTH) && IS_IMMUTABLE(inode))
		return -EACCES; /* Nobody gets write access to an immutable file */
	else if (current->fsuid == inode->i_uid)
		mode >>= 6;
	else if (in_group_p(inode->i_gid))
		mode >>= 3;
	if (((mode & mask & S_IRWXO) == mask) || capable(CAP_DAC_OVERRIDE))
		return 0;
	/* read and search access */
	if ((mask == S_IROTH) ||
	    (S_ISDIR(inode->i_mode)  && !(mask & ~(S_IROTH | S_IXOTH))))
		if (capable(CAP_DAC_READ_SEARCH))
			return 0;
	return -EACCES;
}

/* 
 * Set up permission rules for processes looking at other processes.
 * You're not allowed to see a process unless it has the same or more
 * restricted root than your own.  This prevents a chrooted processes
 * from escaping through the /proc entries of less restricted
 * processes, and thus allows /proc to be safely mounted in a chrooted
 * area.
 *
 * Note that root (uid 0) doesn't get permission for this either,
 * since chroot is stronger than root.
 *
 * XXX TODO: use the dentry mechanism to make off-limits procs simply
 * invisible rather than denied?  Does each namespace root get its own
 * dentry tree?
 *
 * This also applies the default permissions checks, as it only adds
 * restrictions.
 *
 * Jeremy Fitzhardinge <jeremy@zip.com.au>
 */
int proc_permission(struct inode *inode, int mask)
{
	struct task_struct *p;
	unsigned long ino = inode->i_ino;
	unsigned long pid;
	struct dentry *de, *base;

	if (standard_permission(inode, mask) != 0)
		return -EACCES;

	/* 
	 * Find the root of the processes being examined (if any).
	 * XXX Surely there's a better way of doing this?
	 */
	if (ino >= PROC_OPENPROM_FIRST && 
	    ino <  PROC_OPENPROM_FIRST + PROC_NOPENPROM)
		return 0;		/* already allowed */

	pid = ino >> 16;
	if (pid == 0)
		return 0;		/* already allowed */
	
	de = NULL;
	base = current->fs->root;

	read_lock(&tasklist_lock);
	p = find_task_by_pid(pid);

	if (p && p->fs)
		de = p->fs->root;
	read_unlock(&tasklist_lock);	/* FIXME! */

	if (p == NULL)
		return -EACCES;		/* ENOENT? */

	if (de == NULL)
	{
		/* kswapd and bdflush don't have proper root or cwd... */
		return -EACCES;
	}
	
	/* XXX locking? */
	for(;;)
	{
		struct dentry *parent;

		if (de == base)
			return 0;	/* already allowed */

		de = de->d_covers;
		parent = de->d_parent;

		if (de == parent)
			break;

		de = parent;
	}

	return -EACCES;			/* incompatible roots */
}

#endif

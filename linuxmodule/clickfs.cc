// -*- c-basic-offset: 4 -*-
/*
 * clickfs.cc -- the Click filesystem
 * Eddie Kohler
 *
 * Copyright (c) 2002-2003 International Computer Science Institute
 * Copyright (c) 2004-2010 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <click/config.h>
#include "modulepriv.hh"
#include "proclikefs.h"

#include <click/router.hh>
#include <click/master.hh>
#include <click/straccum.hh>
#include <click/llrpc.h>
#include <click/ino.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/spinlock.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
# include <linux/locks.h>
#endif
#include <linux/proc_fs.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#define CLICKFS_SUPER_MAGIC	0x436C696B /* "Clik" */

static struct file_operations *click_dir_file_ops;
static struct inode_operations *click_dir_inode_ops;
static struct file_operations *click_handler_file_ops;
static struct inode_operations *click_handler_inode_ops;
static struct dentry_operations click_dentry_ops;
static struct proclikefs_file_system *clickfs;

static spinlock_t clickfs_lock;
static wait_queue_head_t clickfs_waitq;
static atomic_t clickfs_read_count;
extern uint32_t click_config_generation;
static int clickfs_ready;

//#define SPIN_LOCK_MSG(l, file, line, what)	printk("<1>%s:%d: pid %d: %sing %p in clickfs\n", (file), (line), current->pid, (what), (l))
#define SPIN_LOCK_MSG(l, file, line, what)	((void)(file), (void)(line))
#define SPIN_LOCK(l, file, line)	do { SPIN_LOCK_MSG((l), (file), (line), "lock"); spin_lock((l)); } while (0)
#define SPIN_UNLOCK(l, file, line)	do { SPIN_LOCK_MSG((l), (file), (line), "unlock"); spin_unlock((l)); } while (0)

#define LOCK_CONFIG_READ()	lock_config(__FILE__, __LINE__, 0)
#define UNLOCK_CONFIG_READ()	unlock_config_read()
#define LOCK_CONFIG_WRITE()	lock_config(__FILE__, __LINE__, 1)
#define UNLOCK_CONFIG_WRITE()	unlock_config_write(__FILE__, __LINE__)


/*************************** Config locking *********************************/

extern struct task_struct *clickfs_task;

static inline void
lock_config(const char *file, int line, int iswrite)
{
    wait_queue_t wait;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# define private linux_private
    init_wait(&wait);
# undef private
#else
    init_waitqueue_entry(&wait, current);
    add_wait_queue(&clickfs_waitq, &wait);
#endif
    for (;;) {
	SPIN_LOCK(&clickfs_lock, file, line);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	prepare_to_wait(&clickfs_waitq, &wait, TASK_UNINTERRUPTIBLE);
#else
	set_current_state(TASK_UNINTERRUPTIBLE);
#endif
	int reads = atomic_read(&clickfs_read_count);
	if (iswrite ? reads == 0 : reads >= 0)
	    break;
	SPIN_UNLOCK(&clickfs_lock, file, line);
	schedule();
    }
    if (iswrite)
	atomic_dec(&clickfs_read_count);
    else
	atomic_inc(&clickfs_read_count);
    SPIN_UNLOCK(&clickfs_lock, file, line);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    finish_wait(&clickfs_waitq, &wait);
#else
    __set_current_state(TASK_RUNNING);
    remove_wait_queue(&clickfs_waitq, &wait);
#endif
    clickfs_task = current;
}

static inline void
unlock_config_read()
{
    assert(atomic_read(&clickfs_read_count) > 0);
    clickfs_task = 0;
    atomic_dec(&clickfs_read_count);
    wake_up(&clickfs_waitq);
}

static inline void
unlock_config_write(const char *file, int line)
{
    assert(atomic_read(&clickfs_read_count) == -1);
    clickfs_task = 0;
    atomic_inc(&clickfs_read_count);
    wake_up_all(&clickfs_waitq);
}


/*************************** Inode constants ********************************/

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
#define INODE_INFO(inode)		(*((ClickInodeInfo *)(&(inode)->i_private)))
#else
#define INODE_INFO(inode)		(*((ClickInodeInfo *)(&(inode)->u)))
#endif

struct ClickInodeInfo {
    uint32_t config_generation;
};

static ClickIno click_ino;

// Must be called with LOCK_CONFIG_READ.
// Return "subdir_error" if this subdirectory was configuration-specific and
// the configuration was changed.  Otherwise, updates this directory's link
// count (if the configuration was changed) and returns 0.

static int
inode_out_of_date(struct inode *inode, int subdir_error)
{
    int error;
    if (INODE_INFO(inode).config_generation != click_config_generation) {
	if (INO_ELEMENTNO(inode->i_ino) >= 0)
	    return subdir_error;
	if ((error = click_ino.prepare(click_router, click_config_generation)) < 0)
	    return error;
	INODE_INFO(inode).config_generation = click_config_generation;
	inode->i_nlink = click_ino.nlink(inode->i_ino);
    }
    return 0;
}


/*************************** Inode operations ********************************/

static struct inode *
click_inode(struct super_block *sb, ino_t ino)
{
    // Must be called with click_config_lock held.

    if (click_ino.prepare(click_router, click_config_generation) < 0)
	return 0;

    struct inode *inode = new_inode(sb);
    if (!inode)
	return 0;

    inode->i_ino = ino;
    INODE_INFO(inode).config_generation = click_config_generation;

    if (INO_ISHANDLER(ino)) {
	int hi = INO_HANDLERNO(ino);
	if (const Handler *h = Router::handler(click_router, hi)) {
	    inode->i_mode = S_IFREG | (h->read_visible() ? click_fsmode.read : 0) | (h->write_visible() ? click_fsmode.write : 0);
	    inode->i_uid = click_fsmode.uid;
	    inode->i_gid = click_fsmode.gid;
	    inode->i_op = click_handler_inode_ops;
	    inode->i_fop = click_handler_file_ops;
	    inode->i_nlink = click_ino.nlink(ino);
	} else {
	    // can't happen
	    iput(inode);
	    inode = 0;
	    panic("click_inode");
	}
    } else {
	inode->i_mode = click_fsmode.dir;
	inode->i_uid = click_fsmode.uid;
	inode->i_gid = click_fsmode.gid;
	inode->i_op = click_dir_inode_ops;
	inode->i_fop = click_dir_file_ops;
	inode->i_nlink = click_ino.nlink(ino);
    }

    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;

    MDEBUG("%lx:%p:%p: leaving click_inode", ino, inode, inode->i_op);
    return inode;
}



/*************************** Directory operations ****************************/

extern "C" {

static struct dentry *
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
click_dir_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *)
#else
click_dir_lookup(struct inode *dir, struct dentry *dentry)
#endif
{
    LOCK_CONFIG_READ();
    MDEBUG("click_dir_lookup %lx", dir->i_ino);

    struct inode *inode = 0;
    int error;
    if ((error = inode_out_of_date(dir, -EIO)) >= 0) {
	// BEWARE!  Using stable_string() here is quite dangerous, since the
	// data is actually mutable.  The code path has been audited to make
	// sure this is OK.
	String dentry_name = String::make_stable(reinterpret_cast<const char *>(dentry->d_name.name), dentry->d_name.len);
	if (ino_t new_ino = click_ino.lookup(dir->i_ino, dentry_name))
	    inode = click_inode(dir->i_sb, new_ino);
	else
	    error = -ENOENT;
    }

    UNLOCK_CONFIG_READ();
    if (error < 0)
	return reinterpret_cast<struct dentry *>(ERR_PTR(error));
    else if (!inode)
	// couldn't get an inode
	return reinterpret_cast<struct dentry *>(ERR_PTR(-EINVAL));
    else {
	dentry->d_op = &click_dentry_ops;
	d_add(dentry, inode);
	return 0;
    }
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static int
click_dir_revalidate(struct dentry *dentry)
{
    struct inode *inode = dentry->d_inode;
    MDEBUG("click_dir_revalidate %lx", (inode ? inode->i_ino : 0));
    if (!inode)
	return -EINVAL;

    int error = 0;
    LOCK_CONFIG_READ();
    if (INODE_INFO(inode).config_generation != click_config_generation) {
	if (INO_ELEMENTNO(inode->i_ino) >= 0) // not a global directory
	    error = -EIO;
	else if ((error = click_ino.prepare(click_router, click_config_generation)) < 0)
	    /* preserve error */;
	else {
	    INODE_INFO(inode).config_generation = click_config_generation;
	    inode->i_nlink = click_ino.nlink(inode->i_ino);
	}
    }
    UNLOCK_CONFIG_READ();
    return error;
}
#endif


struct my_filldir_container {
    filldir_t filldir;
    void *dirent;
};

static bool
my_filldir(const char *name, int namelen, ino_t ino, int dirtype, uint32_t f_pos, void *thunk)
{
    my_filldir_container *mfd = (my_filldir_container *)thunk;
    int error = mfd->filldir(mfd->dirent, name, namelen, f_pos, ino, dirtype);
    return error >= 0;
}

static int
click_dir_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
    struct my_filldir_container mfd;
    mfd.filldir = filldir;
    mfd.dirent = dirent;

    struct inode *inode = filp->f_dentry->d_inode;
    ino_t ino = inode->i_ino;
    uint32_t f_pos = filp->f_pos;
    MDEBUG("click_dir_readdir %lx", ino);

    LOCK_CONFIG_READ();

    int error = inode_out_of_date(inode, -ENOENT);
    int stored = 0;
    if (error < 0)
	goto done;

    // global '..'
    if (ino == INO_GLOBALDIR && f_pos == 0) {
	if (!my_filldir("..", 2, filp->f_dentry->d_parent->d_inode->i_ino, f_pos, DT_DIR, &mfd))
	    goto done;
	f_pos++;
	stored++;
    }

    // real entries
    stored += click_ino.readdir(ino, f_pos, my_filldir, &mfd);

  done:
    UNLOCK_CONFIG_READ();
    filp->f_pos = f_pos;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    return (error ? error : stored);
#else
    return error;
#endif
}

} // extern "C"


/*************************** Superblock operations ***************************/

static struct super_operations click_superblock_ops;

extern "C" {

static struct super_block *
click_read_super(struct super_block *sb, void * /* data */, int)
{
    struct inode *root_inode = 0;
    if (!clickfs_ready)
	goto out_no_root;
    MDEBUG("click_read_super");
    sb->s_blocksize = 1024;
    sb->s_blocksize_bits = 10;
    sb->s_magic = CLICKFS_SUPER_MAGIC;
    sb->s_op = &click_superblock_ops;
    MDEBUG("click_config_lock");
    LOCK_CONFIG_READ();
    root_inode = click_inode(sb, INO_GLOBALDIR);
    UNLOCK_CONFIG_READ();
    if (!root_inode)
	goto out_no_root;
    sb->s_root = d_alloc_root(root_inode);
    MDEBUG("got root inode %p:%p", root_inode, root_inode->i_op);
    if (!sb->s_root)
	goto out_no_root;
    // XXX options

    MDEBUG("got root directory");
    proclikefs_read_super(sb);
    MDEBUG("done click_read_super");
    return sb;

  out_no_root:
    printk("<1>click_read_super: get root inode failed\n");
    iput(root_inode);
    sb->s_dev = 0;
    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static int
click_fill_super(struct super_block *sb, void *data, int flags)
{
    return click_read_super(sb, data, flags) ? 0 : -ENOMEM;
}

# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
static struct dentry *
click_get_sb(struct file_system_type *fs_type, int flags, const char *, void *data)
{
    return mount_single(fs_type, flags, data, click_fill_super);
}
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
static int
click_get_sb(struct file_system_type *fs_type, int flags, const char *, void *data, struct vfsmount *vfsmount)
{
    return get_sb_single(fs_type, flags, data, click_fill_super, vfsmount);
}
# else
static struct super_block *
click_get_sb(struct file_system_type *fs_type, int flags, const char *, void *data)
{
    return get_sb_single(fs_type, flags, data, click_fill_super);
}
# endif
#endif

static void
click_reread_super(struct super_block *sb)
{
    lock_super(sb);
    if (sb->s_root) {
	struct inode *old_inode = sb->s_root->d_inode;
	LOCK_CONFIG_READ();
	sb->s_root->d_inode = click_inode(sb, INO_GLOBALDIR);
	UNLOCK_CONFIG_READ();
	iput(old_inode);
	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;
	sb->s_op = &click_superblock_ops;
    } else
	printk("<1>silly click_reread_super\n");
    unlock_super(sb);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
static int
click_delete_dentry(const struct dentry *)
{
    return 1;
}
#else
static int
click_delete_dentry(struct dentry *)
{
    return 1;
}
#endif

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

#define FILP_STRINGNO(filp)		(reinterpret_cast<intptr_t>((filp)->private_data))
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
next_handler_string(const Handler *h)
{
    SPIN_LOCK(&handler_strings_lock, __FILE__, __LINE__);
    if (handler_strings_free < 0)
	increase_handler_strings();
    int hs = handler_strings_free;
    if (hs >= 0) {
	handler_strings_free = handler_strings_info[hs].next;
	handler_strings_info[hs].flags = h->flags();
    }
    SPIN_UNLOCK(&handler_strings_lock, __FILE__, __LINE__);
    return hs;
}

static void
free_handler_string(int hs)
{
    SPIN_LOCK(&handler_strings_lock, __FILE__, __LINE__);
    if (hs >= 0 && hs < handler_strings_cap) {
	handler_strings[hs] = String();
	handler_strings_info[hs].next = handler_strings_free;
	handler_strings_free = hs;
    }
    SPIN_UNLOCK(&handler_strings_lock, __FILE__, __LINE__);
}

static void
lock_threads()
{
    click_master->block_all();
}

static void
unlock_threads()
{
    click_master->unblock_all();
}


namespace {
class ClickfsHandlerErrorHandler : public ErrorHandler { public:
    ClickfsHandlerErrorHandler() {
    }
    void *emit(const String &str, void *, bool) {
	_sa << str << '\n';
	return 0;
    }
    StringAccum _sa;
};
}


extern "C" {

static int
handler_open(struct inode *inode, struct file *filp)
{
    LOCK_CONFIG_READ();

    bool reading = (filp->f_flags & O_ACCMODE) != O_WRONLY;
    bool writing = (filp->f_flags & O_ACCMODE) != O_RDONLY;

    int retval = 0;
    int stringno = -1;
    const Handler *h;

    if ((reading && writing)
	|| (filp->f_flags & O_APPEND)
	|| (writing && !(filp->f_flags & O_TRUNC)))
	retval = -EACCES;
    else if ((retval = inode_out_of_date(inode, -EIO)) < 0)
	/* save retval */;
    else if (!(h = Router::handler(click_router, INO_HANDLERNO(inode->i_ino))))
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

    UNLOCK_CONFIG_READ();

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
    if ((handler_strings_info[stringno].flags & (HANDLER_DIRECT | HANDLER_DONE)) != HANDLER_DONE) {
	LOCK_CONFIG_READ();
	int retval;
	const Handler *h;
	struct inode *inode = filp->f_dentry->d_inode;
	if ((retval = inode_out_of_date(inode, -EIO)) < 0)
	    /* save retval */;
	else if (!(h = Router::handler(click_router, INO_HANDLERNO(inode->i_ino))))
	    retval = -EIO;
	else if (!h->read_visible())
	    retval = -EPERM;
	else {
	    int eindex = INO_ELEMENTNO(inode->i_ino);
	    Element *e = Router::element(click_router, eindex);

	    if (handler_strings_info[stringno].flags & HANDLER_DIRECT) {
		click_handler_direct_info hdi;
		hdi.buffer = buffer;
		hdi.count = count;
		hdi.store_f_pos = store_f_pos;
		hdi.string = &handler_strings[stringno];
		hdi.retval = 0;
		(void) h->__call_read(e, &hdi);
		count = hdi.count;
		retval = hdi.retval;
	    } else if (h->exclusive()) {
		lock_threads();
		handler_strings[stringno] = h->call_read(e);
		unlock_threads();
	    } else
		handler_strings[stringno] = h->call_read(e);

	    if (!h->raw()
		&& !(handler_strings_info[stringno].flags & HANDLER_RAW)
		&& !(handler_strings_info[stringno].flags & HANDLER_DIRECT)
		&& handler_strings[stringno]
		&& handler_strings[stringno].back() != '\n')
		handler_strings[stringno] += '\n';
	    retval = (handler_strings[stringno].out_of_memory() ? -ENOMEM : 0);
	}
	UNLOCK_CONFIG_READ();
	if (retval < 0)
	    return retval;
	handler_strings_info[stringno].flags |= HANDLER_DONE;
    }

    if (!(handler_strings_info[stringno].flags & HANDLER_DIRECT)) {
	const String &s = handler_strings[stringno];
	if (f_pos > s.length())
	    f_pos = s.length();
	if (f_pos + count > s.length())
	    count = s.length() - f_pos;
	if (copy_to_user(buffer, s.data() + f_pos, count) > 0)
	    return -EFAULT;
    }

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

    handler_strings_info[stringno].flags &= ~HANDLER_DONE;
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
handler_do_write(struct file *filp, void *address_ptr)
{
    int stringno = FILP_WRITE_STRINGNO(filp);
    struct inode *inode = filp->f_dentry->d_inode;
    const Handler *h;
    int retval;

    if ((retval = inode_out_of_date(inode, -EIO)) < 0)
	/* save retval */;
    else if (!(h = Router::handler(click_router, INO_HANDLERNO(inode->i_ino)))
	     || !h->write_visible())
	retval = -EIO;
    else if (handler_strings[stringno].out_of_memory())
	retval = -ENOMEM;
    else {
	int eindex = INO_ELEMENTNO(inode->i_ino);
	Element *e = Router::element(click_router, eindex);
	click_llrpc_call_handler_st chs;
	chs.flags = 0;
	int r;

	if (address_ptr
	    && (r = CLICK_LLRPC_GET_DATA(&chs, address_ptr, sizeof(chs))) < 0) {
	    retval = r;
	    goto exit;
	}

	String data = handler_strings[stringno];
	if (!h->raw()
	    && !(handler_strings_info[stringno].flags & HANDLER_RAW)
	    && (!address_ptr || !(chs.flags & CLICK_LLRPC_CALL_HANDLER_FLAG_RAW))
	    && data
	    && data.back() == '\n')
	    data = data.substring(data.begin(), data.end() - 1);

	ClickfsHandlerErrorHandler cerrh;
	if (h->exclusive()) {
	    lock_threads();
	    retval = h->call_write(data, e, &cerrh);
	    unlock_threads();
	} else
	    retval = h->call_write(data, e, &cerrh);

	handler_strings_info[stringno].flags |= HANDLER_DONE;

	if (cerrh._sa && !address_ptr) {
	    ErrorHandler *errh = click_logged_errh;
	    if (e)
		errh->message("In write handler '%s' for '%s':", h->name().c_str(), e->declaration().c_str());
	    else
		errh->message("In write handler '%s':", h->name().c_str());
	    String str = cerrh._sa.take_string();
	    const char *s = str.begin(), *end = str.end();
	    while (s != end) {
		const char *nl = find(s, end, '\n');
		errh->xmessage(ErrorHandler::combine_anno(str.substring(s, nl), "  "));
		s = nl + (nl != end);
	    }
	}
	if (address_ptr && chs.errorlen > 0) {
	    String str = cerrh._sa.take_string();
	    const char *s = str.begin(), *end = str.end();
	    while (s != end) {
		String landmark;
		s = ErrorHandler::parse_anno(str, s, end, "l", &landmark,
					     (const char *) 0);
		const char *nl = find(s, end, '\n');
		cerrh._sa << ErrorHandler::clean_landmark(landmark, true)
			  << str.substring(s, nl) << '\n';
		s = nl + (nl != end);
	    }
	    size_t len = cerrh._sa.length();
	    if (len > chs.errorlen)
		len = chs.errorlen;
	    chs.errorlen = cerrh._sa.length();
	    if (chs.errorlen > 0
		&& (r = CLICK_LLRPC_PUT_DATA(chs.errorbuf, cerrh._sa.data(), len)) < 0) {
		retval = r;
		goto exit;
	    }
	    if ((r = CLICK_LLRPC_PUT_DATA(address_ptr, &chs, sizeof(chs))) < 0) {
		retval = r;
		goto exit;
	    }
	}
    }

  exit:
    return retval;
}

static int
handler_flush(struct file *filp
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
	      , struct files_struct *files
#endif
	      )
{
    bool writing = (filp->f_flags & O_ACCMODE) != O_RDONLY;
    int stringno = FILP_WRITE_STRINGNO(filp);
    int retval = 0;

#ifdef file_count
    long f_count = file_count(filp);
#else
    int f_count = atomic_read(&filp->f_count);
#endif

    if (writing && f_count == 1
	&& stringno >= 0 && stringno < handler_strings_cap
	&& !(handler_strings_info[stringno].flags & HANDLER_DONE)) {
	LOCK_CONFIG_WRITE();
	retval = handler_do_write(filp, 0);
	UNLOCK_CONFIG_WRITE();
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

static inline int
do_handler_ioctl(struct inode *inode, struct file *filp,
		 unsigned command, unsigned long address)
{
    if (command & _CLICK_IOC_SAFE)
	LOCK_CONFIG_READ();
    else
	LOCK_CONFIG_WRITE();

    int retval;
    Element *e;

    if ((retval = inode_out_of_date(inode, -EIO)) < 0)
	/* save retval */;
    else if (!click_router)
	retval = -EINVAL;
    else if (command == CLICK_LLRPC_CALL_HANDLER)
	retval = handler_do_write(filp, reinterpret_cast<void *>(address));
    else if (command == CLICK_LLRPC_ABANDON_HANDLER) {
	int stringno = FILP_STRINGNO(filp);
	handler_strings_info[stringno].flags |= HANDLER_DONE;
	handler_strings[stringno] = String();
	retval = 0;
    } else if (command == CLICK_LLRPC_RAW_HANDLER) {
	int stringno = FILP_STRINGNO(filp);
	handler_strings_info[stringno].flags |= HANDLER_RAW;
	retval = 0;
    } else if (INO_ELEMENTNO(inode->i_ino) < 0
	     || !(e = click_router->element(INO_ELEMENTNO(inode->i_ino))))
	retval = -EIO;
    else {
	union {
	    char buf[128];
	    long align;
	} ubuf;
	char *data;
	void *address_ptr, *arg_ptr;

	// allocate ioctl buffer
	int size = _CLICK_IOC_SIZE(command);
	if (size <= 128)
	    data = ubuf.buf;
	else if (size > 16384 || !(data = new char[size])) {
	    retval = -ENOMEM;
	    goto exit;
	}

	// fetch incoming data if necessary
	address_ptr = reinterpret_cast<void *>(address);
	if (size && (command & _CLICK_IOC_IN)
	    && (retval = CLICK_LLRPC_GET_DATA(data, address_ptr, size)) < 0)
	    goto free_exit;

	// call llrpc
	arg_ptr = (size && (command & (_CLICK_IOC_IN | _CLICK_IOC_OUT)) ? data : address_ptr);
	if (click_router->initialized())
	    retval = e->llrpc(command, arg_ptr);
	else
	    retval = e->Element::llrpc(command, arg_ptr);

	// store outgoing data if necessary
	if (retval >= 0 && size && (command & _CLICK_IOC_OUT))
	    retval = CLICK_LLRPC_PUT_DATA(address_ptr, data, size);

      free_exit:
	if (data != ubuf.buf)
	    delete[] data;
    }

  exit:
    if (command & _CLICK_IOC_SAFE)
	UNLOCK_CONFIG_READ();
    else
	UNLOCK_CONFIG_WRITE();
    return retval;
}

#if HAVE_UNLOCKED_IOCTL
static long
handler_unlocked_ioctl(struct file *filp,
		       unsigned command, unsigned long address)
{
    return do_handler_ioctl(filp->f_dentry->d_inode, filp, command, address);
}
#else
static int
handler_ioctl(struct inode *inode, struct file *filp,
	      unsigned command, unsigned long address)
{
    return do_handler_ioctl(inode, filp, command, address);
}
#endif

#if INO_DEBUG
static String
read_ino_info(Element *, void *)
{
    return click_ino.info();
}
#endif

} // extern "C"


/*********************** Initialization and termination **********************/

struct file_operations *
click_new_file_operations()
{
    if (!clickfs) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	clickfs = proclikefs_register_filesystem("click", 0, click_get_sb);
#else
	// NB: remove FS_SINGLE if it will ever make sense to have different
	// Click superblocks -- if we introduce mount options, for example
	clickfs = proclikefs_register_filesystem("click", FS_SINGLE, click_read_super);
#endif
    }
    if (clickfs)
	return proclikefs_new_file_operations(clickfs);
    else
	return 0;
}

int
init_clickfs()
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
    static_assert(sizeof(((struct inode *)0)->u) >= sizeof(ClickInodeInfo), "The file-system-specific data in struct inode isn't big enough.");
#endif
    static_assert(HANDLER_DIRECT + HANDLER_DONE + HANDLER_RAW + HANDLER_SPECIAL_INODE + HANDLER_WRITE_UNLIMITED < Handler::USER_FLAG_0, "Too few driver handler flags available.");

    spin_lock_init(&handler_strings_lock);
    spin_lock_init(&clickfs_lock);
    init_waitqueue_head(&clickfs_waitq);
    atomic_set(&clickfs_read_count, 0);

    // clickfs creation moved to click_new_file_operations()
    if (!(click_dir_file_ops = click_new_file_operations())
	|| !(click_dir_inode_ops = proclikefs_new_inode_operations(clickfs))
	|| !(click_handler_file_ops = click_new_file_operations())
	|| !(click_handler_inode_ops = proclikefs_new_inode_operations(clickfs))) {
	printk("<1>click: could not initialize clickfs!\n");
	return -EINVAL;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
    click_superblock_ops.put_inode = force_delete;
#endif
    click_superblock_ops.put_super = proclikefs_put_super;
    // XXX statfs

    click_dentry_ops.d_delete = click_delete_dentry;

    click_dir_file_ops->read = generic_read_dir;
    click_dir_file_ops->readdir = click_dir_readdir;
    click_dir_inode_ops->lookup = click_dir_lookup;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
    click_dir_inode_ops->revalidate = click_dir_revalidate;
#endif

    click_handler_file_ops->read = handler_read;
    click_handler_file_ops->write = handler_write;
#if HAVE_UNLOCKED_IOCTL
    click_handler_file_ops->unlocked_ioctl = handler_unlocked_ioctl;
#else
    click_handler_file_ops->ioctl = handler_ioctl;
#endif
    click_handler_file_ops->open = handler_open;
    click_handler_file_ops->flush = handler_flush;
    click_handler_file_ops->release = handler_release;

    click_ino.initialize();

    proclikefs_reinitialize_supers(clickfs, click_reread_super);
    clickfs_ready = 1;

    // initialize a symlink from /proc/click -> /click, to ease transition
    (void) proc_symlink("click", 0, "/click");

#if INO_DEBUG
    Router::add_read_handler(0, "ino_info", read_ino_info, 0);
#endif

    return 0;
}

void
cleanup_clickfs()
{
    MDEBUG("cleanup_clickfs");
    // remove the '/proc/click' directory
    remove_proc_entry("click", 0);

    // kill filesystem
    MDEBUG("proclikefs_unregister_filesystem");
    clickfs_ready = 0;
    if (clickfs)
	proclikefs_unregister_filesystem(clickfs);

    // clean up handler_strings
    MDEBUG("cleaning up handler strings");
    SPIN_LOCK(&handler_strings_lock, __FILE__, __LINE__);
    delete[] handler_strings;
    delete[] handler_strings_info;
    handler_strings = 0;
    handler_strings_info = 0;
    handler_strings_cap = -1;
    handler_strings_free = -1;
    SPIN_UNLOCK(&handler_strings_lock, __FILE__, __LINE__);

    MDEBUG("click_ino cleanup");
    click_ino.cleanup();
}

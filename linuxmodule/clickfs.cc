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

#define USE_PROCLIKEFS 1

#include <click/config.h>
#include "modulepriv.hh"
#if USE_PROCLIKEFS
# include "proclikefs.h"
#endif

#include <click/router.hh>
#include <click/error.hh>
#include <click/llrpc.h>
#include <click/ino.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/spinlock.h>
#include <linux/locks.h>
#include <linux/proc_fs.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#define CLICKFS_SUPER_MAGIC	0x436C696B /* "Clik" */

static struct file_operations click_dir_file_ops;
static struct inode_operations click_dir_inode_ops;
static struct file_operations click_handler_file_ops;
static struct inode_operations click_handler_inode_ops;
static struct dentry_operations click_dentry_ops;
#if USE_PROCLIKEFS
static struct proclikefs_file_system *clickfs;
#else
static struct file_system_type *clickfs;
#endif

static spinlock_t config_write_lock;
static atomic_t config_read_count;
extern uint32_t click_config_generation;


/*************************** Config locking *********************************/

static inline void
lock_config_read()
{
    while (!spin_trylock(&config_write_lock))
	schedule();
    atomic_inc(&config_read_count);
    spin_unlock(&config_write_lock);
}

static inline void
unlock_config_read()
{
    atomic_dec(&config_read_count);
}

static inline void
lock_config_write()
{
    while (1) {
	while (!spin_trylock(&config_write_lock))
	    schedule();
	if (atomic_read(&config_read_count) == 0)
	    return;
	spin_unlock(&config_write_lock);
	schedule();
    }
}

static inline void
unlock_config_write()
{
    spin_unlock(&config_write_lock);
}


/*************************** Inode constants ********************************/

#define INODE_INFO(inode)		(*((ClickInodeInfo *)(&(inode)->u)))

struct ClickInodeInfo {
#if USE_PROCLIKEFS
    struct proclikefs_inode_info padding;
#endif
    uint32_t config_generation;
};

inline bool
inode_out_of_date(struct inode *inode)
{
    return INO_ELEMENTNO(inode->i_ino) >= 0
	&& INODE_INFO(inode).config_generation != click_config_generation;
}

static ClickIno click_ino;


/*************************** Inode operations ********************************/

#ifdef LINUX_2_2
// borrowed from Linux 2.4
static inline struct inode *
new_inode(struct super_block *sb)
{
    struct inode *inode = get_empty_inode();
    if (inode) {
	inode->i_sb = sb;
	inode->i_dev = sb->s_dev;
    }
    return inode;
}
#endif

static struct inode *
click_inode(struct super_block *sb, ino_t ino)
{
    // Must be called with click_config_lock held.

    if (click_ino.prepare(click_router, click_config_generation) < 0)
	return 0;
    
    MDEBUG("i_get");
    struct inode *inode = new_inode(sb);
    if (!inode)
	return 0;

    inode->i_ino = ino;
    INODE_INFO(inode).config_generation = click_config_generation;

    if (INO_ISHANDLER(ino)) {
	int hi = INO_HANDLERNO(ino);
	if (Router::handler_ok(click_router, hi)) {
	    const Router::Handler &h = Router::handler(click_router, hi);
	    inode->i_mode = S_IFREG | (h.read_visible() ? click_mode_r : 0) | (h.write_visible() ? click_mode_w : 0);
	    inode->i_uid = inode->i_gid = 0;
	    inode->i_op = &click_handler_inode_ops;
#ifdef LINUX_2_4
	    inode->i_fop = &click_handler_file_ops;
#endif
	    inode->i_nlink = click_ino.nlink(ino);
	} else {
	    // can't happen
	    iput(inode);
	    inode = 0;
	    panic("click_inode");
	}
    } else {
	MDEBUG("got a directory");
	inode->i_mode = click_mode_dir;
	inode->i_uid = inode->i_gid = 0;
	inode->i_op = &click_dir_inode_ops;
#ifdef LINUX_2_4
	inode->i_fop = &click_dir_file_ops;
#endif
	inode->i_nlink = click_ino.nlink(ino);
    }

    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
#if USE_PROCLIKEFS
    proclikefs_read_inode(inode);
#endif
	
    MDEBUG("%lx:%p:%p: leaving click_inode", ino, inode, inode->i_op);
    return inode;
}



/*************************** Directory operations ****************************/

extern "C" {

static struct dentry *
click_dir_lookup(struct inode *dir, struct dentry *dentry)
{
    lock_config_read();
    MDEBUG("click_dir_lookup %lx", dir->i_ino);

    struct inode *inode = 0;
    int error;
    if (inode_out_of_date(dir))
	error = -EIO;
    else if ((error = click_ino.prepare(click_router, click_config_generation)) < 0)
	/* save error */;
    else {
	String dentry_name = String::stable_string(reinterpret_cast<const char *>(dentry->d_name.name), dentry->d_name.len);
	if (ino_t new_ino = click_ino.lookup(dir->i_ino, dentry_name))
	    inode = click_inode(dir->i_sb, new_ino);
	else
	    error = -ENOENT;
    }

    unlock_config_read();
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

static int
click_dir_revalidate(struct dentry *dentry)
{
    struct inode *inode = dentry->d_inode;
    MDEBUG("click_dir_revalidate %lx", (inode ? inode->i_ino : 0));
    if (!inode)
	return -EINVAL;
    
    int error = 0;
    lock_config_read();
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
    unlock_config_read();
    return error;
}


struct my_filldir_container {
    filldir_t filldir;
    void *dirent;
};

static bool
my_filldir(const char *name, int namelen, ino_t ino, int dirtype, uint32_t f_pos, void *thunk)
{
    my_filldir_container *mfd = (my_filldir_container *)thunk;
#ifdef LINUX_2_2
    (void)dirtype;
    int error = mfd->filldir(mfd->dirent, name, namelen, f_pos, ino);
#else
    int error = mfd->filldir(mfd->dirent, name, namelen, f_pos, ino, dirtype);
#endif
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

    lock_config_read();

    int error;
    if (inode_out_of_date(inode))
	error = -ENOENT;
    else
	error = click_ino.prepare(click_router, click_config_generation);

    // '.' and '..'
    if (error >= 0 && f_pos == 0) {
	if (my_filldir(".", 1, ino, f_pos, DT_DIR, &mfd))
	    f_pos++;
	else
	    error = -1;
    }
    if (error >= 0 && f_pos == 1) {
	if (my_filldir("..", 2, filp->f_dentry->d_parent->d_inode->i_ino, f_pos, DT_DIR, &mfd))
	    f_pos++;
	else
	    error = -1;
    }

    // real entries
    if (error >= 0)
	error = click_ino.readdir(ino, f_pos, my_filldir, &mfd);

    unlock_config_read();
    filp->f_pos = f_pos;
    return (error == -1 ? 0 : error);
}

} // extern "C"


/*************************** Superblock operations ***************************/

static struct super_operations click_superblock_ops;

extern "C" {

#ifdef LINUX_2_2
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
#endif

static struct super_block *
click_read_super(struct super_block *sb, void * /* data */, int)
{
    MDEBUG("click_read_super");
    sb->s_blocksize = 1024;
    sb->s_blocksize_bits = 10;
    sb->s_magic = CLICKFS_SUPER_MAGIC;
    sb->s_op = &click_superblock_ops;
    MDEBUG("click_config_lock");
    lock_config_read();
    struct inode *root_inode = click_inode(sb, INO_GLOBALDIR);
    unlock_config_read();
    if (!root_inode)
	goto out_no_root;
#ifdef LINUX_2_4
    sb->s_root = d_alloc_root(root_inode);
#else
    sb->s_root = d_alloc_root(root_inode, 0);
#endif
    MDEBUG("got root inode %p:%p", root_inode, root_inode->i_op);
    if (!sb->s_root)
	goto out_no_root;
    // XXX options

    MDEBUG("got root directory");
#if USE_PROCLIKEFS
    proclikefs_read_super(sb);
#endif
    MDEBUG("done click_read_super");
    return sb;

  out_no_root:
    printk("<1>click_read_super: get root inode failed\n");
    iput(root_inode);
    sb->s_dev = 0;
    return 0;
}

static void
click_reread_super(struct super_block *sb)
{
    lock_super(sb);
    if (sb->s_root) {
	struct inode *old_inode = sb->s_root->d_inode;
	lock_config_read();
	sb->s_root->d_inode = click_inode(sb, INO_GLOBALDIR);
	unlock_config_read();
	iput(old_inode);
	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;
	sb->s_op = &click_superblock_ops;
    } else
	printk("<1>silly click_reread_super\n");
    unlock_super(sb);
}

#ifdef LINUX_2_4
static int
click_delete_dentry(struct dentry *)
{
    return 1;
}
#else
static void
click_delete_dentry(struct dentry *dentry)
{
    d_drop(dentry);
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

extern "C" {

static int
handler_open(struct inode *inode, struct file *filp)
{
    lock_config_read();

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
    else if (!(h = Router::handlerp(click_router, INO_HANDLERNO(inode->i_ino))))
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

    unlock_config_read();
    
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
	lock_config_read();
	int retval;
	const Router::Handler *h;
	struct inode *inode = filp->f_dentry->d_inode;
	if (inode_out_of_date(inode)
	    || !(h = Router::handlerp(click_router, INO_HANDLERNO(inode->i_ino))))
	    retval = -EIO;
	else if (!h->read_visible())
	    retval = -EPERM;
	else {
	    int eindex = INO_ELEMENTNO(inode->i_ino);
	    Element *e = (eindex >= 0 ? click_router->element(eindex) : 0);
	    handler_strings[stringno] = h->call_read(e);
	    retval = (handler_strings[stringno].out_of_memory() ? -ENOMEM : 0);
	}
	unlock_config_read();
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
	lock_config_write();
	
	struct inode *inode = filp->f_dentry->d_inode;
	const Router::Handler *h;
	
	if (inode_out_of_date(inode)
	    || !(h = Router::handlerp(click_router, INO_HANDLERNO(inode->i_ino)))
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
	
	unlock_config_write();
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
    if (command & _CLICK_IOC_SAFE)
	lock_config_read();
    else
	lock_config_write();

    int retval;
    Element *e;
    
    if (inode_out_of_date(inode))
	retval = -EIO;
    else if (!click_router)
	retval = -EINVAL;
    else if (!(e = click_router->element(INO_ELEMENTNO(inode->i_ino))))
	retval = -EIO;
    else {
	union {
	    char buf[128];
	    long align;
	} ubuf;
	char *data;
	void *address_ptr, *arg_ptr;

	// allocate ioctl buffer
	if (_IOC_SIZE(command) <= 128)
	    data = ubuf.buf;
	else if (_IOC_SIZE(command) > 16384 || !(data = new char[_IOC_SIZE(command)])) {
	    retval = -ENOMEM;
	    goto exit;
	}

	// fetch incoming data if necessary
	address_ptr = reinterpret_cast<void *>(address);
	if (_IOC_SIZE(command) && (command & _CLICK_IOC_IN)
	    && (retval = CLICK_LLRPC_GET_DATA(data, address_ptr, _IOC_SIZE(command))) < 0)
	    goto free_exit;

	// call llrpc
	arg_ptr = (_IOC_SIZE(command) && (command & (_CLICK_IOC_IN | _CLICK_IOC_OUT)) ? data : address_ptr);
	if (click_router->initialized())
	    retval = e->llrpc(command, arg_ptr);
	else
	    retval = e->Element::llrpc(command, arg_ptr);

	// store outgoing data if necessary
	if (retval >= 0 && _IOC_SIZE(command) && (command & _CLICK_IOC_OUT))
	    retval = CLICK_LLRPC_PUT_DATA(address_ptr, data, _IOC_SIZE(command));

      free_exit:
	if (data != ubuf.buf)
	    delete[] data;
    }

  exit:
    if (command & _CLICK_IOC_SAFE)
	unlock_config_read();
    else
	unlock_config_write();
    return retval;
}

#ifdef LINUX_2_2
static int
proc_click_readlink_proc(proc_dir_entry *, char *page)
{
    strcpy(page, "/click");
    return 6;
}
#endif

} // extern "C"


/*********************** Initialization and termination **********************/

int
init_clickfs()
{
    static_assert(sizeof(((struct inode *)0)->u) >= sizeof(ClickInodeInfo));
    
#ifdef LINUX_2_4
    click_superblock_ops.put_inode = force_delete;
#else
    click_superblock_ops.write_inode = click_write_inode;
    click_superblock_ops.put_inode = click_put_inode;
#endif
#if USE_PROCLIKEFS
    click_superblock_ops.delete_inode = proclikefs_delete_inode;
    click_superblock_ops.put_super = proclikefs_put_super;
#endif
    // XXX statfs

    click_dentry_ops.d_delete = click_delete_dentry;

#ifdef LINUX_2_4
    click_dir_file_ops.owner = THIS_MODULE;
    click_dir_file_ops.read = generic_read_dir;
#endif
    click_dir_file_ops.readdir = click_dir_readdir;
    click_dir_inode_ops.lookup = click_dir_lookup;
    click_dir_inode_ops.revalidate = click_dir_revalidate;
#ifdef LINUX_2_2
    click_dir_inode_ops.default_file_ops = &click_dir_file_ops;
#endif

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
    spin_lock_init(&config_write_lock);
    atomic_set(&config_read_count, 0);
    click_ino.initialize();

#if USE_PROCLIKEFS
    clickfs = proclikefs_register_filesystem("click", click_read_super, click_reread_super);
#else
    clickfs = new file_system_type;
    clickfs->name = "click";
    clickfs->next = 0;
    clickfs->read_super = click_read_super;
    clickfs->fs_flags = 0;
    clickfs->owner = THIS_MODULE;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
    INIT_LIST_HEAD(&clickfs->fs_supers);
# endif
    int err = register_filesystem(clickfs);
    if (err < 0)
	printk("<1>click: registering fs[%p]: error %d\n", clickfs, -err);
#endif
    if (!clickfs) {
	printk("<1>click: could not initialize clickfs!\n");
	return -EINVAL;
    }

    // initialize a symlink from /proc/click -> /click, to ease transition
#ifdef LINUX_2_4
    (void) proc_symlink("click", 0, "/click");
#elif defined(LINUX_2_2)
    if (proc_dir_entry *link = create_proc_entry("click", S_IFLNK | S_IRUGO | S_IWUGO | S_IXUGO, 0))
	link->readlink_proc = proc_click_readlink_proc;
#endif
    
    return 0;
}

void
cleanup_clickfs()
{
#if defined(LINUX_2_4) || defined(LINUX_2_2)
    // remove the `/proc/click' directory
    remove_proc_entry("click", 0);
#endif

    // kill filesystem
#if USE_PROCLIKEFS
    proclikefs_unregister_filesystem(clickfs);
#else
    unregister_filesystem(clickfs);
    delete clickfs;
#endif

    // clean up handler_strings
    spin_lock(&handler_strings_lock);
    delete[] handler_strings;
    delete[] handler_strings_info;
    handler_strings = 0;
    handler_strings_info = 0;
    handler_strings_cap = -1;
    handler_strings_free = -1;
    spin_unlock(&handler_strings_lock);
    
    click_ino.cleanup();
}

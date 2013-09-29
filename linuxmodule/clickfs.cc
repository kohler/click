// -*- c-basic-offset: 4 -*-
/*
 * clickfs.cc -- the Click filesystem
 * Eddie Kohler
 *
 * Copyright (c) 2002-2003 International Computer Science Institute
 * Copyright (c) 2004-2010 Regents of the University of California
 * Copyright (c) 2008 Meraki, Inc.
 * Copyright (c) 2002-2013 Eddie Kohler
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
#include <click/master.hh>
#include <click/straccum.hh>
#include <click/llrpc.h>
#include <click/ino.hh>
#include <click/list.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/mutex.h>
#include <linux/namei.h>
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

static struct mutex clickfs_lock, click_ino_lock;
static wait_queue_head_t clickfs_waitq;
static atomic_t clickfs_read_count;
extern uint32_t click_config_generation;
static int clickfs_ready;

//#define SPIN_LOCK_MSG(l, file, line, what)	printk(KERN_ALERT "%s:%d: pid %d: %sing %p in clickfs\n", (file), (line), current->pid, (what), (l))
#define SPIN_LOCK_MSG(l, file, line, what)	((void)(file), (void)(line))
#define SPIN_LOCK(l, file, line)	do { SPIN_LOCK_MSG((l), (file), (line), "lock"); mutex_lock((l)); } while (0)
#define SPIN_UNLOCK(l, file, line)	do { SPIN_LOCK_MSG((l), (file), (line), "unlock"); mutex_unlock((l)); } while (0)

#define LOCK_CONFIG(type)	lock_config(__FILE__, __LINE__, (type))
#define UNLOCK_CONFIG(type)	unlock_config(__FILE__, __LINE__, (type))
#define LOCK_CONFIG_READ()	LOCK_CONFIG(0)
#define UNLOCK_CONFIG_READ()	UNLOCK_CONFIG(0)
#define LOCK_CONFIG_WRITE()	LOCK_CONFIG(1)
#define UNLOCK_CONFIG_WRITE()	UNLOCK_CONFIG(1)


/*************************** Config locking *********************************/

extern struct task_struct *clickfs_task;

static inline void
lock_config(const char *file, int line, int iswrite)
{
    wait_queue_t wait;
# define private linux_private
    init_wait(&wait);
# undef private
    for (;;) {
	SPIN_LOCK(&clickfs_lock, file, line);
	prepare_to_wait(&clickfs_waitq, &wait, TASK_UNINTERRUPTIBLE);
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
    finish_wait(&clickfs_waitq, &wait);
    clickfs_task = current;
}

static inline void
unlock_config(const char* file, int line, int iswrite)
{
    clickfs_task = 0;
    if (iswrite) {
        assert(atomic_read(&clickfs_read_count) == -1);
        atomic_inc(&clickfs_read_count);
        wake_up_all(&clickfs_waitq);
    } else {
        assert(atomic_read(&clickfs_read_count) > 0);
        atomic_dec(&clickfs_read_count);
        wake_up(&clickfs_waitq);
    }
}


/*************************** Inode constants ********************************/

#define INODE_INFO(inode)		(*((ClickInodeInfo *)(&(inode)->i_private)))
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
#define set_nlink(inode, nlink)		((inode)->i_nlink = (nlink))
#endif

struct ClickInodeInfo {
    uint32_t config_generation;
};

static ClickIno click_ino;

// Must be called with LOCK_CONFIG_READ.

static int click_ino_check() {
    int r = 0;
    if (click_ino.generation() != click_config_generation) {
        SPIN_LOCK(&click_ino_lock, __FILE__, __LINE__);
        r = click_ino.prepare(click_router, click_config_generation);
        SPIN_UNLOCK(&click_ino_lock, __FILE__, __LINE__);
    }
    return r;
}

// Must be called with LOCK_CONFIG_READ.
// Return "subdir_error" if this subdirectory was configuration-specific and
// the configuration was changed.  Otherwise, updates this directory's link
// count (if the configuration was changed) and returns 0.

// If click_ino_check returns < 0, then it is not safe to access
// click_ino. If click_ino_check returns 0, then it is safe to access
// both click_ino and `inode`.

static int click_ino_check(struct inode *inode, int subdir_error) {
    int error;
    if (INODE_INFO(inode).config_generation != click_config_generation) {
	if (click_ino.has_element(inode->i_ino))
	    return subdir_error;
	if ((error = click_ino_check()) < 0)
	    return error;
	INODE_INFO(inode).config_generation = click_config_generation;
	set_nlink(inode, click_ino.nlink(inode->i_ino));
    }
    return 0;
}


/*************************** Inode operations ********************************/

static struct inode *
click_inode(struct super_block *sb, ino_t ino)
{
    // Must be called with clickfs_lock held.

    if (click_ino_check() < 0)
	return 0;

    struct inode *inode = new_inode(sb);
    if (!inode)
	return 0;

    inode->i_ino = ino;
    INODE_INFO(inode).config_generation = click_config_generation;

    if (click_ino.is_handler(ino)) {
	int hi = click_ino.ino_handler(ino);
	if (const Handler *h = Router::handler(click_router, hi)) {
	    inode->i_mode = S_IFREG;
            if (h->read_visible())
                inode->i_mode |= click_fsmode.read;
            if (h->write_visible() || (h->read_visible() && h->read_param()))
                inode->i_mode |= click_fsmode.write;
	    inode->i_uid = click_fsmode.uid;
	    inode->i_gid = click_fsmode.gid;
	    inode->i_op = click_handler_inode_ops;
	    inode->i_fop = click_handler_file_ops;
	    set_nlink(inode, click_ino.nlink(ino));
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
	set_nlink(inode, click_ino.nlink(ino));
    }

    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;

    MDEBUG("%lx:%p:%p: leaving click_inode", ino, inode, inode->i_op);
    return inode;
}



/*************************** Directory operations ****************************/

extern "C" {

static struct dentry *
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
click_dir_lookup(struct inode *dir, struct dentry *dentry, unsigned)
#else
click_dir_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *)
#endif
{
    LOCK_CONFIG_READ();
    MDEBUG("click_dir_lookup %lx", dir->i_ino);

    struct inode *inode = 0;
    int error;
    if ((error = click_ino_check(dir, -EIO)) >= 0) {
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
#if !HAVE_LINUX_SUPER_BLOCK_S_D_OP
	dentry->d_op = &click_dentry_ops;
#endif
	d_add(dentry, inode);
	return 0;
    }
}

static int
click_dentry_revalidate(struct dentry *dentry, unsigned flags)
{
    struct inode *inode = dentry->d_inode;
    int r;
    MDEBUG("click_dentry_revalidate %lx", (inode ? inode->i_ino : 0));
    if (!inode)
	return -EINVAL;
    if (INODE_INFO(inode).config_generation == click_config_generation)
	return 1;

    LOCK_CONFIG_READ();
    if (click_ino.has_element(inode->i_ino)) { // not a global directory
	shrink_dcache_parent(dentry);
	d_drop(dentry);
	r = 0;
    }
# ifdef LOOKUP_RCU
    else if (flags & LOOKUP_RCU)
	r = -ECHILD;
# endif
    else if ((r = click_ino_check(inode, -EIO)) == 0)
        r = 1;
    UNLOCK_CONFIG_READ();
    return r;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
static int
click_dentry_revalidate_nd(struct dentry *dentry, struct nameidata *nd)
{
    return click_dentry_revalidate(dentry, nd->flags);
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

    int error = click_ino_check(inode, -ENOENT);
    int stored = 0;
    if (error < 0)
	goto done;

    // global '..'
    if (ino == ClickIno::ino_globaldir && f_pos == 0) {
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
    return (error ? error : stored);
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
#if HAVE_LINUX_SUPER_BLOCK_S_D_OP
    sb->s_d_op = &click_dentry_ops;
#endif
    MDEBUG("click_config_lock");
    LOCK_CONFIG_READ();
    root_inode = click_inode(sb, ClickIno::ino_globaldir);
    UNLOCK_CONFIG_READ();
    if (!root_inode)
	goto out_no_root;
#if HAVE_LINUX_D_MAKE_ROOT
    sb->s_root = d_make_root(root_inode);
#else
    sb->s_root = d_alloc_root(root_inode);
#endif
    MDEBUG("got root inode %p:%p", root_inode, root_inode->i_op);
    MDEBUG("d_op %p:%p", &click_dentry_ops, sb->s_root->d_op);
    if (!sb->s_root)
	goto out_no_root;
    // XXX options

    MDEBUG("got root directory");
    proclikefs_read_super(sb);
    MDEBUG("done click_read_super");
    return sb;

  out_no_root:
    printk(KERN_ALERT "click_read_super: get root inode failed\n");
    iput(root_inode);
    sb->s_dev = 0;
    return 0;
}

static int
click_fill_super(struct super_block *sb, void *data, int flags)
{
    return click_read_super(sb, data, flags) ? 0 : -ENOMEM;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
static struct dentry *
click_get_sb(struct file_system_type *fs_type, int flags, const char *, void *data)
{
    return mount_single(fs_type, flags, data, click_fill_super);
}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
static int
click_get_sb(struct file_system_type *fs_type, int flags, const char *, void *data, struct vfsmount *vfsmount)
{
    return get_sb_single(fs_type, flags, data, click_fill_super, vfsmount);
}
#else
static struct super_block *
click_get_sb(struct file_system_type *fs_type, int flags, const char *, void *data)
{
    return get_sb_single(fs_type, flags, data, click_fill_super);
}
#endif

static void
click_reread_super(struct super_block *sb)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
    lock_super(sb);
#endif
    if (sb->s_root) {
	struct inode *old_inode = sb->s_root->d_inode;
	LOCK_CONFIG_READ();
	sb->s_root->d_inode = click_inode(sb, ClickIno::ino_globaldir);
	UNLOCK_CONFIG_READ();
	iput(old_inode);
	sb->s_blocksize = 1024;
	sb->s_blocksize_bits = 10;
	sb->s_op = &click_superblock_ops;
#if HAVE_LINUX_SUPER_BLOCK_S_D_OP
	sb->s_d_op = &click_dentry_ops;
#endif
    } else
	printk(KERN_ALERT "silly click_reread_super\n");
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
    unlock_super(sb);
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
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

// see static_assert below
#define HS_ALIVE		1
#define HS_READING		2
#define HS_DONE			4
#define HS_RAW			8
#define HS_DIRECT		HANDLER_DIRECT
#define HS_WRITE_UNLIMITED	HANDLER_WRITE_UNLIMITED

struct HandlerString {
    String data;
    int flags;                  // 0 means free
    List_member<HandlerString> link;
};

// handler_strings holds all currently active HandlerString objects, as well
// as a selection of freed ones for fast reuse. Freed objects are at the
// front() of the list.
static List<HandlerString, &HandlerString::link>* handler_strings = 0;
static int free_handler_strings = 0;
static struct mutex handler_strings_lock;

#define FILP_HS(filp)		(reinterpret_cast<HandlerString*>((filp)->private_data))
#define FILP_READ_HS(filp)	FILP_HS(filp)
#define FILP_WRITE_HS(filp)	FILP_HS(filp)

static HandlerString* alloc_handler_string(const Handler* h) {
    SPIN_LOCK(&handler_strings_lock, __FILE__, __LINE__);
    HandlerString* hs = 0;
    if (!handler_strings)
        handler_strings = new List<HandlerString, &HandlerString::link>;
    if (handler_strings) {
        if (!handler_strings->empty() && !handler_strings->front()->flags) {
            hs = handler_strings->front();
            handler_strings->pop_front();
            --free_handler_strings;
        } else
            hs = new HandlerString;
        if (hs) {
            handler_strings->push_back(hs);
            hs->flags = HS_ALIVE
                | (h->flags() & (HS_WRITE_UNLIMITED | HS_DIRECT));
        }
    }
    SPIN_UNLOCK(&handler_strings_lock, __FILE__, __LINE__);
    return hs;
}

static void free_handler_string(HandlerString* hs) {
    SPIN_LOCK(&handler_strings_lock, __FILE__, __LINE__);
    handler_strings->erase(hs);
    hs->data = String();
    hs->flags = 0;
    if (free_handler_strings < 8) {
        handler_strings->push_front(hs);
        ++free_handler_strings;
    } else
        delete hs;
    SPIN_UNLOCK(&handler_strings_lock, __FILE__, __LINE__);
}

static inline void handler_string_add_newline(HandlerString* hs,
                                              const Handler* h) {
    if (!h->raw()
        && !(hs->flags & HS_RAW)
        && !(hs->flags & HS_DIRECT)
        && hs->data
        && hs->data.back() != '\n')
        hs->data += '\n';
}

static inline String handler_string_strip_newline(const HandlerString* hs,
                                                  const Handler* h) {
    if (!h->raw()
        && !(hs->flags & HS_RAW)
        && hs->data
        && hs->data.back() == '\n')
        return hs->data.substring(hs->data.begin(), hs->data.end() - 1);
    else
        return hs->data;
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

    bool reading = (filp->f_mode & FMODE_READ) != 0;
    bool writing = (filp->f_mode & FMODE_WRITE) != 0;

    int retval = 0;
    HandlerString* hs = 0;
    const Handler *h;

    if ((filp->f_flags & O_APPEND)
	|| (!reading && writing && !(filp->f_flags & O_TRUNC)))
	retval = -EACCES;
    else if ((retval = click_ino_check(inode, -EIO)) < 0)
	/* save retval */;
    else if (!(h = Router::handler(click_router, click_ino.ino_handler(inode->i_ino))))
	retval = -EIO;
    else if (reading && writing && (h->flags() & HANDLER_DIRECT))
        retval = -EACCES;
    else if (reading ? !h->read_visible() : !h->write_visible())
	retval = -EPERM;
    else if (!(hs = alloc_handler_string(h)))
	retval = -ENOMEM;
    else
	retval = 0;

    UNLOCK_CONFIG_READ();

    if (retval < 0 && hs) {
	free_handler_string(hs);
	hs = 0;
    }
    filp->private_data = hs;
    return retval;
}

static ssize_t
handler_prepare_read(HandlerString* hs, struct file* filp,
                     char* buffer, size_t count, loff_t* store_f_pos)
{
    int locktype = 0;
retry:
    switch (locktype) {
    case 0: LOCK_CONFIG_READ(); break;
    case 1: LOCK_CONFIG_WRITE(); break;
    }
    ssize_t retval;
    const Handler *h;
    struct inode *inode = filp->f_dentry->d_inode;
    if ((retval = click_ino_check(inode, -EIO)) < 0)
        /* save retval */;
    else if (!(h = Router::handler(click_router, click_ino.ino_handler(inode->i_ino))))
        retval = -EIO;
    else if (!h->read_visible())
        retval = -EPERM;
    else {
        int eindex = click_ino.ino_element(inode->i_ino);
        Element *e = Router::element(click_router, eindex);

        if ((hs->flags & HS_DIRECT) && buffer) {
            click_handler_direct_info hdi;
            hdi.buffer = buffer;
            hdi.count = count;
            hdi.store_f_pos = store_f_pos;
            hdi.string = &hs->data;
            hdi.retval = 0;
            (void) h->__call_read(e, &hdi);
            count = hdi.count;
            retval = hdi.retval;
        } else if (hs->flags & HS_DIRECT)
            retval = -EINVAL;
        else {
            String param;
            if ((filp->f_mode & FMODE_READ) && (filp->f_mode & FMODE_WRITE))
                param = handler_string_strip_newline(hs, h);
            if (h->nonconst() && locktype == 0) {
                UNLOCK_CONFIG_READ();
                locktype = 1;
                goto retry;
            }
            if (h->exclusive()) {
                lock_threads();
                hs->data = h->call_read(e, param, 0).unique();
                unlock_threads();
            } else
                hs->data = h->call_read(e, param, 0);
            handler_string_add_newline(hs, h);
            retval = (hs->data.out_of_memory() ? -ENOMEM : 0);
        }
    }
    switch (locktype) {
    case 0: UNLOCK_CONFIG_READ(); break;
    case 1: UNLOCK_CONFIG_WRITE(); break;
    }
    if (retval >= 0)
        hs->flags |= HS_DONE;
    return retval;
}

static loff_t
handler_llseek(struct file* filp, loff_t offset, int origin)
{
    HandlerString* hs = FILP_HS(filp);
    if (!hs)
        return -EIO;

    if ((filp->f_mode & FMODE_READ) && (filp->f_mode & FMODE_WRITE))
        return -ESPIPE;

    if (origin == SEEK_END) {
        // ensure the string's existence before seeking
        if ((filp->f_mode & FMODE_READ)
            && (hs->flags & (HS_DIRECT | HS_DONE)) != HS_DONE) {
            ssize_t r = handler_prepare_read(hs, filp, 0, 0, 0);
            if (r < 0)
                return r;
        }
        offset += hs->data.length();
    } else if (origin == SEEK_CUR)
        offset += filp->f_pos;

    if (offset >= 0 && offset <= 0x7FFFFFFF) {
        if (offset != filp->f_pos) {
            filp->f_pos = offset;
            filp->f_version = 0;
        }
        return offset;
    } else
        return -EINVAL;
}

static ssize_t
handler_read(struct file *filp, char *buffer, size_t count, loff_t *store_f_pos)
{
    loff_t f_pos = *store_f_pos;
    ssize_t r = -EIO;
    HandlerString* hs = FILP_READ_HS(filp);
    if (!hs)
	return r;

    // read-write handler: reset file position if switching to reading
    if ((filp->f_mode & FMODE_READ) && (filp->f_mode & FMODE_WRITE)
        && !(hs->flags & HS_READING)) {
        f_pos = 0;
        hs->flags |= HS_READING;
    }

    // (re)read handler if necessary
    if ((hs->flags & (HS_DIRECT | HS_DONE)) != HS_DONE) {
        r = handler_prepare_read(hs, filp, buffer, count, store_f_pos);
        if (r < 0)
            return r;
    }

    if (!(hs->flags & HS_DIRECT)) {
	const String &s = hs->data;
	if (f_pos > s.length())
	    f_pos = s.length();
	if (f_pos + count > s.length())
	    count = s.length() - f_pos;
	if (copy_to_user(buffer, s.data() + f_pos, count) > 0)
	    return -EFAULT;
        *store_f_pos = f_pos + count;
        r = count;
    }

    return r;
}

static ssize_t
handler_write(struct file *filp, const char *buffer, size_t count, loff_t *store_f_pos)
{
    loff_t f_pos = *store_f_pos;
    HandlerString* hs = FILP_WRITE_HS(filp);
    if (!hs)
	return -EIO;
    String &s = hs->data;
    int old_length = s.length();

    hs->flags &= ~HS_DONE;
#ifdef LARGEST_HANDLER_WRITE
    if (f_pos + count > LARGEST_HANDLER_WRITE
	&& !(hs->flags & HS_WRITE_UNLIMITED))
	return -EFBIG;
#endif
    // read-write handler: reset file position if switching to writing
    if ((filp->f_mode & FMODE_READ) && (filp->f_mode & FMODE_WRITE)
        && (hs->flags & HS_READING)) {
        f_pos = 0;
        hs->flags &= ~HS_READING;
    }

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

    *store_f_pos = f_pos + count;
    return count;
}

static int
handler_do_write(struct file *filp, void *address_ptr)
{
    HandlerString* hs = FILP_WRITE_HS(filp);
    struct inode *inode = filp->f_dentry->d_inode;
    const Handler *h;
    int retval;

    if ((retval = click_ino_check(inode, -EIO)) < 0)
	/* save retval */;
    else if (!(h = Router::handler(click_router, click_ino.ino_handler(inode->i_ino)))
	     || !h->write_visible())
	retval = -EIO;
    else if (hs->data.out_of_memory())
	retval = -ENOMEM;
    else {
	int eindex = click_ino.ino_element(inode->i_ino);
	Element *e = Router::element(click_router, eindex);
	click_llrpc_call_handler_st chs;
	chs.flags = 0;
	int r;

	if (address_ptr
	    && (r = CLICK_LLRPC_GET_DATA(&chs, address_ptr, sizeof(chs))) < 0) {
	    retval = r;
	    goto exit;
	}

	String data;
        if (!address_ptr || !(chs.flags & CLICK_LLRPC_CALL_HANDLER_FLAG_RAW))
            data = handler_string_strip_newline(hs, h);
        else
            data = hs->data;

	ClickfsHandlerErrorHandler cerrh;
	if (h->exclusive()) {
	    lock_threads();
	    retval = h->call_write(data, e, &cerrh);
	    unlock_threads();
	} else
	    retval = h->call_write(data, e, &cerrh);

	hs->flags |= HS_DONE;

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
    bool writing = (filp->f_mode & FMODE_WRITE) && !(filp->f_mode & FMODE_READ);
    HandlerString* hs = FILP_WRITE_HS(filp);
    int retval = 0;

#ifdef file_count
    long f_count = file_count(filp);
#else
    int f_count = atomic_read(&filp->f_count);
#endif

    if (writing && f_count == 1 && hs && !(hs->flags & HS_DONE)) {
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
    HandlerString* hs = FILP_READ_HS(filp);
    if (hs)
	free_handler_string(hs);
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

    if ((retval = click_ino_check(inode, -EIO)) < 0)
	/* save retval */;
    else if (!click_router)
	retval = -EINVAL;
    else if (command == CLICK_LLRPC_CALL_HANDLER)
	retval = handler_do_write(filp, reinterpret_cast<void *>(address));
    else if (command == CLICK_LLRPC_ABANDON_HANDLER) {
	HandlerString* hs = FILP_HS(filp);
	hs->data = String();
	hs->flags |= HS_DONE;
	retval = 0;
    } else if (command == CLICK_LLRPC_RAW_HANDLER) {
	HandlerString* hs = FILP_HS(filp);
	hs->flags |= HS_RAW;
	retval = 0;
    } else if (click_ino.ino_element(inode->i_ino) < 0
               || !(e = click_router->element(click_ino.ino_element(inode->i_ino))))
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
    if (!clickfs)
	clickfs = proclikefs_register_filesystem("click", 0, click_get_sb);
    if (clickfs)
	return proclikefs_new_file_operations(clickfs);
    else
	return 0;
}

int
init_clickfs()
{
    static_assert(HANDLER_DIRECT + HANDLER_WRITE_UNLIMITED < Handler::USER_FLAG_0, "Too few driver handler flags available.");
    static_assert(((HS_DIRECT | HS_WRITE_UNLIMITED) & (HS_READING | HS_DONE | HS_RAW)) == 0, "Handler flag overlap.");

    mutex_init(&handler_strings_lock);
    mutex_init(&clickfs_lock);
    mutex_init(&click_ino_lock);
    init_waitqueue_head(&clickfs_waitq);
    atomic_set(&clickfs_read_count, 0);

    // clickfs creation moved to click_new_file_operations()
    if (!(click_dir_file_ops = click_new_file_operations())
	|| !(click_dir_inode_ops = proclikefs_new_inode_operations(clickfs))
	|| !(click_handler_file_ops = click_new_file_operations())
	|| !(click_handler_inode_ops = proclikefs_new_inode_operations(clickfs))) {
	printk(KERN_ALERT "click: could not initialize clickfs!\n");
	return -EINVAL;
    }

    click_superblock_ops.put_super = proclikefs_put_super;
    // XXX statfs

    click_dentry_ops.d_delete = click_delete_dentry;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
    click_dentry_ops.d_revalidate = click_dentry_revalidate;
#else
    click_dentry_ops.d_revalidate = click_dentry_revalidate_nd;
#endif

    click_dir_file_ops->read = generic_read_dir;
    click_dir_file_ops->readdir = click_dir_readdir;
    click_dir_inode_ops->lookup = click_dir_lookup;

    click_handler_file_ops->llseek = handler_llseek;
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
    while (handler_strings && !handler_strings->empty()) {
        HandlerString* hs = handler_strings->front();
        handler_strings->pop_front();
        delete hs;
    }
    delete handler_strings;
    handler_strings = 0;
    SPIN_UNLOCK(&handler_strings_lock, __FILE__, __LINE__);

    MDEBUG("click_ino cleanup");
    click_ino.cleanup();
}

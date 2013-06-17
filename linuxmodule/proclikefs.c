/* -*- c-basic-offset: 4 -*- */
/*
 * proclikefs.c -- /proc-like file system infrastructure; allow file systems
 * to be unmounted even while active
 * Eddie Kohler
 *
 * Copyright (c) 2002-2003 International Computer Science Institute
 * Copyright (c) 2005 Regents of the University of California
 * Copyright (c) 2002-2013 Eddie Kohler
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

#undef CLICK_LINUXMODULE
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30) \
	&& !defined(AUTOCONF_INCLUDED)
# include <linux/autoconf.h>
#endif
#ifndef EXPORT_SYMTAB
# define EXPORT_SYMTAB
#endif
#ifdef CONFIG_MODVERSIONS
# define MODVERSIONS
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#  include <linux/modversions.h>
# endif
#endif
#include <linux/module.h>
#include "proclikefs.h"
#include <linux/string.h>
#include <linux/slab.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
# include <linux/locks.h>
#endif
#include <linux/file.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
# include <linux/namei.h>
#endif
#include <linux/dcache.h>
#define CLICK_CONFIG_LINUXMODULE_SYMBOLS_ONLY 1
#include <click/config-linuxmodule.h>
#ifndef HAVE_LINUX_SB_LOCK
# define HAVE_LINUX_SB_LOCK 0
#endif
#ifndef HAVE_LINUX_FILES_LOCK
# define HAVE_LINUX_FILES_LOCK 0
#endif
#ifndef HAVE_LINUX_FILES_LGLOCK
# define HAVE_LINUX_FILES_LGLOCK 0
#endif

#if HAVE_LINUX_FILES_LGLOCK
# include <linux/lglock.h>
# ifdef DECLARE_LGLOCK
DECLARE_LGLOCK(files_lglock);
#  define file_list_lock() lg_local_lock(files_lglock)
#  define file_list_unlock() lg_local_unlock(files_lglock)
# elif HAVE_LINUX_FILES_LGLOCK
extern struct lglock files_lglock;
#  define file_list_lock() lg_local_lock(&files_lglock)
#  define file_list_unlock() lg_local_unlock(&files_lglock)
# endif
#endif

#ifndef MOD_DEC_USE_COUNT
# define MOD_DEC_USE_COUNT	module_put(THIS_MODULE)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
# define fstype_supers_init(fst) INIT_HLIST_HEAD(&(fst)->fs_supers)
# define fstype_for_each_super(sb, fst) hlist_for_each_entry(sb, hlist_pos, &(fst)->fs_supers, s_instances)
#else
# define fstype_supers_init(fst) INIT_LIST_HEAD(&(fst)->fs_supers);
# define fstype_for_each_super(sb, fst) list_for_each_entry(sb, &(fst)->fs_supers, s_instances)
#endif


#if 0
# define DEBUG(args...) do { printk("<1>proclikefs: " args); printk("\n"); } while (0)
#else
# define DEBUG(args...) /* nada */
#endif

struct proclikefs_file_operations {
    struct file_operations pfo_op;
    struct proclikefs_file_operations *pfo_next;
};

struct proclikefs_inode_operations {
    struct inode_operations pio_op;
    struct proclikefs_inode_operations *pio_next;
};

#define PROCLIKEFS_NAME_LEN 32
struct proclikefs_file_system {
    struct file_system_type fs;
    atomic_t nsuper;
    int live;
    struct mutex lock;
    struct proclikefs_file_operations *pfs_pfo;
    struct proclikefs_inode_operations *pfs_pio;
    char name[PROCLIKEFS_NAME_LEN];
};

#define PROCLIKEFS_COUNT 2
static struct proclikefs_file_system fs_array[PROCLIKEFS_COUNT];
static struct mutex fslist_lock;
extern struct mutex inode_lock;

#if !HAVE_LINUX_SB_LOCK
# define lock_sb()
# define unlock_sb()
#else
extern spinlock_t sb_lock;
# define lock_sb() spin_lock(&sb_lock)
# define unlock_sb() spin_unlock(&sb_lock)
#endif

static struct super_operations proclikefs_null_super_operations;
static struct inode_operations proclikefs_null_root_inode_operations;
static struct dentry_operations proclikefs_null_dentry_operations;

EXPORT_SYMBOL(proclikefs_register_filesystem);
EXPORT_SYMBOL(proclikefs_reinitialize_supers);
EXPORT_SYMBOL(proclikefs_unregister_filesystem);
EXPORT_SYMBOL(proclikefs_read_super);
EXPORT_SYMBOL(proclikefs_put_super);
EXPORT_SYMBOL(proclikefs_new_file_operations);
EXPORT_SYMBOL(proclikefs_new_inode_operations);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static struct super_block *
proclikefs_null_read_super(struct super_block *sb, void *data, int silent)
{
    DEBUG("null_read_super");
    sb->s_dev = 0;
    return 0;
}
#endif

static struct dentry *
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
proclikefs_null_root_lookup(struct inode *dir, struct dentry *dentry, unsigned flags)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 16)
proclikefs_null_root_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *namei)
#else
proclikefs_null_root_lookup(struct inode *dir, struct dentry *dentry)
#endif
{
    return (struct dentry *)(ERR_PTR(-ENOENT));
}

static int
proclikefs_defined(struct proclikefs_file_system *pfs)
{
    return pfs->live != 0 || atomic_read(&pfs->nsuper) != 0;
}

struct proclikefs_file_system *
proclikefs_register_filesystem(const char *name, int fs_flags,
			       proclikefs_mountfunc mountfunc)
{
    struct proclikefs_file_system *newfs = 0;
    int i, err;

    if (!name || strlen(name) >= PROCLIKEFS_NAME_LEN)
	return 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    if (!try_module_get(THIS_MODULE)) {
	printk("<1>proclikefs: error using module\n");
	return 0;
    }
#else
    MOD_INC_USE_COUNT;
#endif

    mutex_lock(&fslist_lock);

    /* look for a defined filesystem with that name */
    for (i = 0; i < PROCLIKEFS_COUNT; ++i)
	if (!proclikefs_defined(&fs_array[i]))
	    newfs = &fs_array[i];
	else if (strcmp(name, fs_array[i].name) == 0) {
	    newfs = &fs_array[i];
	    if (newfs->live) { /* active filesystem */
		mutex_unlock(&fslist_lock);
		MOD_DEC_USE_COUNT;
		return 0;
	    } else
		break;
	}
    if (!newfs) {
	printk("<1>proclikefs: out of file system space\n");
	return 0;
    }

    /* otherwise, initialize new file system */
    DEBUG("pfs[%p]: creating filesystem %s", newfs, name);
    newfs->fs.fs_flags = fs_flags;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
    newfs->fs.mount = mountfunc;
    newfs->fs.kill_sb = kill_anon_super;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    newfs->fs.get_sb = mountfunc;
    newfs->fs.kill_sb = kill_anon_super;
#else
    newfs->fs.read_super = mountfunc;
#endif

    if (!proclikefs_defined(newfs)) {
	newfs->pfs_pfo = 0;
	newfs->pfs_pio = 0;
	strcpy(newfs->name, name);
	newfs->fs.name = newfs->name;
	newfs->fs.next = 0;
	newfs->fs.owner = THIS_MODULE;
	fstype_supers_init(&newfs->fs);
	err = register_filesystem(&newfs->fs);
	if (err != 0)
	    printk("<1>proclikefs: error %d while initializing pfs[%p] (%s)\n", -err, newfs, name);
    }

    newfs->live = 1;
    mutex_unlock(&fslist_lock);
    return newfs;
}

void
proclikefs_reinitialize_supers(struct proclikefs_file_system *pfs,
			       void (*reread_super) (struct super_block *))
{
    struct super_block *sb;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
    struct hlist_node *hlist_pos;
#endif
    mutex_lock(&fslist_lock);
    /* transfer superblocks */
    lock_sb();
    fstype_for_each_super(sb, &pfs->fs) {
	if (sb->s_type == &pfs->fs)
	    (*reread_super)(sb);
	else
	    printk("<1>proclikefs: confusion\n");
    }
    unlock_sb();
    mutex_unlock(&fslist_lock);
}

static void
proclikefs_kill_super(struct super_block *sb, struct file_operations *dummy)
{
    struct dentry *dentry_tree;
    struct file *filp;
    int cpu;
    (void) cpu;

    DEBUG("killing files");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 16)
# define FILE_MEMBER f_u.fu_list
#else
# define FILE_MEMBER f_list
#endif
#if HAVE_LINUX_FILES_LOCK || HAVE_LINUX_FILES_LGLOCK
    file_list_lock();
#else
# warning "potential race: file_list_lock() is not available"
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36) && CONFIG_SMP
    for_each_possible_cpu(cpu) {
	list_for_each_entry(filp, per_cpu_ptr(sb->s_files, cpu), FILE_MEMBER) {
	    filp->f_op = dummy;
	}
    }
#else
    list_for_each_entry(filp, &sb->s_files, FILE_MEMBER) {
	filp->f_op = dummy;
    }
#endif
#if HAVE_LINUX_FILES_LOCK || HAVE_LINUX_FILES_LGLOCK
    file_list_unlock();
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
    lock_super(sb);
#endif

    sb->s_op = &proclikefs_null_super_operations;
#if HAVE_LINUX_SUPER_BLOCK_S_D_OP
    sb->s_d_op = 0;
#endif

    /* will not create new dentries any more */

    /* clear out dentries, starting from the root */
    /* Develop a linked list corresponding to depth-first search, through
       the d_fsdata fields. */
    /* XXX locking? */

    DEBUG("killing dentries");
    dentry_tree = sb->s_root;
    if (dentry_tree) {
	/* Do not d_drop(root) */
	dentry_tree->d_fsdata = 0;
    }
    while (dentry_tree) {
	struct list_head *next;
	struct dentry *active = dentry_tree;
	/* Process this dentry, move to next */
	active->d_op = &proclikefs_null_dentry_operations;
	dentry_tree = (struct dentry *)active->d_fsdata;
	/* Prepend children to dentry_tree */
	next = active->d_subdirs.next;
	while (next != &active->d_subdirs) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 16)
	    struct dentry *child = list_entry(next, struct dentry, d_u.d_child);
#else
	    struct dentry *child = list_entry(next, struct dentry, d_child);
#endif
	    next = next->next;
	    d_drop(child);
	    child->d_fsdata = (void *)dentry_tree;
	    dentry_tree = child;
	}
    }

    /* But the root inode can't be a dead inode */
    sb->s_root->d_inode->i_op = &proclikefs_null_root_inode_operations;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
    unlock_super(sb);
#endif
    DEBUG("done killing super");
}

void
proclikefs_unregister_filesystem(struct proclikefs_file_system *pfs)
{
    struct super_block *sb, dummy_sb;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
    struct hlist_node *hlist_pos;
#endif
    struct proclikefs_file_operations *pfo;
    struct proclikefs_inode_operations *pio;
    struct inode dummy_inode;

    if (!pfs)
	return;

    DEBUG("unregister_filesystem entry");
    mutex_lock(&fslist_lock);

    /* create a garbage inode (which requires creating a garbage superblock) */
    inode_init_once(&dummy_inode);
    dummy_sb.s_time_gran = 0;
    dummy_inode.i_sb = &dummy_sb;
    make_bad_inode(&dummy_inode);

    /* clear out file operations */
    for (pfo = pfs->pfs_pfo; pfo; pfo = pfo->pfo_next) {
	struct file_operations *fo = &pfo->pfo_op;
	memcpy(&fo->llseek, &dummy_inode.i_fop->llseek,
	       sizeof(struct file_operations) - offsetof(struct file_operations, llseek));
    }

    for (pio = pfs->pfs_pio; pio; pio = pio->pio_next) {
	struct inode_operations *io = &pio->pio_op;
	memcpy(io, dummy_inode.i_op, sizeof(struct inode_operations));
    }

    mutex_lock(&pfs->lock);

    /* clear out superblock operations */
    DEBUG("clearing superblocks");
    lock_sb();
    fstype_for_each_super(sb, &pfs->fs) {
	proclikefs_kill_super(sb, &pfs->pfs_pfo->pfo_op);
    }
    unlock_sb();

    pfs->live = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
    pfs->fs.read_super = proclikefs_null_read_super;
#endif
    MOD_DEC_USE_COUNT;

    mutex_unlock(&pfs->lock);
    mutex_unlock(&fslist_lock);
}

void
proclikefs_read_super(struct super_block *sb)
{
    struct proclikefs_file_system *pfs = (struct proclikefs_file_system *) (sb->s_type);
    atomic_inc(&pfs->nsuper);
    DEBUG("pfs[%p]: read_super for %s", pfs, pfs->fs.name);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    if (!try_module_get(THIS_MODULE))
	printk("<1>proclikefs: error using module\n");
#else
    MOD_INC_USE_COUNT;
#endif
}

void
proclikefs_put_super(struct super_block *sb)
{
    struct proclikefs_file_system *pfs = (struct proclikefs_file_system *) (sb->s_type);
    DEBUG("pfs[%p]: put_super for %s", pfs, pfs->fs.name);
    mutex_lock(&fslist_lock);
    atomic_dec(&pfs->nsuper);
    if (!proclikefs_defined(pfs)) {
	struct proclikefs_file_operations *pfo;
	struct proclikefs_inode_operations *pio;

	unregister_filesystem(&pfs->fs);
	while ((pfo = pfs->pfs_pfo)) {
	    pfs->pfs_pfo = pfo->pfo_next;
	    kfree(pfo);
	}
	while ((pio = pfs->pfs_pio)) {
	    pfs->pfs_pio = pio->pio_next;
	    kfree(pio);
	}
    }
    mutex_unlock(&fslist_lock);
    MOD_DEC_USE_COUNT;
}

struct file_operations *
proclikefs_new_file_operations(struct proclikefs_file_system *pfs)
{
    struct proclikefs_file_operations *pfo = kmalloc(sizeof(struct proclikefs_file_operations), GFP_ATOMIC);

    if (pfo) {
	mutex_lock(&fslist_lock);
	pfo->pfo_next = pfs->pfs_pfo;
	pfs->pfs_pfo = pfo;
	mutex_unlock(&fslist_lock);
	memset(&pfo->pfo_op, 0, sizeof(struct file_operations));
    }
    return &pfo->pfo_op;
}

struct inode_operations *
proclikefs_new_inode_operations(struct proclikefs_file_system *pfs)
{
    struct proclikefs_inode_operations *pio = kmalloc(sizeof(struct proclikefs_inode_operations), GFP_ATOMIC);

    if (pio) {
	mutex_lock(&fslist_lock);
	pio->pio_next = pfs->pfs_pio;
	pfs->pfs_pio = pio;
	mutex_unlock(&fslist_lock);
	memset(&pio->pio_op, 0, sizeof(struct inode_operations));
    }
    return &pio->pio_op;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
void
proclikefs_read_inode(struct inode *inode)
{
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
static int proclikefs_d_revalidate(struct dentry *dir, unsigned flags)
#else
static int proclikefs_d_revalidate(struct dentry *dir, struct nameidata *nd)
#endif
{
    return 0;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 32)
static int proclikefs_d_delete(const struct dentry *dir)
#else
static int proclikefs_d_delete(struct dentry *dir)
#endif
{
    (void) dir;
    return 0;
}

int
init_module(void)
{
    int i;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
    proclikefs_null_super_operations.read_inode = proclikefs_read_inode;
#endif
    proclikefs_null_super_operations.put_super = proclikefs_put_super;
    proclikefs_null_root_inode_operations.lookup = proclikefs_null_root_lookup;
    proclikefs_null_dentry_operations.d_revalidate = proclikefs_d_revalidate;
    proclikefs_null_dentry_operations.d_delete = proclikefs_d_delete;
    mutex_init(&fslist_lock);
    for (i = 0; i < PROCLIKEFS_COUNT; ++i) {
	fs_array[i].live = 0;
	atomic_set(&fs_array[i].nsuper, 0);
	mutex_init(&fs_array[i].lock);
    }
    return 0;
}

void
cleanup_module(void)
{
    int i;
    mutex_lock(&fslist_lock);
    for (i = 0; i < PROCLIKEFS_COUNT; ++i) {
	if (proclikefs_defined(&fs_array[i]))
	    printk("<1>proclikefs: unregistering active FS %s, prepare to die\n", fs_array[i].name);
	unregister_filesystem(&fs_array[i].fs);
    }
    mutex_unlock(&fslist_lock);
}

#ifdef MODULE_AUTHOR
MODULE_AUTHOR("Eddie Kohler <ekohler@gmail.com>");
#endif
#ifdef MODULE_DESCRIPTION
MODULE_DESCRIPTION("Proclikefs: allow module unload of mounted filesystems");
#endif
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

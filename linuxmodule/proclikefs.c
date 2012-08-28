/* -*- c-basic-offset: 4 -*- */
/*
 * proclikefs.c -- /proc-like file system infrastructure; allow file systems
 * to be unmounted even while active
 * Eddie Kohler
 *
 * Copyright (c) 2002-2003 International Computer Science Institute
 * Copyright (c) 2005 Regents of the University of California
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
#endif

#ifndef MOD_DEC_USE_COUNT
# define MOD_DEC_USE_COUNT	module_put(THIS_MODULE)
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

#define PROCLIKEFS_NAME_LEN (32)

struct proclikefs_file_system {
    struct file_system_type fs;
    atomic_t nsuper;
    int live;
    int allocated;
    struct mutex lock;
    struct proclikefs_file_operations *pfs_pfo;
    struct proclikefs_inode_operations *pfs_pio;
    char name[PROCLIKEFS_NAME_LEN];
};

#ifndef MAX_PROCLIKEFS
#define MAX_PROCLIKEFS (4)
#endif

static struct proclikefs_file_system pfs_filesystems[MAX_PROCLIKEFS];

static struct mutex fslist_lock;
extern struct mutex inode_lock;
#if HAVE_LINUX_SB_LOCK
extern spinlock_t sb_lock;
#endif

static struct super_operations proclikefs_null_super_operations;
static struct inode_operations proclikefs_null_root_inode_operations;

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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 16)
static struct dentry *
proclikefs_null_root_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *namei)
{
    return (struct dentry *)(ERR_PTR(-ENOENT));
}
#else
static struct dentry *
proclikefs_null_root_lookup(struct inode *dir, struct dentry *dentry)
{
    return (struct dentry *)(ERR_PTR(-ENOENT));
}
#endif

struct proclikefs_file_system *
proclikefs_register_filesystem(const char *name, int fs_flags,
			       proclikefs_mountfunc mountfunc)
{
    struct proclikefs_file_system *newfs = 0, *freefs = 0;
    int which;
    int newfs_is_new = 0;

    if (!name)
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

    for (which = 0; which < MAX_PROCLIKEFS; which++) {
	newfs = &pfs_filesystems[which];
	if (newfs->allocated) {
	    if (strcmp(name, newfs->name) == 0) {
		if (newfs->live > 0) { /* active filesystem with that name */
		    mutex_unlock(&fslist_lock);
		    MOD_DEC_USE_COUNT;
		    return 0;
		} else
		    break;
	    }
	} else {
	    if (!freefs)
		freefs = newfs;
	}
    }


    if (which == MAX_PROCLIKEFS) {
	if (!freefs) {
	    mutex_unlock(&fslist_lock);
	    MOD_DEC_USE_COUNT;
	    return 0;
	}
	newfs = freefs;
	newfs->pfs_pfo = 0;
	newfs->pfs_pio = 0;
	newfs->allocated = 1;
	strncpy(newfs->name, name, PROCLIKEFS_NAME_LEN - 1);
	newfs->name[PROCLIKEFS_NAME_LEN - 1] = 0; /* Just in case */
	mutex_init(&newfs->lock);
	atomic_set(&newfs->nsuper, 0);
	newfs->fs.name = newfs->name;
	newfs->fs.next = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
	newfs->fs.owner = THIS_MODULE;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
	INIT_LIST_HEAD(&newfs->fs.fs_supers);
#endif
	newfs_is_new = 1;
    }

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
    newfs->live = 1;
    DEBUG("pfs[%p]: created filesystem %s", newfs, name);

    if (newfs_is_new) {
	int err = register_filesystem(&newfs->fs);
	if (err != 0)
	    printk("<1>proclikefs: error %d while initializing pfs[%p] (%s)\n", -err, newfs, name);
    }

    mutex_unlock(&fslist_lock);
    return newfs;
}

void
proclikefs_reinitialize_supers(struct proclikefs_file_system *pfs,
			       void (*reread_super) (struct super_block *))
{
    struct super_block *sb;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
    struct list_head *p;
#endif
    mutex_lock(&fslist_lock);
    /* transfer superblocks */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
# if HAVE_LINUX_SB_LOCK
    spin_lock(&sb_lock);
# endif
    for (p = pfs->fs.fs_supers.next; p != &pfs->fs.fs_supers; p = p->next) {
	sb = list_entry(p, struct super_block, s_instances);
	if (sb->s_type == &pfs->fs)
	    (*reread_super)(sb);
	else
	    printk("<1>proclikefs: confusion\n");
    }
# if HAVE_LINUX_SB_LOCK
    spin_unlock(&sb_lock);
# endif
#else
    for (sb = sb_entry(super_blocks.next); sb != sb_entry(&super_blocks);
	 sb = sb_entry(sb->s_list.next))
	if (sb->s_type == &pfs->fs)
	    (*reread_super)(sb);
#endif
    mutex_unlock(&fslist_lock);
}

static void
proclikefs_kill_super(struct super_block *sb, struct file_operations *dummy)
{
    struct dentry *dentry_tree;
    struct list_head *p;
    int cpu;
#if HAVE_LINUX_FILES_LGLOCK
    DECLARE_LGLOCK(files_lglock);
#endif
    (void) cpu;

    DEBUG("killing files");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 16)
# if HAVE_LINUX_FILES_LOCK
    file_list_lock();
# elif HAVE_LINUX_FILES_LGLOCK
    lg_local_lock(files_lglock);
# endif
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
    for_each_possible_cpu(cpu) {
	list_for_each(p, per_cpu_ptr(sb->s_files, cpu)) {
	    struct file *filp = list_entry(p, struct file, f_u.fu_list);
	    filp->f_op = dummy;
	}
    }
# else
    list_for_each(p, &sb->s_files) {
	struct file *filp = list_entry(p, struct file, f_u.fu_list);
	filp->f_op = dummy;
    }
# endif
# if HAVE_LINUX_FILES_LOCK
    file_list_unlock();
# elif HAVE_LINUX_FILES_LGLOCK
    lg_local_unlock(files_lglock);
# endif
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
# if HAVE_LINUX_FILES_LOCK
    file_list_lock();
# endif
    for (p = sb->s_files.next; p != &sb->s_files; p = p->next) {
	struct file *filp = list_entry(p, struct file, f_list);
	filp->f_op = dummy;
    }
# if HAVE_LINUX_FILES_LOCK
    file_list_unlock();
# endif
#else
    (void) dummy;
    (void) p;
#endif

    lock_super(sb);

    sb->s_op = &proclikefs_null_super_operations;
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
	active->d_op = 0;
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

    unlock_super(sb);
    DEBUG("done killing super");
}

void
proclikefs_unregister_filesystem(struct proclikefs_file_system *pfs)
{
    struct super_block *sb, dummy_sb;
    struct file *filp;
    struct list_head *p;
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
    /* file operations cleared out superblock by superblock, below */
    (void) filp;
#else
    /* clear out file operations */
    /* inuse_filps is protected by the single kernel lock */
    /* XXX locking? */
    for (filp = inuse_filps; filp; filp = filp->f_next) {
	struct dentry *dentry = filp->f_dentry;
	if (!dentry)
	    continue;
	inode = dentry->d_inode;
	if (!inode || !inode->i_sb || inode->i_sb->s_type != &pfs->fs)
	    continue;
	filp->f_op = &pfs->pfs_pfo->pfo_op;
    }
#endif

    mutex_lock(&pfs->lock);

    /* clear out superblock operations */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
    DEBUG("clearing superblocks");
# if HAVE_LINUX_SB_LOCK
    spin_lock(&sb_lock);
# endif
    for (p = pfs->fs.fs_supers.next; p != &pfs->fs.fs_supers; p = p->next) {
	sb = list_entry(p, struct super_block, s_instances);
	proclikefs_kill_super(sb, &pfs->pfs_pfo->pfo_op);
    }
# if HAVE_LINUX_SB_LOCK
    spin_unlock(&sb_lock);
# endif
#else
    for (sb = sb_entry(super_blocks.next); sb != sb_entry(&super_blocks);
	 sb = sb_entry(sb->s_list.next)) {
	if (sb->s_type != &pfs->fs)
	    continue;
	proclikefs_kill_super(sb, &pfs->pfs_pfo->pfo_op);
    }
    (void) p;
#endif

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
    atomic_dec(&pfs->nsuper);
    DEBUG("pfs[%p]: put_super for %s", pfs, pfs->fs.name);
    MOD_DEC_USE_COUNT;
    mutex_lock(&fslist_lock);
    if (!pfs->live && atomic_read(&pfs->nsuper) == 0) {
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
	pfs->allocated = 0;
    }
    mutex_unlock(&fslist_lock);
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

int
init_module(void)
{
    int which;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
    proclikefs_null_super_operations.read_inode = proclikefs_read_inode;
#endif
    proclikefs_null_super_operations.put_super = proclikefs_put_super;
    proclikefs_null_root_inode_operations.lookup = proclikefs_null_root_lookup;
    mutex_init(&fslist_lock);

    for (which = 0; which < MAX_PROCLIKEFS; which++) {
	mutex_init(&pfs_filesystems[which].lock);
    }
    return 0;
}

void
cleanup_module(void)
{
    int which;
    mutex_lock(&fslist_lock);
    for (which = 0; which < MAX_PROCLIKEFS; which++ ) {
	struct proclikefs_file_system *pfs = &pfs_filesystems[which];
	if (pfs->allocated && (pfs->live || atomic_read(&pfs->nsuper) != 0))
	    printk("<1>proclikefs: unregistering active FS %s, prepare to die\n", pfs->name);
	unregister_filesystem(&pfs->fs);
	pfs->allocated = 0;
    }
    mutex_unlock(&fslist_lock);
}

#ifdef MODULE_AUTHOR
MODULE_AUTHOR("Eddie Kohler <kohler@cs.ucla.edu>");
#endif
#ifdef MODULE_DESCRIPTION
MODULE_DESCRIPTION("Proclikefs: allow module unload of mounted filesystems");
#endif
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

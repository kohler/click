/* -*- c-basic-offset: 4 -*- */
/*
 * proclikefs.c -- /proc-like file system infrastructure; allow file systems
 * to be unmounted even while active
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

#undef CLICK_LINUXMODULE
#include <linux/config.h>
#ifndef EXPORT_SYMTAB
# define EXPORT_SYMTAB
#endif
#include <linux/module.h>
#include "proclikefs.h"
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/locks.h>
#include <linux/file.h>

#if 0
# define DEBUG(args...) do { printk("<1>proclikefs: " args); printk("\n"); } while (0)
#else
# define DEBUG(args...) /* nada */
#endif

struct proclikefs_file_system {
    struct file_system_type fs;
    struct list_head fs_list;
    struct list_head i_list;
    atomic_t nsuper;
    int live;
    spinlock_t lock;
    char name[1];
};

static LIST_HEAD(fs_list);
static spinlock_t fslist_lock;
extern spinlock_t inode_lock;
extern spinlock_t sb_lock;

static struct super_operations proclikefs_null_super_operations;

EXPORT_SYMBOL(proclikefs_register_filesystem);
EXPORT_SYMBOL(proclikefs_unregister_filesystem);
EXPORT_SYMBOL(proclikefs_read_super);
EXPORT_SYMBOL(proclikefs_put_super);
EXPORT_SYMBOL(proclikefs_read_inode);
EXPORT_SYMBOL(proclikefs_delete_inode);

static struct super_block *
proclikefs_null_read_super(struct super_block *sb, void *data, int silent)
{
    DEBUG("null_read_super");
    sb->s_dev = 0;
    return 0;
}

struct proclikefs_file_system *
proclikefs_register_filesystem(const char *name,
	struct super_block *(*read_super) (struct super_block *, void *, int),
	void (*reread_super) (struct super_block *))
{
    struct proclikefs_file_system *newfs = 0;
    struct list_head *next;
    int newfs_is_new = 0;
    
    if (!name)
	return 0;

    spin_lock(&fslist_lock);
    
    for (next = fs_list.next; next != &fs_list; next = next->next) {
	newfs = list_entry(next, struct proclikefs_file_system, fs_list);
	if (strcmp(name, newfs->name) == 0) {
	    if (newfs->live > 0) { /* active filesystem with that name */
		spin_unlock(&fslist_lock);
		return 0;
	    } else
		break;
	}
    }

    if (!newfs) {
	newfs = kmalloc(sizeof(struct proclikefs_file_system) + strlen(name), GFP_KERNEL);
	if (!newfs) {		/* out of memory */
	    spin_unlock(&fslist_lock);
	    return 0;
	}
	INIT_LIST_HEAD(&newfs->i_list);
	list_add(&newfs->fs_list, &fs_list);
	strcpy(newfs->name, name);
	spin_lock_init(&newfs->lock);
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

    newfs->fs.fs_flags = 0;
    newfs->fs.read_super = read_super;
    newfs->live = 1;

    if (newfs_is_new)
	register_filesystem(&newfs->fs); /* XXX check return value */
    else if (reread_super) {
	/* transfer superblocks */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
	struct list_head *p;
	spin_lock(&sb_lock);
	for (p = newfs->fs.fs_supers.next; p != &newfs->fs.fs_supers; p = p->next) {
	    struct super_block *sb = list_entry(p, struct super_block, s_instances);
	    if (sb->s_type == &newfs->fs)
		(*reread_super)(sb);
	    else
		printk("<1>proclikefs confusion\n");
	}
	spin_unlock(&sb_lock);
#else
	struct super_block *sb;
	for (sb = sb_entry(super_blocks.next); sb != sb_entry(&super_blocks); 
	     sb = sb_entry(sb->s_list.next))
	    if (sb->s_type == &newfs->fs)
		(*reread_super)(sb);
#endif
    }
    
    MOD_INC_USE_COUNT;

    spin_unlock(&fslist_lock);
    return newfs;
}

static void
proclikefs_kill_super(struct super_block *sb, struct inode *dummy_inode)
{
    struct dentry *dentry, *all_dentries;
    struct list_head *p;

    DEBUG("killing files");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
    file_list_lock();
    for (p = sb->s_files.next; p != &sb->s_files; p = p->next) {
	struct file *filp = list_entry(p, struct file, f_list);
	filp->f_op = dummy_inode->i_fop;
    }
    file_list_unlock();
#else
    (void) dummy_inode;
    (void) p;
#endif

    lock_super(sb);

    sb->s_op = &proclikefs_null_super_operations;
    /* will not create new dentries any more */

    /* clear out dentries, starting from the root */
    /* XXX locking? */
    
    DEBUG("killing dentries");
    all_dentries = sb->s_root;
    if (all_dentries) {
	d_drop(all_dentries);
	all_dentries->d_op = 0;
	all_dentries->d_fsdata = 0;
    }
    while (all_dentries) {
	struct list_head *next;
	dentry = all_dentries;
	all_dentries = (struct dentry *)dentry->d_fsdata;
	next = dentry->d_subdirs.next;
	while (next != &dentry->d_subdirs) {
	    struct dentry *child = list_entry(next, struct dentry, d_child);
	    next = next->next;
	    d_drop(child);
	    child->d_op = 0;
	    child->d_fsdata = (void *)all_dentries;
	    all_dentries = child;
	}
    }

    unlock_super(sb);
    DEBUG("done killing super");
}

void
proclikefs_unregister_filesystem(struct proclikefs_file_system *pfs)
{
    struct super_block *sb;
    struct inode *inode, dummy_inode;
    struct file *filp;
    struct list_head *p;
    
    if (!pfs)
	return;
    
    spin_lock(&fslist_lock);

    /* Borrow make_bad_inode's file operations. */
    make_bad_inode(&dummy_inode);
    
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
	filp->f_op = dummy_inode.i_op->default_file_ops;
    }
#endif
    
    spin_lock(&pfs->lock);

    /* clear out inode operations */
    {
	struct list_head *next = pfs->i_list.next;
	while (next != &pfs->i_list) {
	    struct list_head *tmp = next;
	    next = next->next;
	    inode = list_entry(tmp, struct inode, u);
	    make_bad_inode(inode);
	}
    }
    
    /* clear out superblock operations */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
    spin_lock(&sb_lock);
    for (p = pfs->fs.fs_supers.next; p != &pfs->fs.fs_supers; p = p->next) {
	sb = list_entry(p, struct super_block, s_instances);
	proclikefs_kill_super(sb, &dummy_inode);
    }
    spin_unlock(&sb_lock);
#else
    for (sb = sb_entry(super_blocks.next); sb != sb_entry(&super_blocks); 
	 sb = sb_entry(sb->s_list.next)) {
	if (sb->s_type != &pfs->fs)
	    continue;
	proclikefs_kill_super(sb, &dummy_inode);
    }
    (void) p;
#endif

    pfs->live = 0;
    pfs->fs.read_super = proclikefs_null_read_super;
    MOD_DEC_USE_COUNT;

    spin_unlock(&pfs->lock);
    spin_unlock(&fslist_lock);
}

void
proclikefs_read_super(struct super_block *sb)
{
    struct proclikefs_file_system *pfs = (struct proclikefs_file_system *) (sb->s_type);
    atomic_inc(&pfs->nsuper);
    DEBUG("read_super for %s", pfs->fs.name);
    MOD_INC_USE_COUNT;
}

void
proclikefs_put_super(struct super_block *sb)
{
    struct proclikefs_file_system *pfs = (struct proclikefs_file_system *) (sb->s_type);
    atomic_dec(&pfs->nsuper);
    DEBUG("put_super for %s", pfs->fs.name);
    MOD_DEC_USE_COUNT;
    spin_lock(&fslist_lock);
    if (!pfs->live && atomic_read(&pfs->nsuper) == 0) {
	list_del(&pfs->fs_list);
	unregister_filesystem(&pfs->fs);
	kfree(pfs);
    }
    spin_unlock(&fslist_lock);
}

void
proclikefs_read_inode(struct inode *inode)
{
    struct proclikefs_file_system *pfs = (struct proclikefs_file_system *) (inode->i_sb->s_type);
    struct proclikefs_inode_info *inode_info = (struct proclikefs_inode_info *) (&inode->u);

    spin_lock(&pfs->lock);
    list_add(&inode_info->fsi_list, &pfs->i_list);
    spin_unlock(&pfs->lock);
}

void
proclikefs_delete_inode(struct inode *inode)
{
    struct proclikefs_file_system *pfs = (struct proclikefs_file_system *) (inode->i_sb->s_type);
    struct proclikefs_inode_info *inode_info = (struct proclikefs_inode_info *) (&inode->u);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 0)
    inode->i_state = I_CLEAR;
#endif
    
    spin_lock(&pfs->lock);
    list_del(&inode_info->fsi_list);
    spin_unlock(&pfs->lock);
}

int
init_module(void)
{
    proclikefs_null_super_operations.read_inode = proclikefs_read_inode;
    proclikefs_null_super_operations.delete_inode = proclikefs_delete_inode;
    proclikefs_null_super_operations.put_super = proclikefs_put_super;
    spin_lock_init(&fslist_lock);
    return 0;
}

void
cleanup_module(void)
{
    struct list_head *next;
    spin_lock(&fslist_lock);
    for (next = fs_list.next; next != &fs_list; ) {
	struct proclikefs_file_system *pfs = list_entry(next, struct proclikefs_file_system, fs_list);
	next = next->next;
	if (pfs->live || atomic_read(&pfs->nsuper) != 0)
	    printk("<1>proclikefs: unregistering active FS %s, prepare to die\n", pfs->name);
	unregister_filesystem(&pfs->fs);
	kfree(pfs);
    }
    spin_unlock(&fslist_lock);
}

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

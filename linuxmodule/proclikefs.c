/* -*- c-basic-offset: 4 -*- */
/*
 * proclikefs.c -- /proc-like file system infrastructure; allow file systems
 * to be unmounted even while active
 * Eddie Kohler
 *
 * Copyright (c) 2002-2003 International Computer Science Institute
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
#include <linux/config.h>
#ifndef EXPORT_SYMTAB
# define EXPORT_SYMTAB
#endif
#ifdef CONFIG_MODVERSIONS
# define MODVERSIONS
# include <linux/modversions.h>
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
# define INODE_INFO(inode)	((struct proclikefs_inode_info *)((inode)->u.generic_ip))
#else
# define INODE_INFO(inode)	((struct proclikefs_inode_info *)(&(inode)->u))
#endif

#ifndef MOD_DEC_USE_COUNT
# define MOD_DEC_USE_COUNT	module_put(THIS_MODULE)
#endif


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
static struct inode_operations proclikefs_null_root_inode_operations;

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

static struct dentry *
proclikefs_null_root_lookup(struct inode *dir, struct dentry *dentry)
{
    return (struct dentry *)(ERR_PTR(-ENOENT));
}

struct proclikefs_file_system *
proclikefs_register_filesystem(const char *name, int fs_flags,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	struct super_block *(*get_sb) (struct file_system_type *, int, const char *, void *),
#else
	struct super_block *(*read_super) (struct super_block *, void *, int),
#endif
	void (*reread_super) (struct super_block *))
{
    struct proclikefs_file_system *newfs = 0;
    struct list_head *next;
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

    spin_lock(&fslist_lock);
    
    for (next = fs_list.next; next != &fs_list; next = next->next) {
	newfs = list_entry(next, struct proclikefs_file_system, fs_list);
	if (strcmp(name, newfs->name) == 0) {
	    if (newfs->live > 0) { /* active filesystem with that name */
		spin_unlock(&fslist_lock);
		MOD_DEC_USE_COUNT;
		return 0;
	    } else
		break;
	}
    }

    if (!newfs) {
	newfs = kmalloc(sizeof(struct proclikefs_file_system) + strlen(name), GFP_KERNEL);
	if (!newfs) {		/* out of memory */
	    spin_unlock(&fslist_lock);
	    MOD_DEC_USE_COUNT;
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

    newfs->fs.fs_flags = fs_flags;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    newfs->fs.get_sb = get_sb;
    newfs->fs.kill_sb = kill_anon_super;
#else
    newfs->fs.read_super = read_super;
#endif
    newfs->live = 1;
    DEBUG("pfs[%p]: created filesystem %s", newfs, name);

    if (newfs_is_new) {
	int err = register_filesystem(&newfs->fs);
	if (err != 0)
	    printk("<1>proclikefs: error %d while initializing pfs[%p] (%s)\n", -err, newfs, name);
    } else if (reread_super) {
	/* transfer superblocks */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
	struct list_head *p;
	spin_lock(&sb_lock);
	for (p = newfs->fs.fs_supers.next; p != &newfs->fs.fs_supers; p = p->next) {
	    struct super_block *sb = list_entry(p, struct super_block, s_instances);
	    if (sb->s_type == &newfs->fs)
		(*reread_super)(sb);
	    else
		printk("<1>proclikefs: confusion\n");
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

    spin_unlock(&fslist_lock);
    return newfs;
}

static void
proclikefs_kill_super(struct super_block *sb, struct inode *dummy_inode)
{
    struct dentry *dentry_tree;
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
	    struct dentry *child = list_entry(next, struct dentry, d_child);
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
    struct super_block *sb;
    struct inode *inode, dummy_inode;
    struct file *filp;
    struct list_head *p;
    
    if (!pfs)
	return;

    DEBUG("unregister_filesystem entry");
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
    DEBUG("clearing inode operations");
    {
	struct list_head *next = pfs->i_list.next;
	while (next != &pfs->i_list) {
	    struct list_head *tmp = next;
	    next = next->next;
	    inode = list_entry(tmp, struct inode, u);
	    make_bad_inode(inode);
	}
    }
    INIT_LIST_HEAD(&pfs->i_list); /* we've made all the inodes bad */
    
    /* clear out superblock operations */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 10)
    DEBUG("clearing superblocks");
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
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
    pfs->fs.read_super = proclikefs_null_read_super;
#endif
    MOD_DEC_USE_COUNT;

    spin_unlock(&pfs->lock);
    spin_unlock(&fslist_lock);
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
    struct proclikefs_inode_info *inode_info = INODE_INFO(inode);

    DEBUG("pfs[%p]: add inode %p", pfs, inode);
    spin_lock(&pfs->lock);
    list_add(&inode_info->fsi_list, &pfs->i_list);
    spin_unlock(&pfs->lock);
}

void
proclikefs_delete_inode(struct inode *inode)
{
    struct proclikefs_file_system *pfs = (struct proclikefs_file_system *) (inode->i_sb->s_type);
    struct proclikefs_inode_info *inode_info = INODE_INFO(inode);

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
    proclikefs_null_root_inode_operations.lookup = proclikefs_null_root_lookup;
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

#ifdef MODULE_AUTHOR
MODULE_AUTHOR("Eddie Kohler <kohler@cs.ucla.edu>");
#endif
#ifdef MODULE_DESCRIPTION
MODULE_DESCRIPTION("Proclikefs: allow module unload of mounted filesystems");
#endif
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

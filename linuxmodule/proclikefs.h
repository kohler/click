/* -*- c-basic-offset: 4 -*- */
#ifndef PROCLIKEFS_H
#define PROCLIKEFS_H
#ifdef __cplusplus
extern "C" {
#endif
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/version.h>

struct proclikefs_file_system;

struct proclikefs_inode_info {
    struct list_head fsi_list;
};

struct proclikefs_file_system *proclikefs_register_filesystem
		(const char *name, int fs_flags,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
		 struct super_block *(*get_sb) (struct file_system_type *, int, const char *, void *),
#else
		 struct super_block *(*read_super) (struct super_block *, void *, int),
#endif
		 void (*reread_super) (struct super_block *));
void proclikefs_unregister_filesystem(struct proclikefs_file_system *);

void proclikefs_read_super(struct super_block *);
void proclikefs_put_super(struct super_block *);

void proclikefs_read_inode(struct inode *);
void proclikefs_delete_inode(struct inode *);

#ifdef __cplusplus
}
#endif
#endif


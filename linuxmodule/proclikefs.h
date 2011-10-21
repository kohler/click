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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 0, 0)
typedef struct dentry *(*proclikefs_mountfunc)(struct file_system_type *, int, const char *, void *);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
typedef int (*proclikefs_mountfunc)(struct file_system_type *, int, const char *, void *, struct vfsmount *);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
typedef struct super_block *(*proclikefs_mountfunc)(struct file_system_type *, int, const char *, void *);
#else
typedef struct super_block *(*proclikefs_mountfunc)(struct super_block *, void *, int);
#endif


struct proclikefs_file_system *proclikefs_register_filesystem
		(const char *name, int fs_flags,
		 proclikefs_mountfunc mountfunc);
void proclikefs_reinitialize_supers(struct proclikefs_file_system *,
		 void (*reread_super) (struct super_block *));
void proclikefs_unregister_filesystem(struct proclikefs_file_system *);

void proclikefs_read_super(struct super_block *);
void proclikefs_put_super(struct super_block *);

struct file_operations *proclikefs_new_file_operations(struct proclikefs_file_system *);
struct inode_operations *proclikefs_new_inode_operations(struct proclikefs_file_system *);

#ifdef __cplusplus
}
#endif
#endif

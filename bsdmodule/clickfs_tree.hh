#ifndef CLICKFS_TREE_HH
#define CLICKFS_TREE_HH

#define CLICKFS_DIRENT_NAMLEN	64

enum clickfs_dirent_type {
    CLICKFS_DIRENT_DIR = 0,
    CLICKFS_DIRENT_EHANDLE,
    CLICKFS_DIRENT_SYMLINK,
    CLICKFS_DIRENT_CONFIG
};

struct clickfs_dirent {
    enum clickfs_dirent_type type;
    char name[CLICKFS_DIRENT_NAMLEN];
    char lnk_name[CLICKFS_DIRENT_NAMLEN];
    int perm;
    void *param;
    struct clickfs_dirent *next;
    int refcnt;
};

struct clickfs_dir {
    struct clickfs_dirent *ent_head;
};

extern struct clickfs_dirent *clickfs_tree_root;

int clickfs_tree_init();
int clickfs_tree_cleanup();
struct clickfs_dir *clickfs_tree_add_dir(struct clickfs_dir *d, char *name, int perm);
int clickfs_tree_add_file(struct clickfs_dir *d, char *name, enum clickfs_dirent_type t, int perm, void *param);
int clickfs_tree_add_link(struct clickfs_dir *d, char *name, char *lnk_name);
int clickfs_tree_unlink(struct clickfs_dir *d, char *name);
struct clickfs_dir *clickfs_tree_rootdir();
int clickfs_tree_put_dirent(struct clickfs_dirent *de);
int clickfs_tree_ref_dirent(struct clickfs_dirent *de);

#endif

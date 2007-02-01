#ifndef CLICKFS_TREE_HH
#define CLICKFS_TREE_HH

#define CLICKFS_DIRENT_NAMLEN	64

MALLOC_DECLARE(M_CLICKFS);

enum clickfs_dirent_type {
    CLICKFS_DIRENT_DIR = 0,
    CLICKFS_DIRENT_HANDLE,
    CLICKFS_DIRENT_SYMLINK
};

struct clickfs_dirent {
    struct clickfs_dirent *next;
    enum clickfs_dirent_type type;
    char name[CLICKFS_DIRENT_NAMLEN];
    int perm;
    int fileno;				/* aka inode number */
    int file_refcnt;
    union {
	struct dir_struct {
	    struct clickfs_dirent *head;	/* Linked list of dir entries */
	    struct clickfs_dirent *parent;	/* Parent dir */
	} dir;
	struct handle_struct {
	    int eindex;
	    int handle;
	    CLICK_NAME(String) *wbuf;
	    off_t w_offset;
	    CLICK_NAME(String) *rbuf;
	    off_t r_offset;
	} handle;
	struct symlink_struct {
	    char name[CLICKFS_DIRENT_NAMLEN];
	} slink;
    } data;
};

extern struct clickfs_dirent *clickfs_tree_root;

void clickfs_tree_init();
void clickfs_tree_cleanup();
struct clickfs_dirent *clickfs_tree_add_dir(struct clickfs_dirent *cde,
					    char *name, int perm);
void clickfs_tree_add_link(struct clickfs_dirent *cde, char *name,
			  char *lnk_name);
void clickfs_tree_add_handle(struct clickfs_dirent *cde,
			     const CLICK_NAME(Handler) *h, int eindex, int handle);
void clickfs_tree_unlink(struct clickfs_dirent *cde, char *name);

#endif

#ifndef LINUXMODULE_HH
#define LINUXMODULE_HH
#include "element.hh"

extern "C" {
#define __NO_VERSION__
#define new linux_new
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#undef new
}

// definitions for portable proc filesystems
#define DECLARE_READ_FILEOP(name) ssize_t name(struct file *filp, char *buffer, size_t count, loff_t *store_f_pos)
#define DECLARE_WRITE_FILEOP(name) ssize_t name(struct file *filp, const char *buffer, size_t count, loff_t *store_f_pos)
#define DECLARE_RELEASE_FILEOP(name) int name(struct inode *, struct file *filp)
#define FILEOP_F_POS (*store_f_pos)

extern proc_dir_entry proc_click_entry;

void init_click_proc();
void cleanup_click_proc();

int click_register_pde(proc_dir_entry *, proc_dir_entry *);
int click_unregister_pde(proc_dir_entry *);

proc_dir_entry *click_find_pde(proc_dir_entry *, const String &);

proc_dir_entry *click_new_dynamic_pde();
int click_kill_dynamic_pde(proc_dir_entry *);

proc_dir_entry *click_register_new_dynamic_pde
	(proc_dir_entry *parent, const proc_dir_entry *pattern,
	 int namelen = -1, const char *name = 0, void *data = 0);

void init_proc_click_config();
void cleanup_proc_click_config();
void init_router_element_procs();
void cleanup_router_element_procs();
void init_proc_click_elements();
void cleanup_proc_click_elements();
void init_proc_click_errors();
void cleanup_proc_click_errors();

extern ErrorHandler *kernel_errh;
extern Router *current_router;
void initialize_router(const char *data, unsigned len);
void reset_proc_click_errors();

class KernelHandlerRegistry : public Element::HandlerRegistry {
  
  proc_dir_entry *_directory;
  
 public:
  
  KernelHandlerRegistry(proc_dir_entry *directory);
  
  int grow_handlers();
  
  void add_read_write(const char *, int, ReadHandler, void *,
		      WriteHandler, void *);
  
};

#endif

#ifndef MODULEPRIV_HH
#define MODULEPRIV_HH
#include "router.hh"
#include "clickpackage.hh"

extern "C" {
#define new linux_new
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#undef new
}

struct click_proc_dir_entry : public proc_dir_entry {
  struct inode *inode;
};
struct click_x_proc_dir_entry {
  // XXX I wish you could initialize a 'click_proc_dir_entry' directly!
  proc_dir_entry u;
  struct inode *inode;
};

extern click_proc_dir_entry *proc_click_entry;

void init_click_proc();
void cleanup_click_proc();

int click_register_pde(proc_dir_entry *, click_proc_dir_entry *);
int click_register_pde(proc_dir_entry *, click_x_proc_dir_entry *);
int click_unregister_pde(click_proc_dir_entry *);

click_proc_dir_entry *click_find_pde(proc_dir_entry *, const String &);

click_proc_dir_entry *click_new_dynamic_pde();
int click_kill_dynamic_pde(click_proc_dir_entry *);

click_proc_dir_entry *click_register_new_dynamic_pde
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
extern ErrorHandler *kernel_syslog_errh;
extern Router *current_router;
Router *parse_router(String);
void kill_current_router();
void install_current_router(Router *);
void reset_proc_click_errors();

extern atomic_t num_click_threads;
void start_click_sched(Router *, int, ErrorHandler *);
void init_click_sched();
int cleanup_click_sched();

#define ROOT_HANDLERS_CAP 16
extern Router::Handler *root_handlers;
extern int nroot_handlers;
void register_handler(proc_dir_entry *, int elementno, int handlerno);

#endif

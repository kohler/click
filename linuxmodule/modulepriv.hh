#ifndef MODULEPRIV_HH
#define MODULEPRIV_HH
#include <click/router.hh>
#include <click/package.hh>

extern "C" {
#define new linux_new
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
#undef new
}

extern proc_dir_entry *proc_click_entry;
extern int proc_click_mode_r, proc_click_mode_w, proc_click_mode_x;
extern int proc_click_mode_dir;

proc_dir_entry *click_find_pde(proc_dir_entry *, const String &);
void remove_proc_entry_recursive(proc_dir_entry *, proc_dir_entry *parent);

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
int start_click_sched(Router *, ErrorHandler *);
void kill_click_sched(Router *);
void init_click_sched();
int cleanup_click_sched();

#define ROOT_HANDLERS_CAP 16
extern Router::Handler *root_handlers;
extern int nroot_handlers;
void register_handler(proc_dir_entry *, int elementno, int handlerno);

#endif

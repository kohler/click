#ifndef MODULEPRIV_HH
#define MODULEPRIV_HH
#define WANT_MOD_USE_COUNT 1	/* glue.hh should use the actual macros */
#include <click/router.hh>
#include <click/driver.hh>
#include <click/pathvars.h>	/* for HAVE_CLICKFS */

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/poll.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
# define LINUX_2_2 1
#else
# define LINUX_2_4 1
#endif

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

#ifdef HAVE_CLICKFS
void init_clickfs();
void cleanup_clickfs();
#endif

extern ErrorHandler *kernel_errh;
extern ErrorHandler *kernel_syslog_errh;
extern Router *current_router;
Router *parse_router(String);
void kill_current_router();
int kill_current_router_threads();
void install_current_router(Router *);
void reset_proc_click_errors();

extern int click_thread_priority;
void init_click_sched();
int start_click_sched(Router *, int, ErrorHandler *);
int cleanup_click_sched();
void get_click_thread_pids(Vector<int> &);
void change_click_thread_priority(int);

void register_handler(proc_dir_entry *, int handlerno);

#endif

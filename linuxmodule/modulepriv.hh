#ifndef MODULEPRIV_HH
#define MODULEPRIV_HH
#define WANT_MOD_USE_COUNT 1	/* glue.hh should use the actual macros */
#include <click/router.hh>
#include <click/driver.hh>
#include <click/pathvars.h>	/* for HAVE_CLICKFS */
#ifndef HAVE_CLICKFS
# define HAVE_PROC_CLICK 1	/* if not clickfs, then /proc/click */
#endif

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#ifdef HAVE_PROC_CLICK
# include <linux/proc_fs.h>
#endif
#include <asm/uaccess.h>
#include <linux/poll.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
# define LINUX_2_2 1
#else
# define LINUX_2_4 1
#endif

#if 0
# define MDEBUG(args...) do { printk("<1>kclick: " args); printk("\n"); } while (0)
#else
# define MDEBUG(args...) /* nada */
#endif

#define HANDLER_REREAD			(Router::Handler::FIRST_USER_FLAG)
#define HANDLER_NEED_READ		(Router::Handler::FIRST_USER_FLAG << 1)
#define HANDLER_SPECIAL_INODE		(Router::Handler::FIRST_USER_FLAG << 2)
#define HANDLER_WRITE_UNLIMITED		(Router::Handler::FIRST_USER_FLAG << 3)

extern ErrorHandler *click_logged_errh;
void click_clear_error_log();

void click_init_config();
void click_cleanup_config();

extern Router *click_router;
int click_kill_router_threads();

void click_init_sched();
int click_cleanup_sched();
int click_start_sched(Router *, int, ErrorHandler *);

extern int click_mode_r, click_mode_w, click_mode_x, click_mode_dir;

#ifdef HAVE_CLICKFS
int init_clickfs();
void cleanup_clickfs();
#endif

#ifdef HAVE_PROC_CLICK
int init_proc_click();
void cleanup_proc_click();
void init_router_element_procs();
void cleanup_router_element_procs();
#endif

#endif

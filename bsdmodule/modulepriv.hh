#ifndef MODULEPRIV_HH
#define MODULEPRIV_HH

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/vnode.h>
CLICK_CXX_UNPROTECT
#include <click/cxxunprotect.h>

#include <click/router.hh>
#include <click/package.hh>
#include <click/driver.hh>

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

extern int click_thread_priority;
void click_init_sched();
int click_cleanup_sched();
int click_start_sched(Router *, int, ErrorHandler *);

void init_router_element_procs();
void cleanup_router_element_procs();

extern struct vfsops clickfs_vfsops;
extern struct vnodeopv_desc clickfs_vnodeop_opv_desc;
extern String *current_config;

int clickfs_rootvnode(struct mount *, struct vnode **);

#endif

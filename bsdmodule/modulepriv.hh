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

extern ErrorHandler *kernel_errh;
extern ErrorHandler *kernel_syslog_errh;
extern Router *current_router;
Router *parse_router(String);
void kill_current_router();
void install_current_router(Router *);

extern struct simplelock click_thread_spinlock;
extern Vector<int> *click_thread_pids;
extern int click_thread_priority;
void init_click_sched();
int start_click_sched(Router *, int, ErrorHandler *);
int cleanup_click_sched();

void init_router_element_procs();
void cleanup_router_element_procs();

extern struct vfsops clickfs_vfsops;
extern struct vnodeopv_desc clickfs_vnodeop_opv_desc;
extern String *current_config;

int clickfs_rootvnode(struct mount *, struct vnode **);

#endif

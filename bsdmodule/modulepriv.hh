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

#define WANT_MOD_USE_COUNT 1    /* glue.hh should use the actual macros */
#include <click/router.hh>
#include <click/package.hh>
#include <click/driver.hh>
#include <click/error.hh>

#define HANDLER_REREAD                  (Handler::h_driver_flag_0)
#define HANDLER_WRITE_UNLIMITED         (Handler::h_driver_flag_1)

CLICK_DECLS

class KernelErrorHandler : public ErrorHandler { public:

    KernelErrorHandler()		: _pos(0), _generation(0) { }
    void *emit(const String &str, void *user_data, bool more);
    void account(int level);
    void clear_log()			{ _pos = 0; _generation += 2; }
    inline String stable_string() const;

 private:

  enum { LOGBUF_SIZ = 4096, LOGBUF_SAVESIZ = 2048 };
  char _logbuf[LOGBUF_SIZ];
  int _pos;
  unsigned _generation;
  void log_line(const char *begin, const char *end);

};

extern KernelErrorHandler *click_logged_errh;

CLICK_ENDDECLS

void click_clear_error_log();

void click_init_config();
void click_cleanup_config();

extern CLICK_NAME(Master) *click_master;
extern CLICK_NAME(Router) *click_router;
int click_kill_router_threads();

void click_init_sched(CLICK_NAME(ErrorHandler) *);
int click_cleanup_sched();

void init_router_element_procs();
void cleanup_router_element_procs();

extern struct vfsops clickfs_vfsops;
extern struct vnodeopv_desc clickfs_vnodeop_opv_desc;

int clickfs_rootvnode(struct mount *, struct vnode **);

#endif

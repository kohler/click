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

#define HANDLER_REREAD                  (Handler::DRIVER_FLAG_0)
#define HANDLER_NEED_READ               (Handler::DRIVER_FLAG_1)
#define HANDLER_SPECIAL_INODE           (Handler::DRIVER_FLAG_2)
#define HANDLER_WRITE_UNLIMITED         (Handler::DRIVER_FLAG_3)


class KernelErrorHandler : public BaseErrorHandler { public:

  KernelErrorHandler()                  : _pos(0), _generation(0) { }
  void handle_text(Seriousness, const String &);
  void clear_log()                      { _pos = 0; _generation += 2; }
  inline String stable_string() const;

 private:

  enum { LOGBUF_SIZ = 4096, LOGBUF_SAVESIZ = 2048 };
  char _logbuf[LOGBUF_SIZ];
  int _pos;
  unsigned _generation;
  void log_line(const char *begin, const char *end);

};

extern KernelErrorHandler *click_logged_errh;
void click_clear_error_log();

void click_init_config();
void click_cleanup_config();

extern Master *click_master;
extern Router *click_router;
int click_kill_router_threads();

void click_init_sched(ErrorHandler *);
int click_cleanup_sched();

void init_router_element_procs();
void cleanup_router_element_procs();

extern struct vfsops clickfs_vfsops;
extern struct vnodeopv_desc clickfs_vnodeop_opv_desc;

int clickfs_rootvnode(struct mount *, struct vnode **);

#endif

#ifndef MODULEPRIV_HH
#define MODULEPRIV_HH
#define WANT_MOD_USE_COUNT 1	/* glue.hh should use the actual macros */
#include <click/router.hh>
#include <click/driver.hh>
#include <click/error.hh>

#include <click/cxxprotect.h>
CLICK_CXX_PROTECT
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

#define HANDLER_REREAD			(Router::Handler::DRIVER_FLAG_0)
#define HANDLER_NEED_READ		(Router::Handler::DRIVER_FLAG_1)
#define HANDLER_SPECIAL_INODE		(Router::Handler::DRIVER_FLAG_2)
#define HANDLER_WRITE_UNLIMITED		(Router::Handler::DRIVER_FLAG_3)


class KernelErrorHandler : public BaseErrorHandler { public:
  
  KernelErrorHandler()			: _pos(0), _generation(0) { }
  void handle_text(Seriousness, const String &);
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
void click_clear_error_log();

void click_init_config();
void click_cleanup_config();

extern Master *click_master;
extern Router *click_router;

void click_init_sched(ErrorHandler *);
int click_cleanup_sched();

extern int click_mode_r, click_mode_w, click_mode_x, click_mode_dir;

int init_clickfs();
void cleanup_clickfs();

#endif

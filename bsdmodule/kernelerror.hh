#ifndef KERNELERROR_HH
#define KERNELERROR_HH
#include <click/error.hh>
CLICK_DECLS

class KernelErrorHandler : public BaseErrorHandler { public:
  KernelErrorHandler()			{ }
  void handle_text(Seriousness, const String &);
};

class SyslogErrorHandler : public BaseErrorHandler { public:
  SyslogErrorHandler()			{ }
  void handle_text(Seriousness, const String &);
};

CLICK_ENDDECLS
#endif

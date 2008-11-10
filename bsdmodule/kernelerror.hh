#ifndef KERNELERROR_HH
#define KERNELERROR_HH
#include <click/error.hh>
CLICK_DECLS

class KernelErrorHandler : public ErrorHandler { public:

    KernelErrorHandler() { }

    void *emit(const String &str, void *user_data, bool more);
    void account(int level);

};

CLICK_ENDDECLS
#endif

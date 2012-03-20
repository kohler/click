#include <click/config.h>
#include "msqueue.hh"
#include <click/error.hh>
CLICK_DECLS

#define PREFETCH    1

MSQueue::MSQueue()
{
}

void *
MSQueue::cast(const char *n)
{
    if (strcmp(n, "MSQueue") == 0)
	return (Element *)this;
    else
	return ThreadSafeQueue::cast(n);
}

int
MSQueue::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int r = ThreadSafeQueue::configure(conf, errh);
    if (r >= 0)
	errh->warning("MSQueue is deprecated, use ThreadSafeQueue instead");
    return r;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(multithread)
EXPORT_ELEMENT(MSQueue)

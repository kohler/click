
#ifndef XOKREADER_HH
#define XOKREADER_HH
#include "element.hh"

extern "C" {
#include <vos/net/fast_eth.h>
}

/*
 * =c
 * xokReader(pktring_size, off0a/val0a off0b/val0b ..., ..., offNa/valNa)
 * =d
 * Read network packets from xok by inserting DPF filters. Configuration
 * string specifies the size of the packet ring and filter to insert.
 * Configuration strings for filters are similar to the configuration strings
 * in the Classifier element, but the expression "-" does not work. See
 * Classifier for expression format. There is an upper limit of 16 filters per
 * xokReader.
 *
 * The xokReader element installs a separate dpf filter for each expression
 * given, all using the same packet ring.
 *
 * =e
 * For example, 
 *
 *   xokReader(32, 12/0806 14/99)
 * 
 * says that the reader should keep a ring size of 32. output 0 should get
 * packets that have 0x0806 at byte 12 and 0x99 at byte 14.
 *
 * =a
 * Classifier, xokWriter
 */

#define MAX_DPF_FILTERS 16

class xokReader : public Element {
  int dpf_ids[MAX_DPF_FILTERS];
  int fd;

 public:
  
  xokReader();
  xokReader(const xokReader &);
  ~xokReader();
  
  const char *class_name() const		{ return "xokReader"; }
  const char *processing() const		{ return PUSH; }
  
  xokReader *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  void selected(int fd);
};

#endif


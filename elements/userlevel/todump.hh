#ifndef TODUMP_HH
#define TODUMP_HH
#include <click/timer.hh>
#include <click/element.hh>
#include <stdio.h>

/*
 * =c
 *
 * ToDump(FILENAME [, SNAPLEN, ENCAPTYPE])
 *
 * =s sinks
 *
 * writes packets to a tcpdump(1) file
 *
 * =d
 *
 * Writes incoming packets to FILENAME in `tcpdump -w' format. This file
 * can be read `tcpdump -r', or by FromDump on a later run.
 *
 * Writes at most SNAPLEN bytes of each packet to the file. The default
 * SNAPLEN is 2000. ENCAPTYPE specifies the first header each packet is
 * expected to have. This information is stored in the file header, and must
 * be correct or tcpdump won't be able to read the file correctly. It can be
 * `C<IP>' or `C<ETHER>'; default is C<ETHER>.
 *
 * This element is only available at user level.
 *
 * =a
 *
 * FromDump, FromDevice.u, ToDevice.u, tcpdump(1) */

class ToDump : public Element {
  
  String _filename;
  FILE *_fp;
  unsigned _snaplen;
  unsigned _encap_type;
  
  void write_packet(Packet *);
  
 public:
  
  ToDump();
  ~ToDump();
  
  const char *class_name() const		{ return "ToDump"; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flags() const			{ return "S2"; }
  ToDump *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void push(int, Packet *);
  void run_scheduled();
  
};

#endif

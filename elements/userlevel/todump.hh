#ifndef TODUMP_HH
#define TODUMP_HH
#include <click/timer.hh>
#include <click/element.hh>
#include <click/task.hh>
#include <stdio.h>

/*
=c

ToDump(FILENAME [, SNAPLEN, ENCAP, I<KEYWORDS>])

=s sinks

writes packets to a tcpdump(1) file

=d

Writes incoming packets to FILENAME in `tcpdump -w' format. This file can be
read `tcpdump -r', or by FromDump on a later run. FILENAME can be `-', in
which case ToDump writes to the standard output.

Writes at most SNAPLEN bytes of each packet to the file. The default SNAPLEN
is 2000. ENCAP specifies the first header each packet is expected to have.
This information is stored in the file header, and must be correct or tcpdump
won't be able to read the file correctly. It can be `C<IP>' or `C<ETHER>';
default is `C<ETHER>'.

Keyword arguments are:

=over 8

=item SNAPLEN

Integer. Same as the SNAPLEN argument.

=item ENCAP

Either `C<IP>' or `C<ETHER>'. Same as the ENCAP argument.

=back

This element is only available at user level.

=a

FromDump, FromDevice.u, ToDevice.u, tcpdump(1) */

class ToDump : public Element { public:
  
  ToDump();
  ~ToDump();
  
  const char *class_name() const		{ return "ToDump"; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flags() const			{ return "S2"; }
  ToDump *clone() const;
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

  void push(int, Packet *);
  void run_scheduled();

 private:
  
  String _filename;
  FILE *_fp;
  unsigned _snaplen;
  unsigned _encap_type;
  bool _active;
  Task _task;
  
  void write_packet(Packet *);
  
};

#endif

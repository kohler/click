#ifndef FROMDUMP_HH
#define FROMDUMP_HH
#include <click/element.hh>
#include <click/task.hh>

/*
 * =c
 * FromDump(FILENAME [, TIMING])
 * =s sources
 * reads packets from a tcpdump(1) file
 * =d
 *
 * Reads packets from a file produced by `tcpdump -w FILENAME' or ToDump.
 * Pushes them out the output, and stops the driver when there are no more
 * packets. If TIMING is true, then FromDump tries to maintain the timing of
 * the original packet stream. TIMING is true by default.
 *
 * By default, `tcpdump -w FILENAME' dumps only the first 68 bytes of
 * each packet. You probably want to run `tcpdump -w FILENAME -s 2000' or some
 * such.
 *
 * Only available in user-level processes.
 *
 * =a ToDump, FromDevice.u, ToDevice.u, tcpdump(1) */

class FromDump : public Element { public:

  FromDump();
  ~FromDump();

  const char *class_name() const		{ return "FromDump"; }
  const char *processing() const		{ return PUSH; }
  FromDump *clone() const			{ return new FromDump; }
  
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();

  void run_scheduled();
  
 private:
  
  String _filename;
  FILE *_fp;
  bool _timing;
  bool _swapped;
    bool _stop;
  int _minor_version;
  int _linktype;
  
  Packet *_packet;
  Task _task;
  
  struct timeval _time_offset;

  WritablePacket *read_packet(ErrorHandler *);
  
};

#endif

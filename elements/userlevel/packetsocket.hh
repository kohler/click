#ifndef PACKETSOCKET_HH
#define PACKETSOCKET_HH

/*
 * =c
 * PacketSocket(DEVNAME)
 * =d
 * User-level only.  In Linux, is a replacement for To/FromBPF.  Reads
 * and writes packets from and to the network device named by
 * DEVNAME.  PacketSocket is always promiscuous.
 *
 * PacketSocket produces and expects Ethernet packets.
 * 
 * =e
 * = ... -> PacketSocket(eth0) -> ...
 *
 * =a ToBPF 
 * =a FromBPF */

#include "element.hh"

class PacketSocket : public Element {
 public:
  PacketSocket();
  ~PacketSocket();
  
  const char *class_name() const		{ return "PacketSocket"; }
  const char *processing() const	{ return PULL_TO_PUSH; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  PacketSocket *clone() const;

  int select_fd() { return(_fd); }
  void selected(int fd);

  void push(int port, Packet *);
  void run_scheduled();

 private:
  String _dev;
  int _fd;
  int _ifindex;
};

#endif

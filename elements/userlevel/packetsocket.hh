#ifndef PACKETSOCKET_HH
#define PACKETSOCKET_HH

/*
 * =c
 * PacketSocket(DEVNAME, PROMISC?)
 * =d
 * User-level only.  In Linux, is a replacement for To/FromBPF.  Reads
 * and writes packets from and to the network device named by
 * DEVNAME.  PacketSocket is promiscuous if PROMISC? is true.
 *
 * PacketSocket produces and expects Ethernet packets.
 *
 * This element is only available at user level.  The same cautions
 * about forwarding apply as in FromBPF.
 * 
 * =e
 * = ... -> PacketSocket(eth0, 0) -> ...
 *
 * =a ToBPF 
 * =a FromBPF */

#include "element.hh"

class PacketSocket : public Element {
 public:
  PacketSocket();
  ~PacketSocket();

  const char *class_name() const		{ return "PacketSocket"; }
  PacketSocket *clone() const;

#ifdef __linux__  
  const char *processing() const	{ return PULL_TO_PUSH; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void selected(int fd);

  void push(int port, Packet *);
  void run_scheduled();

 private:
  String _dev;
  int _fd;
  int _ifindex;
  bool _promisc;
#endif
};

#endif

#ifndef TUN_HH
#define TUN_HH

/*
 * =c
 * Tun(near-address, far-address)
 * =d
 * Reads and writes packets from/to a /dev/tun* device.
 * This allows a user-level Click to hand packets to the
 * ordinary kernel IP packet processing code.
 * A Tun will also transfer packets from the kernel IP
 * code if the kernel routing table has entries pointing
 * at the tun device.
 *
 * Tun produces and expects IP packets.
 * 
 * Tun allocates a /dev/tun device (this might fail) and
 * runs ifconfig to set the interface's local (ie kernel)
 * address to near-address and the distant (ie Click)
 * address to far-address.
 *
 * =a ToLinux
 */

#include "element.hh"
#include "ipaddress.hh"

class Tun : public Element {
 public:
  Tun();
  ~Tun();
  
  const char *class_name() const		{ return "Tun"; }
  const char *processing() const	{ return PULL_TO_PUSH; }
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  Tun *clone() const;

  int select_fd() { return(_fd); }
  void selected(int fd);

  void push(int port, Packet *);
  void run_scheduled();

 private:
  String _dev;
  IPAddress _near;
  IPAddress _far;
  int _fd;

  int alloc_tun(struct in_addr near, struct in_addr far, ErrorHandler *errh);
};

#endif

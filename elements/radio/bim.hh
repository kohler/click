#ifndef BIM_HH
#define BIM_HH

/*
 * BIM(/dev/cuaa0, speed)
 *
 * Read and write packets from/to ABACOM BIM-4xx-RS232 radio.
 * Takes care of low-level framing.
 * Pulls *and* pushes packets.
 */

#include <click/element.hh>

class BIM : public Element {
 public:
  BIM();
  ~BIM();
  
  const char *class_name() const	{ return "BIM"; }
  const char *processing() const	{ return PULL_TO_PUSH; }
  
  BIM *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void selected(int fd);

  void push(int port, Packet *);
  void run_scheduled();

 private:
  String _dev;
  int _speed;
  int _fd;

  /* turn bytes from the radio into frames */
  void got_char(int c);
  char _buf[2048];
  int _len;
  int _started;
  int _escaped;

  void send_packet(const unsigned char buf[], unsigned int len);
};

#endif

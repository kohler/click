#ifndef DELAYSHAPER_HH
#define DELAYSHAPER_HH
#include <click/element.hh>
#include <click/task.hh>

/*

DelayShaper(DELAY)

=s packet scheduling

shapes traffic to meet delay requirements

=d

Pulls packets from input port. queues packet if current timestamp minus packet
timestamp is less than DELAY seconds (microsecond precision). otherwise return
packet on output.

SetTimestamp element can be used to stamp the packet.

=a BandwidthShaper, DelayUnqueue, SetTimestamp */

class DelayShaper : public Element { public:
  
  DelayShaper();
  ~DelayShaper();

  const char *class_name() const	{ return "DelayShaper"; }
  const char *processing() const	{ return PULL; }
  DelayShaper *clone() const		{ return new DelayShaper; }

  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void add_handlers();
  static String read_param(Element *e, void *);

  Packet* pull(int);

 private:

  uint32_t _delay;
  Packet *_p;
};

#endif

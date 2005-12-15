#ifndef CLICK_PACKETSTORE_HH
#define CLICK_PACKETSTORE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/dequeue.hh>
#include <click/notifier.hh>
#include <clicknet/wifi.h>

CLICK_DECLS

/*
 * =c
 * PacketStore(I<KEYWORKDS>)
 * 
 * =s Wifi
 * Log transmit feedback stats for later analysis.
 * 
 * =d 
 * PacketStore records the size, timestamp, and other infor for
 * each packet that passed through.  The list of
 * recorded data can be dumped (and cleared) by repeated calls to the
 * read handler 'log'.
 *
 * =h log read-only
 * Print as much of the list of logged packets as possible, clearing 
 * printed packets from the log.
 * =h more read-only
 * Returns how many entries are left to be read.
 * =h reset write-only
 * Clears the log.
 *
 */

class PacketStore : public Element { public:
  
  PacketStore();
  ~PacketStore();
  
  const char *class_name() const		{ return "PacketStore"; }
  const char *port_count() const		{ return PORTS_1_1; }
    const char* processing() const		{ return PUSH_TO_PULL; }
  const char *flow_code() const			{ return "#/#"; }
  void *cast(const char *);

  void push(int port, Packet *);
  Packet *pull(int);

  int configure(Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }

  void add_handlers();

  
  DEQueue <Packet *> _packets;

  bool _active;
  ActiveNotifier _empty_note;
};

CLICK_ENDDECLS
#endif

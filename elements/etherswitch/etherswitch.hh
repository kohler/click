#ifndef ETHERSWITCH_HH
#define ETHERSWITCH_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>

class EtherSwitch : public Element {
  
 public:
  
  EtherSwitch();
  ~EtherSwitch();
  
  const char *class_name() const		{ return "EtherSwitch"; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/[^#]"; }
  
  EtherSwitch *clone() const;
  void notify_ninputs(int);

  void push(int port, Packet* p);

  static String read_table(Element* f, void *);
  void add_handlers();

  void set_timeout(int seconds)			{ _timeout = seconds; }

  struct AddrInfo {
    int port;
    timeval stamp;
    AddrInfo(int p, const timeval& s);
  };

private:

  typedef HashMap<EtherAddress, AddrInfo *> Table;
  Table _table;
  int _timeout;
  
  void broadcast(int source, Packet*);
};

#endif

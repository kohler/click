#ifndef SWITCH_HH
#define SWITCH_HH
#include "unlimelement.hh"
#include "etheraddress.hh"
#include "hashmap.hh"

class EtherSwitch : public UnlimitedElement {
  
 public:
  
  EtherSwitch();
  ~EtherSwitch();
  EtherSwitch *clone() const;
  
  const char *class_name() const		{ return "EtherSwitch"; }
  Processing default_processing() const	{ return PUSH; }
  bool unlimited_inputs() const			{ return true; }
  bool unlimited_outputs() const		{ return true; }
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
  void push(int port, Packet* p);

  static String read_table(Element* f, void *);
  void add_handlers(HandlerRegistry *);

  void set_timeout(int seconds)			{ _timeout = seconds; }

  struct AddrInfo {
    int port;
    timeval stamp;
    AddrInfo(int p, const timeval& s);
  };

private:
  HashMap<EtherAddress,AddrInfo*> _table;
  int _timeout;
  
  void broadcast(int source, Packet*);
};

#endif

#ifndef SWITCH_HH
#define SWITCH_HH
#include "unlimelement.hh"
#include "etheraddress.hh"
#include "hashmap.hh"

class EtherSwitch : public Element {
  
 public:
  
  EtherSwitch();
  ~EtherSwitch();
  EtherSwitch *clone() const;
  
  const char *class_name() const		{ return "EtherSwitch"; }
  const char *processing() const		{ return "h/h"; }
  void notify_ninputs(int);
  void notify_noutputs(int);
  Bitvector forward_flow(int) const;
  Bitvector backward_flow(int) const;
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
  HashMap<EtherAddress,AddrInfo*> _table;
  int _timeout;
  
  void broadcast(int source, Packet*);
};

#endif

#ifndef GRIDSRFW
#define GRIDSRFW

#include <click/element.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

class GridSRForwarder : public Element {

public:
  GridSRForwarder() CLICK_COLD;
  ~GridSRForwarder() CLICK_COLD;

  const char *class_name() const { return "GridSRForwarder"; }
  void *cast(const char *);
  const char *port_count() const { return "1/2"; }
  const char *processing() const { return "h/h"; }
  const char *flow_code() const { return "x/x"; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  int initialize(ErrorHandler *) CLICK_COLD;

  void push(int port, Packet *);

private:
  IPAddress _ip;

  void handle_host(Packet *p);
};

CLICK_ENDDECLS
#endif

#ifndef GRIDSRFW
#define GRIDSRFW

#include <click/element.hh>
#include <click/ipaddress.hh>

class GridSRForwarder : public Element {

public:
  GridSRForwarder();
  ~GridSRForwarder();

  const char *class_name() const { return "GridSRForwarder"; }
  void *cast(const char *);
  const char *processing() const { return "h/h"; }
  const char *flow_code() const { return "x/x"; }
  GridSRForwarder *clone() const { return new GridSRForwarder; }
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void push(int port, Packet *);
  
private:
  IPAddress _ip;

  void handle_host(Packet *p);
};

#endif

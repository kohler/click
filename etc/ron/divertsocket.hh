#ifndef DIVERTSOCKET_HH
#define DIVERTSOCKET_HH

/*
=title DivertSocket

=c

DivertSocket(PROTOCOL, SADDR/MASK, SPORTLOW, SPORTHIGH, DADDR/MASK, DPORTLOW, DPORTHIGH, [DIRECTION])

=s sources

=d 

DivertSocket sets up a firewall rule according to the input 
parameters, and diverts matching IP packets to it's output port.

DIRECTION can be either "in" or "out" for packets coming into this machine
or going out of this machine.

*/


#include <click/element.hh>


class DivertSocket : public Element {

public:

  DivertSocket();
  ~DivertSocket();

  const char *class_name() const     { return "DivertSocket"; }
  const char *processing() const     {return PUSH;}

  DivertSocket *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  void selected(int fd);


private:

  int _fd;
  
};

#endif


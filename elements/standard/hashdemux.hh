#ifndef HASHDEMUX_HH
#define HASHDEMUX_HH
#include <click/element.hh>

/*
 * =c
 * HashDemux(OFFSET, LENGTH)
 * =s classification
 * old name for HashSwitch
 * =d
 * This is the old name for the HashSwitch element. You should use HashSwitch
 * instead.
 * =a HashSwitch */

class HashDemux : public Element {

  int _offset;
  int _length;
  
 public:
  
  HashDemux();
  ~HashDemux();
  
  const char *class_name() const		{ return "HashDemux"; }
  const char *processing() const		{ return PUSH; }
  void notify_noutputs(int);
  
  HashDemux *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int port, Packet *);
  
};

#endif

#ifndef HASHDEMUX_HH
#define HASHDEMUX_HH
#include "unlimelement.hh"

/*
 * =c
 * HashDemux(offset, length)
 * =d
 * Can have any number of outputs.
 * Chooses the output on which to emit each packet based on
 * a hash of the bytes at the indicated offset.
 * Could be used for stochastic fair queuing.
 * =e
 * This element expects IP packets and chooses the output
 * based on a hash of the IP destination address:
 * 
 * = HashDemux(16, 4)
 */

class HashDemux : public UnlimitedElement {

  int _offset;
  int _length;
  
 public:
  
  HashDemux();
  
  const char *class_name() const		{ return "HashDemux"; }
  const char *processing() const	{ return PUSH; }
  bool unlimited_outputs() const		{ return true; }
  
  HashDemux *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  
  void push(int port, Packet *);
  
};

#endif

#ifndef MAPPINGCREATOR_HH
#define MAPPINGCREATOR_HH

#include "rewriter2.hh"

/*
 * =c
 * MappingCreator()
 * =d
 * Basic round-robin mapping creator for Rewriter.
 *
 * Every Rewriter with more than one pattern must be followed by exactly one
 * MappingCreator on its output 0.
 * When a packet appears on the MappingCreator's single input, the
 * MappingCreator communicates out-of-band with its upstream Rewriter
 * to establish a mapping between the packet's connection (source/destination
 * pair) and one of the Rewriter's patterns, chosen in round-robin order.
 * It then pushes the packet out on its single output, which should be
 * connected to the Rewriter's input 0.
 * 
 * More intelligent MappingCreators (i.e., policies better than round-robin)
 * may be implemented by subclassing MappingCreator.
 *
 * =a Rewriter 
 */

class MappingCreator : public Element {
  
  Rewriter *_rw;
  int _npats;
  int _curpat;
  
 public:

  MappingCreator() : Element(1, 1), _rw(NULL)	{ }
  
  const char *class_name() const		{ return "MappingCreator"; }
  Processing default_processing() const		{ return PUSH; }
  
  MappingCreator *clone() const			{ return new MappingCreator; }

  virtual int initialize(ErrorHandler *);
  
  virtual void push(int port, Packet *);
};

#endif MAPPINGCREATOR_HH

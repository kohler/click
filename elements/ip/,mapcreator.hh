#ifndef MAPPINGCREATOR_HH
#define MAPPINGCREATOR_HH
#include "element.hh"
#include "rewriter.hh"

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
  
  class RWInfo {
  public:
    Rewriter *_rw;
    int _npat;
    int _cpat;

    RWInfo() : _rw(NULL), _npat(0), _cpat(0) 	{ }
    RWInfo(Rewriter *rw, int np, int cp);
    ~RWInfo();
  };

  Vector<RWInfo> _rwi;
  HashMap <IPFlowID, IPFlowID> _eqmap;
  HashMap <IPFlowID, IPFlowID> _rwmap;
  
 public:

  MappingCreator() : _eqmap(), _rwmap()		{ }
  ~MappingCreator()				{ }
  
  const char *class_name() const		{ return "MappingCreator"; }
  const char *processing() const		{ return PUSH; }
  void notify_ninputs(int);
  
  MappingCreator *clone() const			{ return new MappingCreator; }

  virtual int initialize(ErrorHandler *);
  void add_handlers();
  static int equiv_handler(const String &s, Element *e, void *thunk,
			   ErrorHandler *errh);

  virtual void push(int port, Packet *);
};

#endif MAPPINGCREATOR_HH

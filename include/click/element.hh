// -*- c-basic-offset: 2; related-file-name: "../../lib/element.cc" -*-
#ifndef CLICK_ELEMENT_HH
#define CLICK_ELEMENT_HH
#include <click/glue.hh>
#include <click/vector.hh>
#include <click/string.hh>
#include <click/packet.hh>
class Router;
class Task;
class Element;
class ErrorHandler;
class Bitvector;

// #define CLICK_STATS 5

typedef String (*ReadHandler)(Element *, void *);
typedef int (*WriteHandler)(const String &, Element *, void *, ErrorHandler *);

class Element { public:
  
  static const char * const AGNOSTIC, * const PUSH, * const PULL;
  static const char * const PUSH_TO_PULL, * const PULL_TO_PUSH;
  enum Processing { VAGNOSTIC, VPUSH, VPULL };

  static const char * const COMPLETE_FLOW;

  enum ConfigurePhase {
    CONFIGURE_PHASE_FIRST = 0,
    CONFIGURE_PHASE_INFO = 20,
    CONFIGURE_PHASE_DEFAULT = 100,
    CONFIGURE_PHASE_LAST = 2000
  };
  
  class Port;
  
  Element();
  Element(int ninputs, int noutputs);
  virtual ~Element();
  static int nelements_allocated;

  // CHARACTERISTICS
  virtual const char *class_name() const = 0;
  virtual void *cast(const char *);
  virtual Element *clone() const = 0;
  
  String id() const;
  String declaration() const;
  String landmark() const;
  
  Router *router() const			{ return _router; }
  int eindex() const				{ return _eindex; }
  int eindex(Router *) const;

  // INPUTS AND OUTPUTS
  int ninputs() const				{ return _ninputs; }
  int noutputs() const				{ return _noutputs; }
  void set_ninputs(int);
  void set_noutputs(int);
  void add_input()				{ set_ninputs(ninputs()+1); }
  void add_output()				{ set_noutputs(noutputs()+1); }
  
  const Port &input(int) const;
  const Port &output(int) const;
  bool input_is_pull(int) const;
  bool output_is_push(int) const;
  
  void checked_output_push(int, Packet *) const;

  // PROCESSING, FLOW, AND FLAGS
  virtual const char *processing() const;
  virtual const char *flow_code() const;
  virtual const char *flags() const;
  
  // CONFIGURATION AND INITIALIZATION
  virtual void notify_ninputs(int);
  virtual void notify_noutputs(int);
  virtual int configure_phase() const;
  virtual int configure(const Vector<String> &, ErrorHandler *);
  virtual int initialize(ErrorHandler *);
  virtual void uninitialize();

  // LIVE RECONFIGURATION
  virtual void configuration(Vector<String> &, bool *) const;
  void configuration(Vector<String> &v) const	{ configuration(v, 0); }
  String configuration() const;
  
  virtual bool can_live_reconfigure() const;
  virtual int live_reconfigure(const Vector<String> &, ErrorHandler *);
  virtual void take_state(Element *, ErrorHandler *);

  // HANDLERS
  virtual void add_handlers();
  
  void add_read_handler(const String &, ReadHandler, void *);
  void add_write_handler(const String &, WriteHandler, void *);
  void add_task_handlers(Task *, const String &prefix = String());
  
  static String read_positional_handler(Element *, void *);
  static String read_keyword_handler(Element *, void *);
  static int reconfigure_positional_handler(const String &, Element *, void *, ErrorHandler *);
  static int reconfigure_keyword_handler(const String &, Element *, void *, ErrorHandler *);
  static int reconfigure_positional_handler_2(const String &, Element *, void *, ErrorHandler *);
  static int reconfigure_keyword_handler_2(const String &, Element *, void *, ErrorHandler *);
  
  virtual int llrpc(unsigned command, void *arg);
  virtual int local_llrpc(unsigned command, void *arg);
  
  // RUNTIME
  virtual void push(int port, Packet *);
  virtual Packet *pull(int port);
  virtual Packet *simple_action(Packet *);
  
  virtual void run_scheduled();
  
#if CLICK_USERLEVEL
  enum { SELECT_READ = 1, SELECT_WRITE = 2 };
  int add_select(int fd, int mask) const;
  int remove_select(int fd, int mask) const;
  virtual void selected(int fd);
#endif

  // METHODS USED BY `ROUTER'
  void attach_router(Router *r, int n)		{ _router = r; _eindex = n; }
  
  void processing_vector(Subvector<int> &, Subvector<int> &, ErrorHandler *) const;
  void initialize_ports(const Subvector<int> &, const Subvector<int> &);
  
  void forward_flow(int, Bitvector *) const;
  void backward_flow(int, Bitvector *) const;
  
  int connect_input(int which, Element *, int);
  int connect_output(int which, Element *, int);

  void add_default_handlers(bool writable_config);

#if CLICK_STATS >= 2
  // Statistics
  int _calls; // Push and pull calls into this element.
  unsigned long long _self_cycles;  // Cycles spent in self and children.
  unsigned long long _child_cycles; // Cycles spent in children.
#endif
  
  class Port { public:

    Port();
    Port(Element *, Element *, int);
    
    operator bool() const		{ return _e != 0; }
    bool allowed() const		{ return _port >= 0; }
    bool initialized() const		{ return _port >= -1; }
    
    Element *element() const		{ return _e; }
    int port() const			{ return _port; }
    
    void push(Packet *p) const;
    Packet *pull() const;

#if CLICK_STATS >= 1
    unsigned npackets() const		{ return _packets; }
#endif

   private:
    
    Element *_e;
    int _port;
    
#if CLICK_STATS >= 1
    mutable unsigned _packets;		// How many packets have we moved?
#endif
#if CLICK_STATS >= 2
    Element *_owner;			// Whose input or output are we?
#endif
    
  };

 private:
  
  static const int INLINE_PORTS = 4;

  Port *_inputs;
  Port *_outputs;
  Port _ports0[INLINE_PORTS];

  int _ninputs;
  int _noutputs;

  Router *_router;
  int _eindex;

  Element(const Element &);
  Element &operator=(const Element &);
  
  void set_nports(int, int);
  
};


inline int
Element::eindex(Router *r) const
{
  return (router() == r ? eindex() : -1);
}

inline const Element::Port &
Element::input(int i) const
{
  assert(i >= 0 && i < ninputs());
  return _inputs[i];
}

inline const Element::Port &
Element::output(int o) const
{
  assert(o >= 0 && o < noutputs());
  return _outputs[o];
}

inline bool
Element::output_is_push(int o) const
{
  return o >= 0 && o < noutputs() && _outputs[o].allowed();
}

inline bool
Element::input_is_pull(int i) const
{
  return i >= 0 && i < ninputs() && _inputs[i].allowed();
}

#if CLICK_STATS >= 2
# define PORT_CTOR_INIT(o) , _packets(0), _owner(o)
#else
# if CLICK_STATS >= 1
#  define PORT_CTOR_INIT(o) , _packets(0)
# else
#  define PORT_CTOR_INIT(o)
# endif
#endif

inline
Element::Port::Port()
  : _e(0), _port(-2) PORT_CTOR_INIT(0)
{
}

inline
Element::Port::Port(Element *owner, Element *e, int p)
  : _e(e), _port(p) PORT_CTOR_INIT(owner)
{
  (void) owner;
}

inline void
Element::Port::push(Packet *p) const
{
  assert(_e);
#if CLICK_STATS >= 1
  _packets++;
#endif
#if CLICK_STATS >= 2
  _e->input(_port)._packets++;
  unsigned long long c0 = click_get_cycles();
  _e->push(_port, p);
  unsigned long long c1 = click_get_cycles();
  unsigned long long x = c1 - c0;
  _e->_calls += 1;
  _e->_self_cycles += x;
  _owner->_child_cycles += x;
#else
  _e->push(_port, p);
#endif
}

inline Packet *
Element::Port::pull() const
{
  assert(_e);
#if CLICK_STATS >= 2
  _e->output(_port)._packets++;
  unsigned long long c0 = click_get_cycles();
  Packet *p = _e->pull(_port);
  unsigned long long c1 = click_get_cycles();
  unsigned long long x = c1 - c0;
  _e->_calls += 1;
  _e->_self_cycles += x;
  _owner->_child_cycles += x;
#else
  Packet *p = _e->pull(_port);
#endif
#if CLICK_STATS >= 1
  if (p) _packets++;
#endif
  return p;
}

inline void
Element::checked_output_push(int o, Packet *p) const
{
  if ((unsigned)o < (unsigned)noutputs())
    _outputs[o].push(p);
  else
    p->kill();
}

#undef CONNECTION_CTOR_INIT
#endif

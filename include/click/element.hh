#ifndef ELEMENT_HH
#define ELEMENT_HH
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
  
  enum Processing { VAGNOSTIC, VPUSH, VPULL };
  static const char * const AGNOSTIC, * const PUSH, * const PULL;
  static const char * const PUSH_TO_PULL, * const PULL_TO_PUSH;

  static const char * const COMPLETE_FLOW;

  enum ConfigurePhase {
    CONFIGURE_PHASE_FIRST = 0,
    CONFIGURE_PHASE_INFO = 20,
    CONFIGURE_PHASE_DEFAULT = 100,
    CONFIGURE_PHASE_LAST = 2000
  };
  
  class Connection;
  
  Element();
  Element(int ninputs, int noutputs);
  virtual ~Element();
  static int nelements_allocated;

  // CHARACTERISTICS
  virtual const char *class_name() const = 0;
  virtual void *cast(const char *);
  
  String id() const;
  String declaration() const;
  String landmark() const;
  
  Router *router() const		{ return _router; }
  int eindex() const			{ return _eindex; }
  int eindex(Router *) const;

  void attach_router(Router *r, int n)	{ _router = r; _eindex = n; }

  // INPUTS
  int ninputs() const				{ return _ninputs; }
  const Connection &input(int input_id) const;
  
  void add_input()				{ set_ninputs(ninputs()+1); }
  void set_ninputs(int);
  virtual void notify_ninputs(int);
  
  int connect_input(int input_id, Element *, int);
  
  // OUTPUTS
  int noutputs() const				{ return _noutputs; }
  const Connection &output(int output_id) const;
  void checked_push_output(int output_id, Packet *) const;
  
  void add_output()				{ set_noutputs(noutputs()+1); }
  void set_noutputs(int);
  virtual void notify_noutputs(int);
  
  int connect_output(int output_id, Element *, int);
  
  // FLOW
  virtual const char *flow_code() const;
  
  void forward_flow(int, Bitvector *) const;
  void backward_flow(int, Bitvector *) const;
  
  // PUSH OR PULL PROCESSING
  virtual const char *processing() const;
  virtual const char *flags() const;
  
  void processing_vector(Subvector<int> &, Subvector<int> &, ErrorHandler *) const;
  void set_processing_vector(const Subvector<int> &, const Subvector<int> &);
  
  bool output_is_push(int) const;
  bool input_is_pull(int) const;
  
  // CLONING AND CONFIGURATION
  virtual Element *clone() const = 0;
  virtual int configure_phase() const;
  virtual int configure(const Vector<String> &, ErrorHandler *);
  virtual int initialize(ErrorHandler *);
  virtual void uninitialize();

  // LIVE CONFIGURATION
  virtual bool can_live_reconfigure() const;
  virtual int live_reconfigure(const Vector<String> &, ErrorHandler *);
  virtual void take_state(Element *, ErrorHandler *);
  
  void set_configuration(const String &);
  void set_configuration_argument(int, const String &);

  // HANDLERS
  void add_read_handler(const String &, ReadHandler, void *);
  void add_write_handler(const String &, WriteHandler, void *);
  void add_default_handlers(bool allow_write_config);
  void add_task_handlers(Task *, bool allow_write = false);
  virtual void add_handlers();
  static String configuration_read_handler(Element *, void *);
  static int reconfigure_write_handler(const String &, Element *, void *,
				       ErrorHandler *);

  virtual int llrpc(unsigned command, void *arg);
  virtual int local_llrpc(unsigned command, void *arg);
  
  // RUNTIME
  virtual void run_scheduled();
#if CLICK_USERLEVEL
  enum { SELECT_READ = 1, SELECT_WRITE = 2 };
  int add_select(int fd, int mask) const;
  int remove_select(int fd, int mask) const;
  virtual void selected(int fd);
#endif

  virtual void push(int port, Packet *);
  virtual Packet *pull(int port);
  virtual Packet *simple_action(Packet *);
  
#if CLICK_STATS >= 2
  // Statistics
  int _calls; // Push and pull calls into this element.
  unsigned long long _self_cycles;  // Cycles spent in self and children.
  unsigned long long _child_cycles; // Cycles spent in children.
#endif
  
  class Connection { public:

    Connection();
    Connection(Element *);
    Connection(Element *, Element *, int);
    
    operator bool() const		{ return _e != 0; }
    bool allowed() const		{ return _port >= 0; }
    void clear()			{ _e = 0; _port = 0; }
    void disallow()			{ _e = 0; _port = -1; }
    
    Element *element() const		{ return _e; }
    int port() const			{ return _port; }
    
    void push(Packet *p) const;
    Packet *pull() const;

#if CLICK_STATS >= 1
    unsigned int packet_count() const	{ return _packets; }
#endif

   private:
    
    Element *_e;
    int _port;
    
    // Statistics.
#if CLICK_STATS >= 1
    mutable int _packets;	// How many packets have we moved?
#endif
#if CLICK_STATS >= 2
    Element *_owner;		// Whose input or output are we?
#endif
    
  };

 private:
  
  static const int INLINE_PORTS = 4;

  Connection *_inputs;
  Connection *_outputs;
  Connection _ports0[INLINE_PORTS];

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

inline const Element::Connection &
Element::input(int i) const
{
  assert(i >= 0 && i < ninputs());
  return _inputs[i];
}

inline const Element::Connection &
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
# define CONNECTION_CTOR_ARG(o) Element *o
# define CONNECTION_CTOR_INIT(o) , _packets(0), _owner(o)
#else
# define CONNECTION_CTOR_ARG(o) Element *
# if CLICK_STATS >= 1
#  define CONNECTION_CTOR_INIT(o) , _packets(0)
# else
#  define CONNECTION_CTOR_INIT(o)
# endif
#endif

inline
Element::Connection::Connection()
  : _e(0), _port(0) CONNECTION_CTOR_INIT(0)
{
}

inline
Element::Connection::Connection(CONNECTION_CTOR_ARG(owner))
  : _e(0), _port(0) CONNECTION_CTOR_INIT(owner)
{
}

inline
Element::Connection::Connection(CONNECTION_CTOR_ARG(owner), Element *e, int p)
  : _e(e), _port(p) CONNECTION_CTOR_INIT(owner)
{
}

inline void
Element::Connection::push(Packet *p) const
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
Element::Connection::pull() const
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
Element::checked_push_output(int o, Packet *p) const
{
  if ((unsigned)o < (unsigned)noutputs())
    _outputs[o].push(p);
  else
    p->kill();
}

#undef CONNECTION_CTOR_ARG
#undef CONNECTION_CTOR_INIT
#endif

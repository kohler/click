#ifndef ELEMENT_HH
#define ELEMENT_HH
#include "glue.hh"
#include "vector.hh"
#include "string.hh"
#include "packet.hh"
#include "elemlink.hh"
class Router;
class ErrorHandler;
class Bitvector;

class Element : public ElementLink { public:
  
  enum Processing { AGNOSTIC, PUSH, PULL, PUSH_TO_PULL, PULL_TO_PUSH };
  class HandlerRegistry;
  class Connection;
  
  Element();
  Element(int ninputs, int noutputs);
  virtual ~Element();
  static void static_initialize();
  
  void use()				{ _refcount++; }
  void unuse()				{ if (--_refcount <= 0) delete this; }
  static int nelements_allocated;
  
  // CHARACTERISTICS
  virtual const char *class_name() const = 0;
  virtual bool is_a(const char *) const;
  Element *is_a_cast(const char *name)	{ return (is_a(name) ? this : 0); }
  
  const String &id() const			{ return _id; }
  void set_id(const String &);
  String declaration() const;
  
  const String &landmark() const		{ return _landmark; }
  void set_landmark(const String &s)		{ _landmark = s; }
  
  int number() const				{ return _number; }
  void set_number(int n)			{ _number = n; }

  Router *router() const		{ return (Router *)scheduled_list(); }
  
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
  virtual Bitvector forward_flow(int) const;
  virtual Bitvector backward_flow(int) const;
  
  // PUSH OR PULL PROCESSING
  virtual void processing_vector(Vector<int>&, int, Vector<int>&, int) const;
  virtual Processing default_processing() const;
  
  void set_processing_vector(const Vector<int>&, int, const Vector<int>&, int);
  
  static const char *processing_name(Processing);
  static const char *processing_name(int);
  
  bool output_is_push(int) const;
  bool input_is_pull(int) const;
  
  // CLONING AND CONFIGURATION
  virtual Element *clone() const = 0;
  virtual bool configure_first() const;
  virtual int configure(const String &, ErrorHandler *);
  virtual int initialize(ErrorHandler *);
  virtual void uninitialize();
  
  // LIVE CONFIGURATION
  virtual bool can_live_reconfigure() const;
  virtual int live_reconfigure(const String &, ErrorHandler *);
  
  // HANDLERS
  void add_default_handlers(HandlerRegistry *, bool allow_write_config);
  virtual void add_handlers(HandlerRegistry *);
  static String configuration_read_handler(Element *, void *);
  static int reconfigure_write_handler(const String &, Element *, void *,
				       ErrorHandler *);
  
  // RUNNING user level input elements.
  virtual int select_fd()		{ return(-1); }
  virtual void selected(int)		{ }

  // for Router::wait() to use to set up event waiting
  virtual bool still_busy() { return false; }
  virtual struct wait_queue** get_wait_queue() { return 0L; }
  virtual void do_waiting() {}
  virtual void finish_waiting() {}
  
  // Hooks for a non-empty Queue to tell an output driver to pull().
  virtual bool wants_packet_upstream() const;
  virtual void run_scheduled();
  
  virtual void push(int port, Packet *);
  virtual Packet *pull(int port);
  virtual Packet *simple_action(Packet *);
  
#if CLICK_STATS >= 2
  // Statistics
  int _calls; // Push and pull calls into this element.
  unsigned long long _self_cycles;  // Cycles spent in self and children.
  unsigned long long _child_cycles; // Cycles spent in children.
#endif
  
  class Connection {
    
    Element *_f;
    int _port;
    
    // Statistics.
#if CLICK_STATS >= 1
    mutable int _packets;	// How many packets have we moved?
#endif
#if CLICK_STATS >= 2
    Element *_owner;		// Whose input or output are we?
#endif
    
   public:
    
    Connection();
    Connection(Element *);
    Connection(Element *, Element *, int);
    
    operator bool() const		{ return _f != 0; }
    bool allowed() const		{ return _port >= 0; }
    void clear()			{ _f = 0; _port = 0; }
    void disallow()			{ _f = 0; _port = -1; }
    
    Element *element() const		{ return _f; }
    int port() const			{ return _port; }
    
    void push(Packet *p) const;
    Packet *pull() const;

#if CLICK_STATS >= 1
    unsigned int packet_count() const	{ return _packets; }
#endif
    
  };
  
 private:
  
  String _id;
  String _landmark;
  int _number;
  int _refcount;
  
  static const int InlinePorts = 1;
  int _ninputs;
  Connection *_inputs;
  Connection _input0[InlinePorts];
  int _noutputs;
  Connection *_outputs;
  Connection _output0[InlinePorts];
  
  Element(const Element &);
  Element &operator=(const Element &);
  
  void set_nports(int &, Connection *&, Connection *, int count);
  
};

typedef String (*ReadHandler)(Element *, void *);
typedef int (*WriteHandler)(const String &, Element *, void *, ErrorHandler *);

class Element::HandlerRegistry { public:
  
  HandlerRegistry()			{ }
  virtual ~HandlerRegistry()		{ }
  
  virtual void add_read(const char *name, int namelen, ReadHandler, void *);
  virtual void add_write(const char *name, int namelen, WriteHandler, void *);
  virtual void add_read_write(const char *name, int namelen,
			      ReadHandler, void *, WriteHandler, void *);

  void add_read(const char *, ReadHandler, void *);
  void add_write(const char *, WriteHandler, void *);
  void add_read_write(const char *, ReadHandler, void *, WriteHandler, void *);
  
};


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
  : _f(0), _port(0) CONNECTION_CTOR_INIT(0)
{
}

inline
Element::Connection::Connection(CONNECTION_CTOR_ARG(owner))
  : _f(0), _port(0) CONNECTION_CTOR_INIT(owner)
{
}

inline
Element::Connection::Connection(CONNECTION_CTOR_ARG(owner), Element *f, int p)
  : _f(f), _port(p) CONNECTION_CTOR_INIT(owner)
{
}

inline bool
operator==(const Element::Connection &c1, const Element::Connection &c2)
{
  return c1.element() == c2.element() && c1.port() == c2.port();
}

inline void
Element::Connection::push(Packet *p) const
{
  assert(_f);
#if CLICK_STATS >= 1
  _packets++;
#endif
#if CLICK_STATS >= 2
  _f->input(_port)._packets++;
  unsigned long long c0 = click_get_cycles();
  _f->push(_port, p);
  unsigned long long c1 = click_get_cycles();
  unsigned long long x = c1 - c0;
  _f->_calls += 1;
  _f->_self_cycles += x;
  _owner->_child_cycles += x;
#else
  _f->push(_port, p);
#endif
}

inline Packet *
Element::Connection::pull() const
{
  assert(_f);
#if CLICK_STATS >= 2
  _f->output(_port)._packets++;
  unsigned long long c0 = click_get_cycles();
  Packet *p = _f->pull(_port);
  unsigned long long c1 = click_get_cycles();
  unsigned long long x = c1 - c0;
  _f->_calls += 1;
  _f->_self_cycles += x;
  _owner->_child_cycles += x;
#else
  Packet *p = _f->pull(_port);
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

#ifndef ROUTER_HH
#define ROUTER_HH
#include "element.hh"
#include "timer.hh"
#include "bitvector.hh"
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
class ElementFilter;

class Router : public ElementLink {

  struct Hookup;
  struct Handler;

  Timer _timer_head;
  bool _please_stop_driver;

#ifdef CLICK_USERLEVEL
  struct Selector;
  fd_set _read_select_fd_set;
  fd_set _write_select_fd_set;
  Vector<Selector> _selectors;
#endif
  
  int _refcount;
  
  Vector<Element *> _elements;
  Vector<String> _element_names;
  Vector<String> _configurations;
  Vector<String> _element_landmarks;
  
  Vector<Hookup> _hookup_from;
  Vector<Hookup> _hookup_to;
  
  Vector<int> _input_pidx;
  Vector<int> _output_pidx;
  Vector<int> _input_fidx;
  Vector<int> _output_fidx;
  
  Vector<int> _hookpidx_from;
  Vector<int> _hookpidx_to;

  Vector<String> _requirements;

  bool _initialized: 1;
  bool _have_connections: 1;
  bool _have_hookpidx: 1;

  Vector<int> _ehandler_first_by_element;
  Vector<int> _ehandler_to_handler;
  Vector<int> _ehandler_next;

  Vector<int> _handler_first_by_name;
  Vector<int> _handler_next_by_name;
  Vector<int> _handler_use_count;
  
  Handler *_handlers;
  int _nhandlers;
  int _handlers_cap;

  Router(const Router &);
  Router &operator=(const Router &);
  
  void remove_hookup(int);
  void hookup_error(const Hookup &, bool, const char *, ErrorHandler *);
  int check_hookup_elements(ErrorHandler *);
  void notify_hookup_range();
  void check_hookup_range(ErrorHandler *);
  void check_hookup_completeness(ErrorHandler *);  
  
  int processing_error(const Hookup&, const Hookup&, bool, int, ErrorHandler*);
  int check_push_and_pull(ErrorHandler *);
  
  void make_pidxes();
  int input_pidx(const Hookup &) const;
  int input_pidx_element(int) const;
  int input_pidx_port(int) const;
  int output_pidx(const Hookup &) const;
  int output_pidx_element(int) const;
  int output_pidx_port(int) const;
  void make_hookpidxes();
  
  void set_connections();
  
  String context_message(int element_no, const char *) const;
  int element_lerror(ErrorHandler *, Element *, const char *, ...) const;
  
  Element *find(String, const String &, ErrorHandler * = 0) const;
  int find_ehandler(int, const String &, bool);
  int put_handler(const Handler &);
  
  int downstream_inputs(Element *, int o, ElementFilter *, Bitvector &);
  int upstream_outputs(Element *, int i, ElementFilter *, Bitvector &);

  static const unsigned int _max_driver_count = 10000;

 public:
  
  Router();
  ~Router();
  void use()					{ _refcount++; }
  void unuse();
  
  int add_element(Element *, const String &name, const String &conf, const String &landmark);
  int add_connection(int from_idx, int from_port, int to_idx, int to_port);
  void add_requirement(const String &);
  
  bool initialized() const			{ return _initialized; }
  
  int nelements() const				{ return _elements.size(); }
  Element *element(int) const;
  const String &ename(int) const;
  const String &econfiguration(int) const;
  const String &elandmark(int) const;
  const Vector<Element *> &elements() const	{ return _elements; }
  Element *find(const String &, ErrorHandler * = 0) const;
  Element *find(Element *, const String &, ErrorHandler * = 0) const;

  const Vector<String> &requirements() const	{ return _requirements; }

  int ninput_pidx() const			{ return _input_fidx.size(); }
  int noutput_pidx() const			{ return _output_fidx.size(); }

  int downstream_elements(Element *, int o, ElementFilter*, Vector<Element*>&);
  int downstream_elements(Element *, int o, Vector<Element *> &);
  int downstream_elements(Element *, Vector<Element *> &);
  int upstream_elements(Element *, int i, ElementFilter*, Vector<Element*>&);
  int upstream_elements(Element *, int i, Vector<Element *> &);  
  int upstream_elements(Element *, Vector<Element *> &);
  
  int initialize(ErrorHandler *);
  void take_state(Router *, ErrorHandler *);

  void add_read_handler(int, const String &, ReadHandler, void *);
  void add_write_handler(int, const String &, WriteHandler, void *);
  int find_handler(Element *, const String &);
  void element_handlers(int, Vector<int> &) const;
  int nhandlers() const				{ return _nhandlers; }
  const Handler &handler(int i) const;

  int live_reconfigure(int, const String &, ErrorHandler *);
  int live_reconfigure(int, const Vector<String> &, ErrorHandler *);
  void set_configuration(int, const String &);

  Timer *timer_head()				{ return &_timer_head; }

#if CLICK_USERLEVEL
  enum { SELECT_READ = Element::SELECT_READ, SELECT_WRITE = Element::SELECT_WRITE };
  int add_select(int fd, int element, int mask);
  int remove_select(int fd, int element, int mask);
#endif
  
  void driver();
  void driver_once();
  void wait();
  
  String flat_configuration_string() const;
  String element_list_string() const;
  String element_inputs_string(int) const;
  String element_outputs_string(int) const;
  
  void please_stop_driver()			{ _please_stop_driver = 1; }
  
};


struct Router::Hookup {
  int idx;
  int port;
  Hookup()				: idx(-1) { }
  Hookup(int i, int p)			: idx(i), port(p) { }
};

struct Router::Handler {
  String name;
  ReadHandler read;
  void *read_thunk;
  WriteHandler write;
  void *write_thunk;
};
  
#if CLICK_USERLEVEL
struct Router::Selector {
  int fd;
  int element;
  int mask;
  Selector()				: fd(-1), element(-1), mask(0) { }
  Selector(int f, int e, int m)		: fd(f), element(e), mask(m) { }
};
#endif


inline bool
operator==(const Router::Hookup &a, const Router::Hookup &b)
{
  return a.idx == b.idx && a.port == b.port;
}

inline bool
operator!=(const Router::Hookup &a, const Router::Hookup &b)
{
  return a.idx != b.idx || a.port != b.port;
}

inline Element *
Router::find(const String &name, ErrorHandler *errh) const
{
  return find("", name, errh);
}

inline const Router::Handler &
Router::handler(int i) const
{
  assert(i>=0 && i<_nhandlers);
  return _handlers[i];
}

#endif

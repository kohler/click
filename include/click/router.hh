// -*- c-basic-offset: 2; related-file-name: "../../lib/router.cc" -*-
#ifndef CLICK_ROUTER_HH
#define CLICK_ROUTER_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/sync.hh>
#include <click/bitvector.hh>
#include <click/task.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
#if CLICK_NS
# include <click/simclick.h>
#endif

CLICK_DECLS
class ElementFilter;
class RouterThread;
class BigHashMap_ArenaFactory;

class Router { public:

  struct Handler;
  struct Hookup {
    int idx;
    int port;
    Hookup()					: idx(-1) { }
    Hookup(int i, int p)			: idx(i), port(p) { }
  };
  
  Router(const String &configuration = String());
  ~Router();
  void use()					{ _refcount++; }
  void unuse();

  static void static_initialize();
  static void static_cleanup();

  bool initialized() const			{ return _initialized; }

  // ELEMENTS
  int nelements() const				{ return _elements.size(); }
  const Vector<Element *> &elements() const	{ return _elements; }
  static Element *element(const Router *, int);
  Element *element(int e) const			{ return element(this, e); }
  // element() returns 0 on bad index or no router, root_element() on index -1
  Element *root_element() const			{ return _root_element; }
  
  const String &ename(int) const;
  const String &elandmark(int) const;
  const String &default_configuration_string(int) const;
  void set_default_configuration_string(int, const String &);
  
  Element *find(const String &, ErrorHandler * = 0) const;
  Element *find(const String &, Element *context, ErrorHandler * = 0) const;
  Element *find(const String &, String prefix, ErrorHandler * = 0) const;
  
  int downstream_elements(Element *, int o, ElementFilter*, Vector<Element*>&);
  int downstream_elements(Element *, int o, Vector<Element *> &);
  int downstream_elements(Element *, Vector<Element *> &);
  int upstream_elements(Element *, int i, ElementFilter*, Vector<Element*>&);
  int upstream_elements(Element *, int i, Vector<Element *> &);  
  int upstream_elements(Element *, Vector<Element *> &);

  // HANDLERS
  enum { FIRST_GLOBAL_HANDLER = 0x40000000 };
  int nhandlers() const				{ return _nhandlers; }
  static int nglobal_handlers();
  static int hindex(const Element *, const String &);
  static void element_hindexes(const Element *, Vector<int> &);

  // 'const Handler *' result pointers last only until a new handler is added
  static const Handler *handler(const Router *, int);
  static const Handler *handler(const Element *, int);
  const Handler *handler(int) const;
  static const Handler *handler(const Element *, const String &);
  
  static void add_read_handler(const Element *, const String &, ReadHandler, void *);
  static void add_write_handler(const Element *, const String &, WriteHandler, void *);
  static int change_handler_flags(const Element *, const String &, uint32_t clear_flags, uint32_t set_flags);

  // ATTACHMENTS AND REQUIREMENTS
  void *attachment(const String &) const;
  void *&force_attachment(const String &);
  void *set_attachment(const String &, void *);
  const Vector<String> &requirements() const	{ return _requirements; }

  Router *hotswap_router() const		{ return _hotswap_router; }
  ErrorHandler *chatter_channel(const String &) const;
  BigHashMap_ArenaFactory *arena_factory() const;

  // THREADS
  int nthreads() const				{ return _threads.size() - 1; }
  RouterThread *thread(int id) const		{ return _threads[id + 1]; }
  // thread(-1) is the quiescent thread
  void add_thread(RouterThread *);
  void remove_thread(RouterThread *);
  
  // DRIVER RESERVATIONS
  void please_stop_driver()		{ adjust_driver_reservations(-1); }
  void reserve_driver()			{ adjust_driver_reservations(1); }
  void set_driver_reservations(int);
  void adjust_driver_reservations(int);
  bool check_driver();
  const volatile int *driver_runcount_ptr() const { return &_driver_runcount; }

  // TIMERS AND SELECTS
  TimerList *timer_list()			{ return &_timer_list; }
  TaskList *task_list()				{ return &_task_list; }
#if CLICK_USERLEVEL
  enum { SELECT_READ = Element::SELECT_READ, SELECT_WRITE = Element::SELECT_WRITE };
  int add_select(int fd, int element, int mask);
  int remove_select(int fd, int element, int mask);
#endif

#if CLICK_USERLEVEL
  void run_selects(bool more_tasks);
#endif
  void run_timers();
  
  // UNPARSING
  const String &configuration_string() const	{ return _configuration; }
  void unparse(StringAccum &, const String & = String()) const;
  void unparse_requirements(StringAccum &, const String & = String()) const;
  void unparse_classes(StringAccum &, const String & = String()) const;
  void unparse_declarations(StringAccum &, const String & = String()) const;
  void unparse_connections(StringAccum &, const String & = String()) const;
  
  String element_ports_string(int) const;
  
  // INITIALIZATION
  void add_requirement(const String &);
  int add_element(Element *, const String &name, const String &conf, const String &landmark);
  int add_connection(int from_idx, int from_port, int to_idx, int to_port);
  
  void preinitialize();
  void pre_take_state(Router *);
  int initialize(ErrorHandler *);
  void take_state(ErrorHandler *);

#if CLICK_NS
  int sim_get_ifid(const char *ifname);
  int sim_listen(int ifid, int element);
  int sim_if_ready(int ifid);
  int sim_write(int ifid, int ptype, const unsigned char *, int len,
		simclick_simpacketinfo *pinfo);
  int sim_incoming_packet(int ifid, int ptype, const unsigned char *, int len,
                          simclick_simpacketinfo* pinfo);
  void set_siminst(simclick_sim newinst) { _siminst = newinst; }
  simclick_sim get_siminst() const	{ return _siminst; }

  void set_clickinst(simclick_click newinst) { _clickinst = newinst; }
  simclick_click get_clickinst() const	{ return _clickinst; }

 protected:
  simclick_sim _siminst;
  simclick_click _clickinst;
  Vector<Vector<int> *> _listenvecs;

  Vector<int> *sim_listenvec(int ifid);
#endif
  
 private:
  
  TimerList _timer_list;
  TaskList _task_list;
  volatile int _driver_runcount;
  Spinlock _runcount_lock;

#if CLICK_USERLEVEL
  enum { SELECT_RELEVANT = (SELECT_READ | SELECT_WRITE) };
  struct Selector;
  fd_set _read_select_fd_set;
  fd_set _write_select_fd_set;
  int _max_select_fd;
  Vector<Selector> _selectors;
  Spinlock _wait_lock;
#endif

  uatomic32_t _refcount;
  
  Vector<Element *> _elements;
  Vector<String> _element_names;
  Vector<String> _element_configurations;
  Vector<String> _element_landmarks;
  Vector<int> _element_configure_order;

  Vector<RouterThread *> _threads;
  
  Vector<Hookup> _hookup_from;
  Vector<Hookup> _hookup_to;
  
  Vector<int> _input_pidx;
  Vector<int> _output_pidx;
  Vector<int> _input_eidx;
  Vector<int> _output_eidx;
  
  Vector<int> _hookpidx_from;
  Vector<int> _hookpidx_to;

  Vector<String> _requirements;

  bool _preinitialized : 1;
  bool _initialized : 1;
  bool _initialize_attempted : 1;
  bool _cleaned : 1;
  bool _have_connections : 1;
  bool _have_hookpidx : 1;
  mutable bool _allow_star_handler : 1;
  
  Vector<int> _ehandler_first_by_element;
  Vector<int> _ehandler_to_handler;
  Vector<int> _ehandler_next;

  Vector<int> _handler_first_by_name;
  
  Handler *_handlers;
  int _nhandlers;
  int _handlers_cap;

  Vector<String> _attachment_names;
  Vector<void *> _attachments;
  
  Element *_root_element;
  String _configuration;

  BigHashMap_ArenaFactory *_arena_factory;
  Router *_hotswap_router;
  
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
  int ninput_pidx() const		{ return _input_eidx.size(); }
  int noutput_pidx() const		{ return _output_eidx.size(); }
  inline int input_pidx(const Hookup &) const;
  inline int input_pidx_element(int) const;
  inline int input_pidx_port(int) const;
  inline int output_pidx(const Hookup &) const;
  inline int output_pidx_element(int) const;
  inline int output_pidx_port(int) const;
  void make_hookpidxes();
  
  void set_connections();
  
  String context_message(int element_no, const char *) const;
  int element_lerror(ErrorHandler *, Element *, const char *, ...) const;

  // private handler methods
  void initialize_handlers(bool, bool);
  int find_ehandler(int, const String &) const;
  static inline Handler fetch_handler(const Element *, const String &);
  void store_local_handler(int, const Handler &);
  static void store_global_handler(const Handler &);
  static inline void store_handler(const Element *, const Handler &);

  int downstream_inputs(Element *, int o, ElementFilter *, Bitvector &);
  int upstream_outputs(Element *, int i, ElementFilter *, Bitvector &);

  // global handlers
  static String router_read_handler(Element *, void *);
  
};


class Router::Handler { public:

  enum { DRIVER_FLAG_0 = 1, DRIVER_FLAG_1 = 2,
	 DRIVER_FLAG_2 = 4, DRIVER_FLAG_3 = 8,
	 USER_FLAG_SHIFT = 4, USER_FLAG_0 = 1 << USER_FLAG_SHIFT };
  
  const String &name() const	{ return _name; }
  uint32_t flags() const	{ return _flags; }
  
  bool readable() const		{ return _read; }
  bool read_visible() const	{ return _read; }
  bool writable() const		{ return _write; }
  bool write_visible() const	{ return _write; }
  bool visible() const		{ return read_visible() || write_visible(); }

  String call_read(Element *) const;
  int call_write(const String &, Element *, ErrorHandler *) const;
  
  String unparse_name(Element *) const;
  static String unparse_name(Element *, const String &);

 private:
  
  String _name;
  ReadHandler _read;
  void *_read_thunk;
  WriteHandler _write;
  void *_write_thunk;
  uint32_t _flags;
  int _use_count;
  int _next_by_name;

  Handler();
  Handler(const String &);

  bool compatible(const Handler &) const;
  
  friend class Router;
  
};

/* The largest size a write handler is allowed to have. */
#define LARGEST_HANDLER_WRITE 65536


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
  return find(name, "", errh);
}

inline const Router::Handler *
Router::handler(const Element *e, int hi)
{
  return handler(e ? e->router() : 0, hi);
}

inline const Router::Handler *
Router::handler(int hi) const
{
  return handler(this, hi);
}

inline
Router::Handler::Handler()
  : _read(0), _read_thunk(0), _write(0), _write_thunk(0),
    _flags(0), _use_count(0), _next_by_name(-1)
{
}

inline
Router::Handler::Handler(const String &name)
  : _name(name), _read(0), _read_thunk(0), _write(0), _write_thunk(0),
    _flags(0), _use_count(0), _next_by_name(-1)
{
}

inline String
Router::Handler::call_read(Element *e) const
{
  return _read(e, _read_thunk);
}

inline int
Router::Handler::call_write(const String &s, Element *e, ErrorHandler *errh) const
{
  return _write(s, e, _write_thunk, errh);
}

inline bool
Router::Handler::compatible(const Handler &h) const
{
  return (_read == h._read && _read_thunk == h._read_thunk
	  && _write == h._write && _write_thunk == h._write_thunk
	  && _flags == h._flags);
}

inline void
Router::run_timers()
{
  _timer_list.run(&_driver_runcount);
}

inline BigHashMap_ArenaFactory *
Router::arena_factory() const
{
  return _arena_factory;
}

CLICK_ENDDECLS
#endif

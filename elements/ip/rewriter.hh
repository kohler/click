#ifndef REWRITER_HH
#define REWRITER_HH
#include "element.hh"
#include "timer.hh"
#include "hashmap.hh"

/*
 * =c
 * Rewriter()
 * =d
 * 
 */

class IPConnection {
  
  IPAddress _src;
  IPAddress _dst;

public:

  IPConnection() : _src(), _dst() { }
  explicit IPConnection(IPAddress s, IPAddress d) { _src = s; _dst = d; }
  
  operator bool() const		{ return _src && _dst; }
  bool operator==(IPConnection &c) { return _src == c._src && _dst == c._dst; }

				// is this a good hash function?
  unsigned hashcode() const	{ return _src.hashcode() ^ _dst.hashcode(); }
  
  operator String() const	{ return (s()); }
  String s() const { return _src.s() + "->" + _dst.s(); };
};

class Rewriter : public Element {
  Timer _timer;

  IPAddress _fakenet;
  IPAddress _localaddr;
  int _gc_interval_sec;

  struct Mapping {
    IPAddress rs;		// Real source
    IPAddress rd;		// Real destination
    IPAddress fs;		// Fake source
    IPAddress fd;		// Fake destination
    int type;
    int used;
  };

  HashMap<IPConnection, int> _r2f; // Real->Fake
  Vector<Mapping *> _f2r;	// Fake->Real

  Vector<int> _free;
  inline int next_index();
  inline int get_index(IPAddress);
  int fix_csums(Packet *);
  void check_tcp();

public:

  Rewriter();
  ~Rewriter();
  Rewriter *clone() const;
  
  const char *class_name() const		{ return "Rewriter"; }
  Processing default_processing() const		{ return PUSH; }
  
  int configure(const String &, ErrorHandler *);
  void add_handlers(HandlerRegistry *);
  int initialize(ErrorHandler *);
  void uninitialize();
  void run_scheduled();
  
  void push(int, Packet *);

  String dump_table();
};

#endif REWRITER_HH

#ifndef REWRITER_HH
#define REWRITER_HH
#include "element.hh"
#include "timer.hh"
#include "hashmap.hh"

/*
 * =c
 * Rewriter(pattern_1, ..., pattern_n)
 * =d
 * Rewrites UDP and TCP packets according to the specified rewrite patterns.
 * 
 * Has two inputs.  If it is configured with only one pattern, it has 2 
 * outputs.  Otherwise, for n patterns, it has 2+n outputs.
 *
 * Input 0 is the "forward" input.  If there is only one pattern, data
 * coming in on input 0 is rewritten and pushed out on output 0.
 * If there are two or more patterns, the first time that a packet for a
 * particular connection (source/destination pair) is seen on input 0, 
 * it is forwarded unchanged on output 0.  Output 0 should be connected to a
 * MappingCreator or similar element, which uses Rewriter's "establish_mapping"
 * method to create a mapping in the Rewriter between the new connection and
 * a given Rewriter pattern i (1 <= i <= n).  When a packet for the connection
 * is next seen, it is rewritten according to pattern i and pushed on output i.
 *
 * Input 1 is the "reverse" input.  Packets coming in on input 1 are
 * rewritten back to their original form (the rewriting performed on input 0
 * is undone) and sent out on the last output.  In the case of one Rewriter
 * pattern, the last output is output 1; for n>1 patterns, it is output n+1.
 * 
 * 
 * =a MappingCreator
 */

class Rewriter : public Element {

  class Pattern;
  class Connection;
  class Mapping;

  class Connection {
    // Connection represents a specific TCP or UDP connection, identified
    // by <saddr/sport/daddr/dport>.
    IPAddress _saddr;
    short _sport;		// network byte order
    IPAddress _daddr;
    short _dport;		// network byte order

    Pattern *_pat;

    bool _used;
    bool _removed;

    void fix_csums(Packet *);

    friend class Pattern;

  public:
    Connection();
    Connection(Packet *p);
    Connection(unsigned long sa, unsigned short sp, 
	       unsigned long da, unsigned short dp);
    ~Connection() 				{ }

    void set(Packet *p);

    operator bool() const;
    bool operator==(Connection &c);

    unsigned hashcode() const;
  
    String s() const;
    operator String() const			{ return (s()); }

    void set_pattern(Pattern *p) 		{ _pat = p; }
    Pattern *pattern() 				{ return _pat; }

    bool used()					{ return _used; }
    void mark_used()				{ _used = true; }
    void reset_used()				{ _used = false; }
    bool removed() 				{ return _removed; }
    void remove() 				{ _removed = true; }
  };

  class Pattern {
    // Pattern is <saddr/sport[-sport2]/daddr/dport>.
    // It is associated with a Rewriter output port.
    // It can be applied to a specific Connection (<saddr/sport/daddr/dport>)
    // to obtain a new Connection rewritten according to the Pattern.
    // Any Pattern component can be '*', which means that the corresponding
    // Connection component is left unchanged. 
    IPAddress _saddr;
    int _sportl;		// host byte order
    int _sporth;		// host byte order
    IPAddress _daddr;
    int _dport;			// host byte order

    Vector<int> _free;
    void init_ports();

    int _output;

  public:
    Pattern();
    Pattern(int o);
    ~Pattern() 					{ }

    bool initialize(String &s);
    int output() 				{ return _output; }

    bool apply(Connection &in, Connection &out);
    bool free(Connection &c);

    String s() const;
    operator String() const			{ return (s()); }
  };

  class Mapping {
    HashMap <Connection, Connection> _fwd;
    HashMap <Connection, Connection> _rev;

  public:
    Mapping() : _fwd(), _rev()			{ }
    ~Mapping()					{ }

    bool add(Packet *p, Pattern *pat);
    bool apply(Packet *p, int &port);
    bool rapply(Packet *p);

    void mark_live_tcp();
    void clean();

    String s();
  };

  Vector<Pattern *> _patterns;
  Mapping _mapping;
  Timer _timer;
  int _npat;

  static const int _gc_interval_sec = 10;

public:

  Rewriter();
  ~Rewriter();
  Rewriter *clone() const			{ return new Rewriter(); }
  
  const char *class_name() const		{ return "Rewriter"; }
  Processing default_processing() const		{ return PUSH; }
  
  int configure(const String &, ErrorHandler *);
  void add_handlers();
  int initialize(ErrorHandler *);
  void uninitialize();
  void run_scheduled();
  
  int npatterns() const				{ return _npat; }
  bool establish_mapping(Packet *p, int pat);
  void push(int, Packet *);

  String dump_table();
  String dump_patterns();
};

#endif REWRITER_HH

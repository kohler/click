#ifndef CHUCKCHECK_HH
#define CHUCKCHECK_HH
#include "element.hh"

/*
 * =c
 * ChuckCheck()
 * =d
 */

class ChuckCheck : public Element {

  struct Stat {
    struct timeval time;
    unsigned saddr;
  };

  static const unsigned BUCKETS = 4096;
  
  Stat _info[BUCKETS];
  int _head;
  int _tail;
  int _first;

  static String read_handler(Element *, void *);

  inline void count(Packet *);
  
 public:
  
  ChuckCheck()					{ add_input(); add_output(); }
  
  const char *class_name() const		{ return "ChuckCheck"; }
  const char *processing() const		{ return AGNOSTIC; }
  void add_handlers();
  
  ChuckCheck *clone() const			{ return new ChuckCheck; }
  int initialize(ErrorHandler *);
  
  void push(int, Packet *);
  Packet *pull(int);
  
};

#endif

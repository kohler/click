#ifndef RED_HH
#define RED_HH
#include "element.hh"
#include "ewma.hh"
class Queue;

/*
 * =c
 * RED(min_thresh, max_thresh, max_p)
 * =d
 * Implements the Random Early Detection packet dropping
 * algorithm.
 *
 * A RED element expects to be followed by one or more
 * Queues. It maintains an average of the sum of the
 * queue lengths, and drops packets with a probability
 * proportional to that sum.
 *
 * =e
 * = ... -> RED(5, 50, 0.02) -> Queue(200) -> ...
 * 
 * =a Queue
 */

class RED : public Element {
  
  Queue *_queue1;
  Vector<Element *> _queues;
  
  // Queue sizes are shifted by this much.
  static const int QUEUE_SCALE = 10;

  unsigned _min_thresh;		// scaled by QUEUE_SCALE
  unsigned _max_thresh;		// scaled by QUEUE_SCALE
  int _max_p;			// out of 0xFFFF
  
  EWMA _size;
  
  int _C1;
  int _C2;
  int _count;
  int _random_value;
  
  int _drops;
  
  void set_C1_and_C2();

  static String read_stats(Element *, void *);
  static String read_queues(Element *, void *);
  static String read_parameter(Element *, void *);
  
 public:
  
  RED();
  
  const char *class_name() const		{ return "RED"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  void add_handlers(HandlerRegistry *fcr);
  
  int queue_size() const;
  const EWMA &average_queue_size() const	{ return _size; }
  int drops() const				{ return _drops; }
    
  RED *clone() const;
  int configure(const String &, ErrorHandler *);
  int initialize(ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  
  bool drop();
  void push(int port, Packet *);
  Packet *pull(int port);
  
};

#endif

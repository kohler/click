#ifndef RED_HH
#define RED_HH
#include "element.hh"
#include "ewma.hh"
class Storage;

/*
 * =c
 * RED(MIN_THRESH, MAX_THRESH, MAX_P [, QUEUES])
 * =d
 * Implements the Random Early Detection packet dropping
 * algorithm.
 *
 * A RED element is associated with one or more Storage elements (usually
 * Queues). It maintains an average of the sum of the
 * queue lengths, and drops packets with a probability
 * proportional to that sum. Usually, the Queues are found with
 * flow-based router context: if the RED is a push element, the nearest
 * downstream Queues are used; if it is a pull element, the nearest
 * upstream Queues are used.
 *
 * =e
 * = ... -> RED(5, 50, 0.02) -> Queue(200) -> ...
 *
 * =h min_thresh read/write
 * Returns or sets the MIN_THRESH configuration parameter.
 * =h max_thresh read/write
 * Returns or sets the MAX_THRESH configuration parameter.
 * =h max_p read/write
 * Returns or sets the MAX_P configuration parameter.
 * =h drops read-only
 * Returns the number of packets dropped so far.
 * =h queues read-only
 * Returns the Queues associated with this RED element, listed one per line.
 * =h stats read-only
 * Returns some human-readable statistics.
 *
 * =a Queue
 */

class RED : public Element {
  
  Storage *_queue1;
  Vector<Storage *> _queues;
  
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
  Vector<Element *> _queue_elements;
  
  void set_C1_and_C2();

  static String read_stats(Element *, void *);
  static String read_queues(Element *, void *);
  static String read_parameter(Element *, void *);
  
 public:
  
  RED();
  
  const char *class_name() const		{ return "RED"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  void add_handlers();
  
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

#ifndef RADIOSIM_HH
#define RADIOSIM_HH

/*
 * =c
 * RadioSim([keywords,] [lat1 lon1, lat2 lon2, ...])
 * =s Grid
 * simulates reachability and broadcast in an 802.11-like radio network
 * =d
 * RadioSim simulates reachability and broadcast in an 802.11-like
 * radio network.
 *
 * Each corresponding input/output pair corresponds to one node.
 * Each node has a latitude/longitude, given by the <i>th
 * configuration argument.
 *
 * When node <i> sends a packet into RadioSim's input <i>,
 * RadioSim sends a copy to each output whose node is
 * within 250 meters of node <i>.
 *
 * Inputs are pull, outputs are push. Services inputs in round
 * robin order.
 *
 * Keyword:
 *
 * =over 8
 * 
 * =item USE_XY
 *
 * Boolean.  Defaults to false.  Use x,y coordinates in metres instead
 * of lat,lon in degrees.  lat is treated as x, and lon is treated as
 * y.
 *
 * =back
 *
 * The loc read/write handler format is
 *   node-index latitude longitude */

#include <click/element.hh>
#include <click/vector.hh>
#include "grid.hh"
#include <click/task.hh>

class RadioSim : public Element {
  
 public:
  
  RadioSim();
  ~RadioSim();
  
  const char *class_name() const		{ return "RadioSim"; }
  const char *processing() const		{ return PULL_TO_PUSH; }
  RadioSim *clone() const;
  void notify_noutputs(int);
  void notify_ninputs(int);
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  void add_handlers();

  void run_scheduled();



private:

  struct Node {
    double _lat;
    double _lon;
    Node(double la, double lo) : _lat(la), _lon(lo) { }
    Node() : _lat(0), _lon(0) { }
  };

  Node get_node_loc(int i);
  void set_node_loc(int i, double lat, double lon);
  int nnodes() { return(_nodes.size()); }

  static int rs_write_handler(const String &, Element *, void *, ErrorHandler *);
  static String rs_read_handler(Element *, void *);

  Vector<Node> _nodes;
  Task _task;
  
  bool _use_xy;
};

#endif

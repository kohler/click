#ifndef RADIOSIM_HH
#define RADIOSIM_HH

/*
 * =c
 * RadioSim([lat1 lon1, lat2 lon2, ...])
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
 * The loc read/write handler format is
 *   node-index latitude longitude
 */

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
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *errh);
  void add_handlers();

  void run_scheduled();

  grid_location get_node_loc(int i);
  void set_node_loc(int i, double lat, double lon);
  int nnodes() { return(_nodes.size()); }

private:

  struct Node {
    double _lat;
    double _lon;
  };
  Vector<Node> _nodes;
  Task _task;
  
};

#endif

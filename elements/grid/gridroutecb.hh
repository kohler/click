/*
 * gridroutecb.hh -- Grid route action callback interface
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#ifndef GRIDROUTECB_HH
#define GRIDROUTECB_HH
#include <click/packet_anno.hh>
CLICK_DECLS

class GridRouteActionCallback {
public:

    virtual ~GridRouteActionCallback() { }

  enum Action {
    UnknownAction      = 0,
    SendToIP           = 1,
    ForwardDSDV        = 2,   // data = next hop ip, data2 = (qual << 16) | (-sig & 0xFFff)
    FallbackToGF       = 3,
    QueuedForLocQuery  = 4,
    ForwardGF          = 5,   // data = next hop ip, data2 = best nbr ip
    Drop               = 6,   // data = drop reason
    ProbeFinished      = 7,
    NoLocQueryNeeded   = 8,
    CachedLocFound     = 9
  };

  enum DropReason {
    UnknownReason      = 0,
    UnknownType        = 1,   // dest_ip is unknown
    NoLocalRoute       = 2,
    NoDestLoc          = 3,
    NoCloserNode       = 4,
    ConfigError        = 5,
    OwnLocUnknown      = 6,
    BadPacket          = 7
  };

  virtual void route_cb(int id, unsigned int dest_ip, Action a, unsigned int data, unsigned int data2) = 0;

protected:
  static void set_route_cb_bit(Packet *p, unsigned int cb_num) {
    unsigned int mask = 1 << cb_num;
    unsigned int newval = mask | GRID_ROUTE_CB_ANNO(p);
    SET_GRID_ROUTE_CB_ANNO(p, newval);
  }
};


class GridRouteActor {
public:
  GridRouteActor() { memset(&_cbs, 0, sizeof(_cbs)); }
  int add_callback(GridRouteActionCallback *cb) {
    int id = alloc_cb_id();
    if (id < 0)
      return -1;
    _cbs[id] = cb;
    return id;
  }

private:
  static bool cb_is_set(Packet *p, unsigned int cb_num) {
    unsigned int mask = 1 << cb_num;
    return mask & GRID_ROUTE_CB_ANNO(p);
  }

  /* this number must be synchronized with the size of the grid route
     callback annotation in packet_anno.hh */
  static const int _max_route_cbs = 8;

  /* defined in updateroutes.cc -- static so that all callback have
     unique ids, and share a bitmask in the packet annotation */
  static int _next_free_cb;

  static int alloc_cb_id() {
    if (_next_free_cb >= _max_route_cbs)
      return -1;
    int id = _next_free_cb;
    _next_free_cb++;
    return id;
  }

  GridRouteActionCallback *_cbs[_max_route_cbs];

protected:
  void notify_route_cbs(Packet *p, unsigned int dest_ip, GridRouteActionCallback::Action a,
			unsigned int data, unsigned int data2) {
    for (int i = 0; i < _max_route_cbs; i++) {
      if (_cbs[i] && cb_is_set(p, i))
	_cbs[i]->route_cb(i, dest_ip, a, data, data2);
    }
  }
};

CLICK_ENDDECLS
#endif /* GRIDROUTECB_HH */

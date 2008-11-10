#ifndef GRIDGENERICLOGGER_HH
#define GRIDGENERICLOGGER_HH

#include <click/element.hh>
#include "gridgenericrt.hh"
CLICK_DECLS

class GridGenericLogger : public Element {

public:

  GridGenericLogger() { }

  enum reason_t {
    WAS_SENDER        = 0xf1,
    WAS_ENTRY         = 0xf2,
    BROKEN_AD         = 0xf3,
    TIMEOUT           = 0xf4,
    NEXT_HOP_EXPIRED  = 0xf5,

    // DSDV route insertion reason codes
    NEW_DEST          = 0xe0,
    NEW_DEST_SENDER   = 0xe1,
    BETTER_RTE        = 0xe2,
    BETTER_RTE_SENDER = 0xe3,
    NEWER_SEQ         = 0xe4,
    NEWER_SEQ_SENDER  = 0xe5,
    REBOOT_SEQ        = 0xe6,
    REBOOT_SEQ_SENDER = 0xe7
  };


  virtual void log_sent_advertisement(unsigned seq_no, const Timestamp &when) = 0;
  virtual void log_start_recv_advertisement(unsigned seq_no, unsigned ip, const Timestamp &when) = 0;
  virtual void log_added_route(reason_t why, const GridGenericRouteTable::RouteEntry &r) = 0;
  virtual void log_added_route(reason_t why, const GridGenericRouteTable::RouteEntry &r,
			       const unsigned extra) = 0;
  virtual void log_expired_route(reason_t why, unsigned ip) = 0;
  virtual void log_triggered_route(unsigned ip) = 0;
  virtual void log_end_recv_advertisement() = 0;
  virtual void log_start_expire_handler(const Timestamp &when) = 0;
  virtual void log_end_expire_handler() = 0;
  virtual void log_route_dump(const Vector<GridGenericRouteTable::RouteEntry> &rt, const Timestamp &when) = 0;

  // assumes Grid packet
  virtual void log_tx_err(const Packet *p, int err, const Timestamp &when) = 0;
  virtual void log_no_route(const Packet *p, const Timestamp &when) = 0;

  virtual ~GridGenericLogger() { }
};

CLICK_ENDDECLS
#endif

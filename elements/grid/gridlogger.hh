#ifndef GRIDLOGGER_HH
#define GRIDLOGGER_HH
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include <clicknet/ether.h>
#include <clicknet/ip.h>
#include <clicknet/udp.h>
#include <click/string.hh>
#include <click/element.hh>
#include "grid.hh"
#include "gridgenericrt.hh"
#include "gridgenericlogger.hh"
CLICK_DECLS

/*
 * =c
 * GridLogger(I<KEYWORDS>)
 *
 * =s Grid
 * Log Grid-related events.
 *
 * =d
 *
 * This element provides methods which other Grid components can call
 * to log significant protocol events.
 *
 * Multiple GridLogger elements can co-exist.
 *
 * Keyword arguments are:
 *
 * =over 8
 *
 * =item LOGFILE
 *
 * String.  Filename of binary format log file.  If this argument is
 * supplied, the element starts logging to the specified file as soon
 * as the element is initialized.  Otherwise, no logging takes place
 * until the start_log handler is called.
 *
 * =item SHORT_IP
 *
 * Boolean.  Defaults to true.  If true, only log the last byte of IP
 * Addresses.
 *
 * =back
 *
 * =h start_log write-only
 *
 * When a filename is written, starts logging to that file.  Any existing logfile is closed.
 *
 * =h stop_log write-only
 *
 * Stop logging and close any open logfile.
 *
 * =h logfile read-only
 *
 * Return the filename of the current logfile, if any.  May return the empty string.
 *
 * =a
 * GridRouteTable, DSDVRouteTable, GridTxError, LookupLocalGridRoute
 */

class GridLogger : public GridGenericLogger {

  enum state_t {
    WAITING,
    RECV_AD,
    EXPIRE_HANDLER
  };

  state_t _state;

  int _fd;
  String _fn;
  bool _log_full_ip;

  unsigned char _buf[1024];
  size_t _bufptr; // index of next byte available in buf

  bool check_state(state_t s) {
    assert(_state == s);
    return _state == s;
  }

  bool check_space(size_t needed) {
    size_t avail = sizeof(_buf) - _bufptr;
    if (avail < needed) {
      click_chatter("GridLogger %s: log buffer is too small.  total buf size: %u, needed at least %u",
		    name().c_str(), sizeof(_buf), needed + _bufptr);
      return false;
    }
    return true;
  }
  void write_buf() {
    if (!log_is_open()) {
      _bufptr = 0;
      return; // no log active now
    }
    int res = write(_fd, _buf, _bufptr);
    if (res < 0)
      click_chatter("GridLogger %s: error writing log buffer: %s",
		    name().c_str(), strerror(errno));
    else if (res != (int) _bufptr)
      click_chatter("GridLogger %s: bad write to log buffer, had %u bytes in buffer but wrote %d bytes",
		    name().c_str(), (unsigned) _bufptr, res);
    _bufptr = 0;
  }
  void clear_buf() { _bufptr = 0; }
  size_t bufsz() { return _bufptr; }
  void add_bytes(void *bytes, size_t n) {
    if (!check_space(n))
      return;
    memcpy(_buf + _bufptr, bytes, n);
    _bufptr += n;
  }
  void add_one_byte(unsigned char c) { add_bytes(&c, 1); }
  void add_ip(unsigned ip) {
    if (_log_full_ip)
      add_bytes(&ip, sizeof(ip));
    else
      add_one_byte(ntohl(ip) & 0xff);
  }
  void add_long(unsigned long v) {
    v = htonl(v);
    add_bytes(&v, sizeof(v));
  }
  void dump_buf() {
    for (size_t i = 0; i < _bufptr; i++)
      click_chatter("XXXX %x ", (unsigned) _buf[i]);
  }
  void add_timeval(struct timeval tv) {
    tv.tv_sec = htonl(tv.tv_sec);
    tv.tv_usec = htonl(tv.tv_usec);
    add_bytes(&tv, sizeof(tv));
  }

  void log_pkt(struct click_ether *eh) {
    struct grid_hdr *gh = (struct grid_hdr *) (eh + 1);
    add_one_byte(gh->type);
    switch (gh->type) {
    case grid_hdr::GRID_LR_HELLO:
    case grid_hdr::GRID_HELLO: {
      struct grid_hello *hlo = (struct grid_hello *) (gh + 1);
      add_long(ntohl(hlo->seq_no));
      break;
    }
    case grid_hdr::GRID_NBR_ENCAP: {
      struct grid_nbr_encap *nbr = (struct grid_nbr_encap *) (gh + 1);
      add_ip(gh->ip);
      add_ip(nbr->dst_ip);
      add_bytes(eh->ether_dhost, 6);
      log_special_pkt((struct click_ip *) (nbr + 1));
      break;
    }
    default:
      ; /* nothing */
    }
  }

  void log_special_pkt(struct click_ip *ip) {
    bool special = false;
    if (ip->ip_p == IP_PROTO_UDP) {
      struct click_udp *udp = (struct click_udp *) (ip + 1);
      if (udp->uh_dport == htons(8021)) {
	// ahh, it's an experiment packet, get the seqno
	special = true;
	unsigned char *data = (unsigned char *) (udp + 1);
	int num_sent = 0;
	memcpy(&num_sent, data, 4);
	add_one_byte(SPECIAL_PKT_CODE);
	add_long(num_sent);
      }
    }
    if (!special)
      add_one_byte(BORING_PKT_CODE);
  }

public:

  GridLogger() CLICK_COLD;
  ~GridLogger() CLICK_COLD;

  const char *class_name() const { return "GridLogger"; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const { return false; }
  void *cast(const char *);
  void add_handlers() CLICK_COLD;

  // handlers
  static String read_logfile(Element *, void *);
  static int write_start_log(const String &, Element *, void *, ErrorHandler *);
  static int write_stop_log(const String &, Element *, void *, ErrorHandler *);

  bool open_log(const String &);
  void close_log();
  bool log_is_open() { return _fd >= 0; }

private:
  // these must be distinct from the inherited reason_t values
  static const unsigned char SENT_AD_CODE               = 0x01;
  static const unsigned char BEGIN_RECV_CODE            = 0x02;
  static const unsigned char END_RECV_CODE              = 0x03;
  static const unsigned char BEGIN_EXPIRE_CODE          = 0x04;
  static const unsigned char END_EXPIRE_CODE            = 0x05;
  static const unsigned char TRUNCATED_CODE             = 0x06;
  static const unsigned char RECV_ADD_ROUTE_CODE        = 0x07;
  static const unsigned char RECV_TRIGGER_ROUTE_CODE    = 0x08;
  static const unsigned char RECV_EXPIRE_ROUTE_CODE     = 0x09;
  static const unsigned char ROUTE_DUMP_CODE            = 0x0A;
  static const unsigned char TX_ERR_CODE                = 0x0B;
  static const unsigned char NO_ROUTE_CODE              = 0x0C;
  static const unsigned char SPECIAL_PKT_CODE           = 0x0D;
  static const unsigned char BORING_PKT_CODE            = 0x0E;
  static const unsigned char RECV_ADD_ROUTE_CODE_EXTRA  = 0x0F;

public:

  void log_sent_advertisement(unsigned seq_no, const Timestamp &when) {
    if (!check_state(WAITING))
      return;
    if (!check_space(1 + sizeof(seq_no) + sizeof(when)))
      return;
    add_one_byte(SENT_AD_CODE);
    add_long(seq_no);
    add_timeval(when.timeval());
    write_buf();
  }

  void log_start_recv_advertisement(unsigned seq_no, unsigned ip, const Timestamp &when) {
    if (!check_state(WAITING))
      return;
    _state = RECV_AD;
    add_one_byte(BEGIN_RECV_CODE);
    add_ip(ip);
    add_long(seq_no);
    add_timeval(when.timeval());
  }

  void log_added_route(reason_t why, const GridGenericRouteTable::RouteEntry &r) {
    if (!check_state(RECV_AD))
      return;
    add_one_byte(RECV_ADD_ROUTE_CODE);
    add_one_byte(why);
    add_ip(r.dest_ip);
    add_ip(r.next_hop_ip);
    add_one_byte(r.num_hops());
    add_long(r.seq_no());
  }

  void log_added_route(reason_t why, const GridGenericRouteTable::RouteEntry &r, const unsigned extra) {
    if (!check_state(RECV_AD))
      return;
    add_one_byte(RECV_ADD_ROUTE_CODE_EXTRA);
    add_one_byte(why);
    add_ip(r.dest_ip);
    add_ip(r.next_hop_ip);
    add_one_byte(r.num_hops());
    add_long(r.seq_no());
    add_long(extra);
  }

  void log_expired_route(reason_t why, unsigned ip) {
    if (_state != RECV_AD && _state != EXPIRE_HANDLER) {
      assert(0);
      return;
    }
    if (_state == RECV_AD) {
      add_one_byte(RECV_EXPIRE_ROUTE_CODE);
      add_ip(ip);
    }
    else {
      add_one_byte(why);
      add_ip(ip);
    }
  }

  void log_triggered_route(unsigned ip) {
    if (!check_state(RECV_AD))
      return;
    add_one_byte(RECV_TRIGGER_ROUTE_CODE);
    add_ip(ip);
  }

  void log_end_recv_advertisement() {
    if (!check_state(RECV_AD))
      return;
    _state = WAITING;
    add_one_byte(END_RECV_CODE);
    write_buf();
  }

  void log_start_expire_handler(const Timestamp &when) {
    if (!check_state(WAITING))
      return;
    _state = EXPIRE_HANDLER;
    add_one_byte(BEGIN_EXPIRE_CODE);
    add_timeval(when.timeval());
  }

  void log_end_expire_handler() {
    if (!check_state(EXPIRE_HANDLER))
      return;
    _state = WAITING;
    add_one_byte(END_EXPIRE_CODE);
    if (bufsz() <= 2 + sizeof(struct timeval))
      clear_buf(); // don't log if nothing actually expired
    else
      write_buf();
  }

  void log_route_dump(const Vector<GridGenericRouteTable::RouteEntry> &rt, const Timestamp &when) {
    if (!check_state(WAITING))
      return;
    add_one_byte(ROUTE_DUMP_CODE);
    add_timeval(when.timeval());
    int n = rt.size();
    add_long(n);
    for (int i = 0; i < rt.size(); i++) {
      const GridGenericRouteTable::RouteEntry &r = rt[i];
      add_ip(r.dest_ip);
      add_ip(r.next_hop_ip);
      add_one_byte(r.num_hops());
      add_long(r.seq_no());
    }
    write_buf();
  }

  // assumes Grid packet
  void log_tx_err(const Packet *p, int err, const Timestamp &when) {
    if (!check_state(WAITING))
      return;
    struct click_ether *eh = (click_ether *) (p->data());
     if (eh->ether_type != htons(ETHERTYPE_GRID))
      return;
    add_one_byte(TX_ERR_CODE);
    add_timeval(when.timeval());
    add_long((unsigned long) err);
    log_pkt(eh);
    write_buf();
  }

  void log_no_route(const Packet *p, const Timestamp &when) {
    if (!check_state(WAITING))
      return;
    struct click_ether *eh = (click_ether *) (p->data());
    if (eh->ether_type != htons(ETHERTYPE_GRID))
      return;
    add_one_byte(NO_ROUTE_CODE);
    add_timeval(when.timeval());
    log_pkt(eh);
    write_buf();
  }

};

CLICK_ENDDECLS
#endif

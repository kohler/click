#ifndef GRIDLOGGER_HH
#define GRIDLOGGER_HH

#include <fcntl.h>
#include "gridroutetable.hh"

class GridLogger {
  
  GridLogger() { }

  enum state_t {
    NOT_READY,
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

  bool check_space(size_t needed) {
    size_t avail = sizeof(_buf) - _bufptr;
    if (avail < needed) {
      click_chatter("GridLogger: log buffer is too small.  total buf size: %u, needed at least %u",
		    sizeof(_buf), needed + _bufptr);
      return false;
    }
    return true;
  }
  void write_buf() {
    int res = write(_fd, _buf, _bufptr);
    if (res < 0)
      click_chatter("GridLogger: error writing log buffer: %s",
		    strerror(errno));
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
  void add_timeval(struct timeval tv) {
    tv.tv_sec = htonl(tv.tv_sec);
    tv.tv_usec = htonl(tv.tv_usec);
    add_bytes(&tv, sizeof(tv));
  }

public:

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

  // these need to be different than the above codes
  enum reason_t {
    WAS_SENDER        = 0xf1,
    WAS_ENTRY         = 0xf2,
    BROKEN_AD         = 0xf3,
    TIMEOUT           = 0xf4,
    NEXT_HOP_EXPIRED  = 0xf5
  };

  GridLogger(const String &filename, bool log_full_ip = false) 
    : _state(NOT_READY), _fd(-1), _fn(filename),
      _log_full_ip(log_full_ip), _bufptr(0) {
    _fd = open(_fn.cc(), O_WRONLY | O_CREAT, 0777);
    if (_fd == -1) {
      click_chatter("GridLogger: unable to open log file %s, %s",
		    _fn.cc(), strerror(errno));
      return;
    }
    _state = WAITING;
    click_chatter("GridLogger: started logging to %s", _fn.cc());
  }

  bool ok() { return _state != NOT_READY; }

  void log_sent_advertisement(unsigned seq_no, struct timeval when) { 
    if (_state != WAITING) 
      return;
    if (!check_space(1 + sizeof(seq_no) + sizeof(when)))
      return;
    add_one_byte(SENT_AD_CODE);
    add_long(seq_no);
    add_timeval(when);
    write_buf();
  }

  void log_start_recv_advertisement(unsigned seq_no, unsigned ip, struct timeval when) {
    if (_state != WAITING) 
      return;
    _state = RECV_AD;
    add_one_byte(BEGIN_RECV_CODE);
    add_ip(ip);
    add_long(seq_no);
    add_timeval(when);
  }
  
  void log_added_route(reason_t why, const GridRouteTable::RTEntry &r) {
    if (_state != RECV_AD) 
      return;
    add_one_byte(RECV_ADD_ROUTE_CODE);
    add_one_byte(why);
    add_ip(r.dest_ip);
    add_ip(r.next_hop_ip);
    add_one_byte(r.num_hops);
    add_long(r.seq_no);
  }

  void log_expired_route(reason_t why, unsigned ip) {
    if (_state != RECV_AD && _state != EXPIRE_HANDLER) 
      return;
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
    if (_state != RECV_AD) 
      return;
    add_one_byte(RECV_TRIGGER_ROUTE_CODE);
    add_ip(ip);
  }

  void log_end_recv_advertisement() {
    if (_state != RECV_AD) 
      return;
    _state = WAITING;
    add_one_byte(END_RECV_CODE);
    write_buf();
  }

  void log_start_expire_handler(struct timeval when) {
    if (_state != WAITING) 
      return;
    _state = EXPIRE_HANDLER;
    add_one_byte(BEGIN_EXPIRE_CODE);
    add_timeval(when);
  }

  void log_end_expire_handler() {
    if (_state != EXPIRE_HANDLER) 
      return;
    _state = WAITING;
    add_one_byte(END_EXPIRE_CODE);
    if (bufsz() <= 2 + sizeof(struct timeval))
      clear_buf(); // don't log if nothing actually expired
    else
      write_buf();
  }

  void log_route_dump(const GridRouteTable::RTable &rt, struct timeval when) {
    if (_state != WAITING)
      return;
    add_one_byte(ROUTE_DUMP_CODE);
    add_timeval(when);
    int n = rt.size();
    add_long(n);
    for (GridRouteTable::RTIter i = rt.first(); i; i++) {
      const GridRouteTable::RTEntry &r = i.value();
      add_ip(r.dest_ip);
      add_ip(r.next_hop_ip);
      add_one_byte(r.num_hops);
      add_long(r.seq_no);
    }
    write_buf();
  }

  ~GridLogger() {
    if (_fd != -1) {
      close(_fd);
      click_chatter("GridLogger: stopped logging on %s",_fn.cc());
    }
  }
};

#endif

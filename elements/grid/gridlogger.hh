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
      add_one_byte(ip & 0xff);
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

  static const unsigned char SENT_AD_CODE = 0x1;
  static const unsigned char BEGIN_RECV_CODE = 0x2;
  static const unsigned char END_RECV_CODE = 0x3;
  static const unsigned char BEGIN_EXPIRE_CODE = 0x4;
  static const unsigned char END_EXPIRE_CODE = 0x5;
  static const unsigned char TRUNCATED_CODE = 0x6;
  static const unsigned char RECV_ADD_ROUTE_CODE = 0x7;
  static const unsigned char RECV_TRIGGER_ROUTE_CODE = 0x8;
  static const unsigned char RECV_EXPIRE_ROUTE_CODE = 0x9;

  // these need to be different than the above codes
  enum reason_t {
    WAS_SENDER = 101,
    WAS_ENTRY = 102,
    BROKEN_AD = 103,
    TIMEOUT = 104,
    NEXT_HOP_EXPIRED = 105
  };

  GridLogger(const String &filename, bool log_full_ip = false) 
    : _state(NOT_READY), _fd(-1), _log_full_ip(log_full_ip), _bufptr(0) {
    _fd = open(((String &) filename).cc(), O_WRONLY | O_CREAT, 0004);
    if (_fd == -1) {
      click_chatter("GridLogger: unable to open log file %s, %s",
		    ((String &) filename).cc(), strerror(errno));
      return;
    }
    _state = WAITING;
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
    write_buf();
  }

  ~GridLogger() {
    if (_fd != -1)
      close(_fd);
  }
};

#endif

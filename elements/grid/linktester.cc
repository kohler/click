/*
 * linktester.{cc,hh} -- probe wireless links
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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

#include <fcntl.h>
#include <click/config.h>
#include <click/confparse.hh>
#include "linktester.hh"
#include <click/glue.hh>
#include <clicknet/ether.h>
#include <click/error.hh>
#include <click/router.hh>
#include "timeutils.hh"
CLICK_DECLS

LinkTester::LinkTester() :
  _start_time(-1),
  _timer(static_timer_hook, this), 
  _curr_state(WAITING_TO_START),
  _iterations_done(0),
  _num_iters(1),
  _pad(10000),
  _packet_size(sizeof(click_ether) + sizeof(payload_t)),
  _send_time(10000),
  _lambda(1),
  _bcast_packet_size(sizeof(click_ether) + sizeof(payload_t)),
  _bcast_send_time(10000),
  _bcast_lambda(1),
  _data_buf(0)
{
  MOD_INC_USE_COUNT;
  add_output();
}

LinkTester::~LinkTester()
{
  MOD_DEC_USE_COUNT;
  if (_data_buf)
    delete[] _data_buf;
}

int
LinkTester::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpEthernetAddress, "source ethernet address", &_src_eth,
			cpEthernetAddress, "destination ethernet address", &_dst_eth,
			cpKeywords,
			"START_TIME", cpInteger, "start time (unix time_t)", &_start_time,
			"ITERATIONS", cpUnsigned, "number of iterations", &_num_iters,
			"SEND_FIRST", cpBool, "send first?", &_send_first,
			"PAD_TIME", cpUnsigned, "milliseconds between each phase", &_pad,
			"UNICAST_SEND_TIME", cpUnsigned, "time to send unicast backets (milliseconds)", &_send_time,
			"BROADCAST_SEND_TIME", cpUnsigned, "time to send broadcast backets (milliseconds)", &_bcast_send_time,
			"UNICAST_PACKET_SZ", cpUnsigned, "total size of unicast backets (bytes)", &_packet_size,
			"BROADCAST_PACKET_SZ", cpUnsigned, "total size of broadcast backets (bytes)", &_bcast_packet_size,
			"UNICAST_LAMBDA", cpDouble, "unicast inter-packet spacing lambda parameter", &_lambda,
			"BROADCAST_LAMBDA", cpDouble, "broadcast inter-packet spacing lambda parameter", &_bcast_lambda,
			cpEnd);

  if (res > -1 && experiment_params_ok(errh))
    return 1;
  else
    return -1;
}

// check consistency and viability of user-supplied parameters
bool
LinkTester::experiment_params_ok(ErrorHandler *errh)
{
  if (_packet_size < sizeof(click_ether) + sizeof(payload_t)) {
    errh->error("Unicast packets too small for ether header and sequence numbers");
    return false;
  }
  if (_bcast_packet_size < sizeof(click_ether) + sizeof(payload_t)) {
    errh->error("Broadcast packets too small for ether header and sequence numbers");
    return false;
  }
  return true;
}

int
LinkTester::initialize(ErrorHandler *errh)
{
  unsigned int biggest = _packet_size > _bcast_packet_size ?
    _packet_size : _bcast_packet_size;
  unsigned int data_sz = biggest - sizeof(click_ether) - sizeof(payload_t);
  _data_buf = new unsigned char[data_sz];
  if (!_data_buf)
    return errh->error("Unable to allocate data buffer");
  for (unsigned int i = 0; i < data_sz; i++)
    _data_buf[i] = i % 256;

  bool res = init_random();
  if (!res)
    return errh->error("Unable to initialize random number generator");

  struct timeval tv;
  click_gettimeofday(&tv);

  if (_start_time < 0) // start ``immediately''
    _start_time = tv.tv_sec + 5;

  if (tv.tv_sec >= (int) _start_time)
    return errh->error("Start time %u has alread passed", _start_time);

  _timer.initialize(this);
  _start_time_tv.tv_sec = _start_time;
  _start_time_tv.tv_usec = 0;
  _timer.schedule_at(_start_time_tv);

  _last_time = tv;
  _next_time = _start_time_tv;

  return 0;
}

void
LinkTester::timer_hook()
{
  struct timeval tv;
  click_gettimeofday(&tv);
#if 1
  click_chatter("OK %s    (delta: %s)\n", timeval_to_str(tv).cc(),
		timeval_to_str(tv - _last_time).cc());
#endif
  _last_time = tv;
  switch (_curr_state) {
  case WAITING_TO_START: handle_timer_waiting(tv); break;
  case LISTENING: handle_timer_listening(tv); break;
  case BCAST_1: handle_timer_bcast(tv); break;
  case UNICAST: handle_timer_unicast(tv); break;
  case BCAST_2: handle_timer_bcast(tv); break;
  case DONE:
  default:
    assert(0);
  }
}

void 
LinkTester::handle_timer_waiting(const struct timeval &tv)
{
  assert(_curr_state == WAITING_TO_START);
  _iterations_done = 0;

  if (_num_iters == 0) {
    finish_experiment();
    return;
  }

  if (_send_first) {
    _curr_state = BCAST_1;
    _bcast_packets_sent = 0;
    handle_timer_bcast(tv);
  }
  else {
    _curr_state = LISTENING;
    int listen_for = calc_listen_time() + calc_pad_time();
    _next_time.tv_sec = _start_time + (listen_for / 1000);
    _next_time.tv_usec = 1000 * (listen_for % 1000);
    
    assert(_next_time > tv);
    _timer.schedule_at(_next_time);
  }
}

void 
LinkTester::handle_timer_listening(const struct timeval &tv)
{
  assert(_curr_state == LISTENING);

  // check for end of experiment
  if (_send_first) {
    _iterations_done++;
    if (_iterations_done >= _num_iters) {
      finish_experiment();
      return;
    }
  }

  _curr_state = BCAST_1;
  _bcast_packets_sent = 0;
  handle_timer_bcast(tv);
}

void 
LinkTester::handle_timer_bcast(const struct timeval &tv)
{
  assert(_curr_state == BCAST_1 || _curr_state == BCAST_2);
  send_broadcast_packet((unsigned short) _bcast_packet_size, tv, 
			_curr_state == BCAST_1, _bcast_packets_sent, 
			_iterations_done);
  _bcast_packets_sent++;

  // when would we like to send the next bcast packet?
  unsigned int delta = draw_random_msecs(_bcast_lambda);
  struct timeval new_next_time = _next_time + msecs_to_timeval(delta);
  
  // is there enough time left to send the next packet?
  if (new_next_time <= last_bcast_time(_iterations_done,
				       _curr_state == BCAST_1)) {
    _next_time = new_next_time;
  }
  else {
    // switch to the next phase
    if (_curr_state == BCAST_1) {
      _curr_state = UNICAST;
      _packets_sent = 0;
      _next_time = first_unicast_time(_iterations_done);
    }
    else {
      assert(_curr_state == BCAST_2);
      // we just completed the second set of broadcasts.  possible
      // outcomes are:
      // 1. listen at the end of this iteration
      // 2. listen at the beginning of the next iteration
      // 3. quit, experiment over...
      if (_send_first) {
	_curr_state = LISTENING;
	_next_time = first_bcast_time(_iterations_done + 1, true);
      }
      else {
	_iterations_done++;
	if (_iterations_done >= _num_iters) {
	  finish_experiment();	  
	  return;
	}
	else {
	  _curr_state = LISTENING;
	  _next_time = first_bcast_time(_iterations_done, true);
	}
      }
    }
  }

  assert(_next_time > tv);
  _timer.schedule_at(_next_time);
  return;    
}
 
void 
LinkTester::handle_timer_unicast(const struct timeval &tv)
{
  assert(_curr_state == UNICAST);
  send_unicast_packet(tv, _packets_sent, _iterations_done);
  _packets_sent++;

  unsigned int delta = draw_random_msecs(_lambda);
  struct timeval new_next_time = _next_time + msecs_to_timeval(delta);

  // is there enough time left to sent the next packet?
  if (new_next_time <= last_unicast_time(_iterations_done))
    _next_time = new_next_time;
  else {
    // switch to next phase
    _curr_state = BCAST_2;
    _bcast_packets_sent = 0;
    _next_time = first_bcast_time(_iterations_done, false);
  }

  assert(_next_time > tv);
  _timer.schedule_at(_next_time);
}

unsigned int
LinkTester::calc_listen_time() 
{
  return calc_bcast_time() + calc_pad_time() + calc_unicast_time() 
    + calc_pad_time() + calc_bcast_time();
}

unsigned int
LinkTester::calc_unicast_time()
{
  return _send_time;
}

unsigned int
LinkTester::calc_bcast_time()
{
  return _bcast_send_time;
}

struct timeval
LinkTester::first_unicast_time(unsigned int iter)
{
  unsigned int iter_time = 2 * (calc_listen_time() + calc_pad_time());
  unsigned int delta = iter_time * iter;
  delta += calc_bcast_time() + calc_pad_time();
  if (!_send_first) // let other node send first
    delta += calc_listen_time() + calc_pad_time();
  return msecs_to_timeval(delta) + _start_time_tv;
}

struct timeval
LinkTester::first_bcast_time(unsigned int iter, bool before)
{
  unsigned int iter_time = 2 * (calc_listen_time() + calc_pad_time());
  unsigned int delta = iter_time * iter;
  if (!_send_first) // let other node send first
    delta += calc_listen_time() + calc_pad_time();
  if (!before)
    delta += calc_bcast_time() + calc_pad_time() + calc_unicast_time() + calc_pad_time();
  return msecs_to_timeval(delta) + _start_time_tv;
}

struct timeval
LinkTester::last_unicast_time(unsigned int iter)
{
  return first_unicast_time(iter) + msecs_to_timeval(calc_unicast_time());
}

struct timeval
LinkTester::last_bcast_time(unsigned int iter, bool before)
{
  return first_bcast_time(iter, before) + msecs_to_timeval(calc_bcast_time());
}

void
LinkTester::send_unicast_packet(const struct timeval &tv,
				unsigned int seq, unsigned int iter)
{
  WritablePacket *p = Packet::make(_packet_size);
  click_ether *eh = (click_ether *) (p->data());

  memcpy(eh->ether_dhost, _dst_eth.data(), 6);
  memcpy(eh->ether_shost, _src_eth.data(), 6);
  eh->ether_type = htons(ETHERTYPE);

  payload_t *payload = (payload_t *) (eh + 1);
  memset(payload, 0, sizeof(payload_t));
  payload->size = htons(_packet_size);
  payload->iteration = htonl(iter);
  payload->seq_no = htonl(seq);
  payload->tx_sec = htonl(tv.tv_sec);
  payload->tx_usec = htonl(tv.tv_usec);

  unsigned int data_sz = _packet_size - sizeof(click_ether) - sizeof(payload_t);
  if (data_sz > 0)
    memcpy(p->data() + sizeof(click_ether) + sizeof(payload_t), 
	   _data_buf, data_sz);
  output(0).push(p);
}

void
LinkTester::send_broadcast_packet(unsigned short psz, const struct timeval &tv,
				  bool before, unsigned int seq, unsigned int iter)
{
  assert(psz >= sizeof(click_ether) + sizeof(payload_t));
  WritablePacket *p = Packet::make(psz);
  click_ether *eh = (click_ether *) (p->data());
  
  static unsigned char bcast_addr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  memcpy(eh->ether_dhost, bcast_addr, 6);
  memcpy(eh->ether_shost, _src_eth.data(), 6);
  eh->ether_type = htons(ETHERTYPE);

  payload_t *payload = (payload_t *) (eh + 1);
  memset(payload, 0, sizeof(payload_t));
  payload->size = htons(psz);
  payload->before = before ? 1 : 0;
  payload->iteration = htonl(iter);
  payload->seq_no = htonl(seq);
  payload->tx_sec = htonl(tv.tv_sec);
  payload->tx_usec = htonl(tv.tv_usec);

  unsigned int data_sz = psz - sizeof(click_ether) - sizeof(payload_t);
  if (data_sz > 0)
    memcpy(p->data() + sizeof(click_ether) + sizeof(payload_t), 
	   _data_buf, data_sz);
  output(0).push(p);
}

void
LinkTester::finish_experiment() 
{
  click_chatter("DONE\n");
  router()->please_stop_driver();
}

struct timeval
LinkTester::msecs_to_timeval(unsigned int msecs)
{
  struct timeval tv;
  tv.tv_sec = msecs / 1000;
  tv.tv_usec = 1000 * (msecs % 1000);
  return tv;
}

bool
LinkTester::init_random()
{
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd == -1)
    return false;
  
  unsigned long seed;
  int err = read(fd, &seed, sizeof(seed));
  if (err != sizeof(seed))
    return false;
  
  close(fd);
  srandom(seed);
  return true;
}

double
LinkTester::draw_random(double lambda)
{
  // draw an exponentially distributed variable with parameter lambda
#if 0
  double r = (double) random() / (double) RAND_MAX;
  return -lambda * log(r);
#else
  lambda = 0;
  return 100;
#endif
}

EXPORT_ELEMENT(LinkTester)
ELEMENT_REQUIRES(userlevel)

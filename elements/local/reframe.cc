/*
 * reframe.{cc,hh} -- reframes a fragmented stream of packets
 *
 * Copyright (C) 2005  The Trustees of Princeton University
 * Mark Huang <mlhuang@cs.princeton.edu>
 *
 * $Id: reframe.cc,v 1.2 2006/06/21 21:10:32 eddietwo Exp $
 */

#include <click/config.h>
#include "reframe.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>

CLICK_DECLS

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define alignto(m, n) (((m)+(n)-1)/(n)*(n))

Reframe::Reframe()
  : _foff(2), _flen(2), _ntoh(true), _mul(1), _align(1), _add(0),
    _header(0), _qhead(0), _qtail(0), _have(0), _need(-1)
{
}

Reframe::~Reframe()
{
}

int
Reframe::configure(Vector<String> &conf, ErrorHandler *errh)
{
  // remove keyword arguments
  if (Args(this, errh).bind(conf)
      .read("FOFF", _foff)
      .read("FLEN", _flen)
      .read("NTOH", _ntoh)
      .read("MUL", _mul)
      .read("ALIGN", _align)
      .read("ADD", _add)
      .consume() < 0)
    return -1;

  switch (_flen) {
  case 0: case 1: case 2: case 4:
    break;
  default:
    return errh->error("invalid field length %d (valid field lengths are 0, 1, 2, and 4 bytes)", _flen);
  }

  return 0;
}

int
Reframe::initialize(ErrorHandler *errh)
{
  if (_flen) {
    // make a header packet big enough to hold the encoded length field
    _header = Packet::make(_foff + _flen);
    if (!_header) {
      return errh->error("out of memory");
    }
    _header->take(_header->length());
  }
  return 0;
}

void
Reframe::cleanup(CleanupStage)
{
  if (_header) {
    _header->kill();
  }
  while (_qhead) {
    Packet *p = _qhead;
    _qhead = p->next();
    p->kill();
  }
}

void
Reframe::reframe(void)
{
  if (_flen) {
    if (_have >= (_foff + _flen)) {
      // we have enough to examine the header if we have not already
      if (_header->length() == 0) {
	assert(_qhead);
	// fill header
	for (Packet *p = _qhead;
	     p && (int) _header->length() < (_foff + _flen);
	     p = p->next()) {
	  memcpy(_header->end_data(), p->data(),
		 MIN(p->length(), _foff + _flen - _header->length()));
	  _header = _header->put(MIN(p->length(), _foff + _flen - _header->length()));
	}
      }

      // avoid alignment problems
      switch (_flen) {
      case 1: {
	uint8_t len;
	memcpy(&len, _header->data() + _foff, _flen);
	_need = _mul * len;
	break;
      }
      case 2: {
	uint16_t len;
	memcpy(&len, _header->data() + _foff, _flen);
	_need = _mul * (_ntoh ? ntohs(len) : len);
	break;
      }
      case 4: {
	uint32_t len;
	memcpy(&len, _header->data() + _foff, _flen);
	_need = _mul * (_ntoh ? ntohl(len) : len);
	break;
      }
      default:
	assert(_flen == 1 || _flen == 2 || _flen == 4);
	break;
      }

      // we need this much to emit this frame
     _need = alignto(_need, _align) + _add;
    } else {
      // no idea how much we need yet
      _need = -1;
    }
  } else {
    // fixed size frame
    _need = _add;
  }
}

Packet *
Reframe::pull(int)
{
  if (_need >= 0 && _have >= _need) {
    // we have enough to emit a frame
    WritablePacket *p1 = Packet::make(_need);
    if (!p1) {
      return 0;
    }
    p1->take(_need);
    while (_need > 0) {
      assert(_qhead);
      Packet *p = _qhead;
      if ((int) p->length() > _need) {
	// too much
	memcpy(p1->end_data(), p->data(), _need);
	p1 = p1->put(_need);
	// save rest for later
	p->pull(_need);
	_have -= _need;
	_need = 0;
      } else {
	// not enough or just right
	memcpy(p1->end_data(), p->data(), p->length());
	p1 = p1->put(p->length());
	_have -= p->length();
	_need -= p->length();
	// done with this packet
	_qhead = p->next();
	if (!_qhead)
	  _qtail = 0;
	p->kill();
      }
    }
    // we filled the packet
    assert(_need == 0);
    // recalculate
    if (_header)
      _header->take(_header->length());
    reframe();
    return p1;
  }

  return 0;
}

void
Reframe::push(int, Packet *p)
{
  // queue packet
  if (_qtail) {
    assert(_qhead);
    _qtail->set_next(p);
  } else {
    assert(!_qhead);
    _qhead = p;
  }
  _qtail = p;
  p->set_next(0);
  _have += p->length();
  p = 0;

  // calculate how much we need
  reframe();

  while ((p = pull(0)))
    output(0).push(p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(Reframe)

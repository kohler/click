/*
 * tcpsynackctrl.{cc,hh} -- element checks balance between SYNs and ACKs
 * Thomer M. Gil
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "click_ip.h"
#include "click_tcp.h"
#include "tcpsynackctrl.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

TCPSynAckControl::TCPSynAckControl() : _hoc(0)
{
  add_input();
  add_input();
  add_output();
}

TCPSynAckControl::~TCPSynAckControl()
{
}

TCPSynAckControl *
TCPSynAckControl::clone() const
{
  return new TCPSynAckControl;
}

int
TCPSynAckControl::configure(const String &, ErrorHandler *)
{
  return 0;
}

void
TCPSynAckControl::push(int port_number, Packet *p)
{
  click_ip *ip = (click_ip *) p->data();
  if(ip->ip_p == IP_PROTO_TCP) {
    // Identify flow by addresses only. Not ports.
    IPAddress saddr(ip->ip_src);
    IPAddress daddr(ip->ip_dst);
    click_tcp *tcp = (click_tcp *)((unsigned *)ip + ip->ip_hl);
    unsigned short sport = tcp->th_sport;
    unsigned short dport = tcp->th_dport;
    IPFlowID flid(saddr, 0, daddr, 0);

    // Find all half open connections for this flid. May be 0.
    HalfOpenConnections *hocs = _hoc.find(flid);

    // For SYN packets : insert half-open connection.
    if(port_number == 0) {
      if(!hocs) {
        hocs = new HalfOpenConnections(saddr, daddr);
        _hoc.insert(flid, hocs);
      }
      hocs->add(sport, dport);

    // For ACK packets : connection is now open. Closes half-openness, but only
    // if hocs is non-zero (it might be zero when we never saw the SYN flying
    // by, or if connection is already full open). Delete entry if no more
    // half-open connections exist.
    } else if(hocs) {
      hocs->del(sport, dport);
      if(!hocs->amount()) {
        delete hocs;
        _hoc.insert(flid, 0);
      }
    }
  }

  output(0).push(p);
}


TCPSynAckControl::HalfOpenConnections::HalfOpenConnections(IPAddress saddr,
                                                           IPAddress daddr)
{
  _saddr = saddr.saddr();
  _daddr = daddr.saddr();
  _amount = 0;
  for(short i = 0; i < MAX_HALF_OPEN; i++) {
    _hops[i] = 0;
    _free_slots[MAX_HALF_OPEN-i-1] = i;
  }
}

TCPSynAckControl::HalfOpenConnections::~HalfOpenConnections()
{
  _saddr = 0;
  _daddr = 0;
  for(short i = 0; i < MAX_HALF_OPEN; i++) {
    if(_hops[i]) {
      delete _hops[i];
      _amount--;
      assert(_amount >= 0);
    }
  }
}


void
TCPSynAckControl::HalfOpenConnections::add(unsigned short sport, unsigned short dport)
{
  short index = _free_slots[MAX_HALF_OPEN - _amount - 1];
  assert(_hops[index] == 0);
  _hops[index] = new HalfOpenPorts;
  _hops[index]->sport = sport;
  _hops[index]->dport = dport;
  _amount++;
}

//
// XXX: Very stupid. Hash?
//
void
TCPSynAckControl::HalfOpenConnections::del(unsigned short sport,
                                           unsigned short dport)
{
  assert(_amount > 0);

  // Go through all occupied slots.
  for(short i = 0; i < MAX_HALF_OPEN; i++) {
    if(_hops[i] == 0)
      continue;

    // Is this the one? Delete.
    if(_hops[i]->sport == sport && _hops[i]->dport == dport) {
      delete _hops[i];
      _hops[i] = 0;
      _free_slots[MAX_HALF_OPEN - _amount] = i;
      _amount--;
    }
  }
}

EXPORT_ELEMENT(TCPSynAckControl)

#include "hashmap.cc"

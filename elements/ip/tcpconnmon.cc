/*
 * tcpconnmon.{cc,hh} -- element keeps track of half-open connections
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
#include "tcpconnmon.hh"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

TCPConnectionMonitor::TCPConnectionMonitor() : _hoc(0), _thresh(0)
{
  add_input();
  add_input();
  add_output();
}

TCPConnectionMonitor::~TCPConnectionMonitor()
{
}

TCPConnectionMonitor *
TCPConnectionMonitor::clone() const
{
  return new TCPConnectionMonitor;
}

int
TCPConnectionMonitor::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh,
		     cpUnsigned, "threshold", &_thresh,
		     0);
}



void
TCPConnectionMonitor::push(int port_number, Packet *p)
{
  const click_ip *ip = p->ip_header();
  if(ip->ip_p == IP_PROTO_TCP) {
    // Identify flow by addresses only. Not ports.
    IPAddress saddr(ip->ip_src);
    IPAddress daddr(ip->ip_dst);
    IPFlowID flid(saddr, 0, daddr, 0);

    const click_tcp *tcp = reinterpret_cast<const click_tcp *>(p->transport_header());
    unsigned short sport = tcp->th_sport;
    unsigned short dport = tcp->th_dport;

    // Find all half open connections for this src/dst. May be 0.
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

String
TCPConnectionMonitor::look_read_handler(Element *e, void *)
{
  TCPConnectionMonitor *me = (TCPConnectionMonitor *) e;
  String ret;
  
  // Go through all src/addr combi's and see if one has more than _thresh half
  // open connections.

  // For each src/dst combincation
  // Too mony half-open connections?
  // Print out portnumber for open connections.
  for (TCPConnectionMonitor::Table::Iterator iter = me->_hoc.first();
       iter; iter++) {
    IPFlowID flid = iter.key();
    HalfOpenConnections *hocs = iter.value();
    if(hocs != 0 && hocs->amount() >= me->_thresh)
      for(int j = 0; j < MAX_HALF_OPEN; j++) {
        HalfOpenPorts hops;
        if(hocs->half_open_ports(j, hops))
          ret += String(flid.saddr().addr()) + "\t" +
                 String(flid.daddr().addr()) + "\t" +
                 String((int) hops.sport) + "\t" +
                 String((int) hops.dport) + "\n";
      }
  }

  return ret;
}



String
TCPConnectionMonitor::thresh_read_handler(Element *e, void *)
{
  TCPConnectionMonitor *me = (TCPConnectionMonitor *) e;
  return String(me->_thresh) + "\n";
}


int
TCPConnectionMonitor::thresh_write_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);
  TCPConnectionMonitor* me = (TCPConnectionMonitor *) e;

  if(args.size() != 1) {
    errh->error("expecting 1 integer");
    return -1;
  }
  int thresh;
  if(!cp_integer(args[0], &thresh)) {
    errh->error("not an integer");
    return -1;
  }
  me->_thresh = thresh;
  return 0;
}



void
TCPConnectionMonitor::add_handlers()
{
  add_read_handler("thresh", thresh_read_handler, 0);
  add_write_handler("thresh", thresh_write_handler, 0);

  add_read_handler("look", look_read_handler, 0);
}



TCPConnectionMonitor::HalfOpenConnections::HalfOpenConnections(IPAddress saddr,
                                                           IPAddress daddr)
{
  _saddr = saddr.addr();
  _daddr = daddr.addr();
  _amount = 0;
  for(short i = 0; i < MAX_HALF_OPEN; i++) {
    _hops[i] = 0;
    _free_slots[MAX_HALF_OPEN-i-1] = i;
  }
}

TCPConnectionMonitor::HalfOpenConnections::~HalfOpenConnections()
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
TCPConnectionMonitor::HalfOpenConnections::add(unsigned short sport,
                                           unsigned short dport)
{
  if(_amount == MAX_HALF_OPEN)
    return;

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
TCPConnectionMonitor::HalfOpenConnections::del(unsigned short sport,
                                           unsigned short dport)
{
  assert(_amount > 0);

  // Go through all occupied slots. DUMB DUMB DUMB. Make hash.
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


bool
TCPConnectionMonitor::HalfOpenConnections::half_open_ports(int i, HalfOpenPorts &hops)
{
  if(_hops[i] == 0)
    return false;

  hops = *_hops[i];
  return true;
}

EXPORT_ELEMENT(TCPConnectionMonitor)

#include "hashmap.cc"

/*
 * addresstranslator{cc,hh} -- element that translates an IPv6 packet by
 * replacing with the src/dst IPv6 address and/or port by mapped IPv6address
 * and port.
 *
 * Peilei Fan
 *
 * Copyright (c) 1999-2001 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#include <click/config.h>
#include "addresstranslator.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <clicknet/ip.h>
#include <clicknet/ip6.h>
#include <clicknet/icmp.h>
#include <clicknet/icmp6.h>
#include <clicknet/tcp.h>
#include <clicknet/udp.h>
CLICK_DECLS

AddressTranslator::AddressTranslator()
  : _dynamic_mapping_allocation_direction(0),
    _in_map(0), _out_map(0), _rover(0), _rover2(0), _nmappings(0)
{
    // input 0: IPv6 arriving outward packets
    // input 1: IPv6 arriving inward packets
    // output 0: IPv6 outgoing outward packets with mapped addresses and port
    // output 1: IPv6 outgoing inward packets with mapped address and port
}


AddressTranslator::~AddressTranslator()
{
}

AddressTranslator::Mapping::Mapping()
  :  _prev(0), _next(0), _free_next(0)
{
}

void
AddressTranslator::cleanup(CleanupStage)
{
  if (_dynamic_mapping_allocation_direction==0)
    {
      clean_map(_in_map, 1);
      clean_map(_out_map, 0);
    }
  else
    {
      clean_map(_in_map, 0);
      clean_map(_out_map, 1);
    }
}

void
AddressTranslator::mapping_freed(Mapping *m, bool major_map)
{
  if (major_map)
  {
    if (_rover == m) {
      _rover = m->get_next();
      if (_rover == m)
	_rover = 0;
    }

    _nmappings--;
    if (_nmappings) {
      Mapping * m_prev = m->free_next();
      m_prev->set_next(m->get_next());
    }
  }

 else
   {
     if(_rover2 == m) {
       _rover2 = m->get_next();
       if (_rover2 == m)
	 _rover2 = 0;
     }
     _nmappings2--;
    if (_nmappings2) {
      Mapping * m_prev = m->free_next();
      m_prev->set_next(m->get_next());
    }
   }
}

void
AddressTranslator::clean_map(Map6 &table, bool major_map)
{

  Mapping *to_free=0;
  for (Map6::iterator iter = table.begin(); iter.live(); iter++) {
    Mapping *m = iter.value();
    m->set_free_next(to_free);
    to_free = m;
  }

  while (to_free) {
    Mapping *next = to_free->free_next();
    mapping_freed(to_free, major_map);
    delete to_free;
    to_free = next;
  }
  table.clear();
}

unsigned short
AddressTranslator::find_mport( )
{
  if (_mportl == _mporth || !_rover)
    return _mportl;

  Mapping * r = _rover;
  Mapping * r2 = _rover2;
  unsigned short this_mport = r->sport();
  //click_chatter("in find_mport(), this_mport is %x", this_mport);

  do {
    Mapping *next = r->get_next();
    Mapping *next2 = r2->get_next();
    unsigned short next_mport = next->sport();
    if (next_mport > this_mport +1)
      {
      goto found;
     }
    else if (next_mport <= this_mport) {
      if (this_mport < _mporth)
	{
	  goto found;
	}
      else if (next_mport > _mportl) {
	this_mport = _mportl -1;
	goto found;
      }
    }
    r = next;
    r2=next2;
    this_mport = next_mport;
  } while (r!=_rover);

  return 0;

 found:
  _rover = r;
  _rover2 = r2;
  return this_mport+1;

}

//add an entry to the mapping table for dynamic mapping
void
AddressTranslator::add_map(IP6Address &mai,  bool binding)
{
  struct EntryMap e;

   if (_dynamic_mapping_allocation_direction ==0)  //outward address allocation
    {
      e._mai = mai;
    }
   else //inward address allocation
     {
       e._iai = mai;
     }
  e._binding = binding;
  e._static = false;
  _v.push_back(e);
}

//add an entry to the mapping table for static address (and port) mapping
void
AddressTranslator::add_map(IP6Address &iai, unsigned short ipi, IP6Address &mai, unsigned short mpi, IP6Address &ea, unsigned short ep, bool binding)
{
 struct EntryMap e;
 if (_static_mapping[0])
   e._iai = iai;
 if (_static_mapping[1])
   e._ipi = ipi;
 if (_static_mapping[2])
   e._mai = mai;

 if (_static_mapping[3])
   e._mpi = mpi;
 if (_static_mapping[4])
   e._ea = ea;
 if (_static_mapping[5])
   e._ep = ep;

 e._binding = binding;
 e._static = true;
 _v.push_back(e);
}

int
AddressTranslator::configure(Vector<String> &conf, ErrorHandler *errh)
{
  _v.clear();
  int s = 0;
  IP6Address ia, ma, ea;
  int ip, mp, ep = 0;


  //get the static mapping entries for the mapping table
  if (!IntArg().parse(conf[0], _number_of_smap))
      return errh->error("argument %d should be : integer", 0);

  if (_number_of_smap==0)
    s = 1;

  else
    {

      s = _number_of_smap+2;
      if (!BoolArg().parse(conf[1], _static_portmapping))
	errh->error("argument %d should be : bool", 2);

      _static_mapping[0] = 1;
      _static_mapping[1] = 0;
      _static_mapping[2] = 1;
      _static_mapping[3] = 0;
      _static_mapping[4] = 0;
      _static_mapping[5] = 0;

      if (_static_portmapping)
	{
	  _static_mapping[1] = 1;
	  _static_mapping[3] = 1;
	}

      for (int i = 0; i<_number_of_smap; i++)
	{
	  Vector<String> words0;
	  cp_spacevec(conf[2+i], words0);
	  int j = 0;
	  if (_static_mapping[0] ==1)
	    cp_ip6_address(words0[j],(unsigned char *)&ia);
	  if (_static_mapping[1] ==1)
	      IntArg().parse(words0[++j], ip);
	  if (_static_mapping[2] ==1)
	    cp_ip6_address(words0[++j],(unsigned char *)&ma);
	  if (_static_mapping[3] ==1)
	      IntArg().parse(words0[++j], mp);
	  add_map(ia, ip, ma, mp, ea, ep, 1);
	}
    }


  //get the dynamic mapping entries for the mapping table
  if (!BoolArg().parse(conf[s], _dynamic_mapping))
    errh->error("argument %d should be : bool", s);

  if (!_dynamic_mapping)
      return errh->nerrors() ? -1 : 0;

  if (!BoolArg().parse(conf[s+1], _dynamic_portmapping))
      errh->error("argument %d should be : bool", s+1);

  if (!BoolArg().parse(conf[s+2], _dynamic_mapping_allocation_direction))
      errh->error("argument %d should be : bool", s+2);

 //get the rest of the arguments for dynamic mapping


  // get the argument for dynamic address mapping:
  // Mapped_Ip6Address1, Mapped_IP6Address2...
  if (!_dynamic_portmapping)
    {
      IP6Address ipa6;
      for (int i =3; i<(conf.size()-s); i++)
	{
	  if  (cp_ip6_address(conf[s+i], (unsigned char *)&ipa6))
	    add_map(ipa6, 0);
	  else
	    errh->error("argument %d should be : <IP6Address>", i);
	}
      return errh->nerrors() ? -1 : 0;
    }


  // get the argument for dynamic add & port mapping:
  // Mapped_IP6Address port_start# port_end#
  int i = 3;
  Vector<String> words1;
  IP6Address ipa6;
  int  port_start, port_end;
  cp_spacevec(conf[s+i], words1);


  if (cp_ip6_address(words1[0], (unsigned char *)&ipa6) && IntArg().parse(words1[1], port_start) && IntArg().parse(words1[2], port_end))
    {
	  _maddr = ipa6;
	  _mportl = port_start;
	  _mporth = port_end;
    }
  else
	  errh->error("argument %d should be : <IP6Address> <unsigned char> <unsigned char>", i);

  //click_chatter("configuration for AT is successful");
  return errh->nerrors() ? -1 : 0;
}

bool
AddressTranslator::lookup(IP6Address &iai, unsigned short &ipi, IP6Address &mai, unsigned short &mpi, IP6Address &ea, unsigned short &ep, bool lookup_direction)
{

  if (( _number_of_smap >0 ) || (!_dynamic_portmapping))
    {
      //find the mapped entry in the table
      for (int i=0; i<_v.size(); i++)
	{
	  bool match = true;
	  if (_v[i]._static) //this is a static mapping entry
	    {

	      if (_static_mapping[0]  && (lookup_direction ==_dynamic_mapping_allocation_direction))
		match =  match && (_v[i]._iai == iai);
	      if (_static_mapping[1]  && (lookup_direction ==_dynamic_mapping_allocation_direction))
		match =  match && (_v[i]._ipi == ipi);
	      if (_static_mapping[0] && (lookup_direction != _dynamic_mapping_allocation_direction))
		match =  match && (_v[i]._mai == mai);
	      if (_static_mapping[1] && (lookup_direction != _dynamic_mapping_allocation_direction))
		match =  match && (_v[i]._mpi == mpi);

	      if (match)
		{
		  if (_static_mapping[2] && (lookup_direction == _dynamic_mapping_allocation_direction))
		    mai = _v[i]._mai;
		  else if (_static_mapping[2] && (lookup_direction != _dynamic_mapping_allocation_direction))
		    iai = _v[i]._iai;
		  else if (lookup_direction == _dynamic_mapping_allocation_direction)
		    mai = iai;
		  else if (lookup_direction != _dynamic_mapping_allocation_direction)
		    iai = _v[i]._iai;

		  if (_static_mapping[3])
		    mpi = _v[i]._mpi;
		  else if (lookup_direction == _dynamic_mapping_allocation_direction)
		    mpi = ipi;
		  else if (lookup_direction !=_dynamic_mapping_allocation_direction)
		    ipi = mpi;
		  return (true);
		}
	    }

	  else  if (_v[i]._binding) //this is a dynamic bindign entry
	    {
	      if (lookup_direction ==0)
		match =  (_v[i]._iai == iai);  //outward, check if the inner address matches
	      else
		match = (_v[i]._mai == mai); //inward, check if the map address matches
	      if (match)
		{
		  if (lookup_direction==0) //outward packet
		    {
		      mai = _v[i]._mai;
		      mpi = ipi;
		    }
		  else //inward packet
		    {
		      iai = _v[i]._iai;
		      ipi = mpi;
		    }
		   return (true);
		}
	    }
	}

      //no match found
      if (!_dynamic_mapping)
	return false;
      if (_dynamic_mapping_allocation_direction != lookup_direction)
	return false;

      if (!_dynamic_portmapping) // dynamic address mapping only, try to allocate an address
	{

	  for (int i=0; i<_v.size(); i++) {
	    if ((_v[i]._binding == false ) && ((!_v[i]._static)))
	      {
		if (lookup_direction == 0) //outward packet
		  {
		    _v[i]._iai = iai;
		    mai = _v[i]._mai;
		    mpi = ipi;
		  }
		else  //inward packet
		  {
		    _v[i]._mai = mai;
		    iai = _v[i]._iai;
		    ipi = mpi;
		  }

		_v[i]._binding = true;
		//_v[i]._t = time(NULL);
		//_v[i]._t = MyEWMA2::now();

		return (true);
	      }

	  }
	}
      return false;
    }


  //dynamic address and port mapping look up
  IP6FlowID fd;
  // create flowid & lookup in the _out_map or _in_map
  if (lookup_direction ==0)  // outward lookup
    {
      fd = IP6FlowID(iai, ipi, ea, ep);
      Mapping *m = _out_map.find(fd);
      if (m) {
	mai = m->flow_id().saddr();
	mpi = m->flow_id().sport();
	return true;
      }
    }
  else //inward lookup
    {
      fd = IP6FlowID(ea, ep, mai, mpi);
      Mapping *m = _in_map.find(fd);
      if (m) {
	iai = m->flow_id().daddr();
	ipi = m->flow_id().dport();
	return true;
      }
    }


  if (lookup_direction != _dynamic_mapping_allocation_direction )
    return false;

  // if lookup is not successful, create a new mapping if the lookup_direction is the same as the addressAllocationDirection
  // first find the freeport and its corresponding map address, create new flowid
   // then add to in_map and out_map

  unsigned short new_mport = 0;
  new_mport = find_mport(); //
  //click_chatter("***********new port is %x", new_mport);

  if (!new_mport) {
      click_chatter("AddressTranslator ran out of ports");
      return false;
  }

  if (lookup_direction == 0)
    {
      mpi = new_mport;
      mai = _maddr;
    }
  else
    {
      ipi = new_mport;
      iai = _maddr;
    }

  IP6FlowID orig_out_flow = IP6FlowID(iai, ipi, ea, ep);
  IP6FlowID orig_in_flow  = IP6FlowID(ea, ep, mai, mpi);
  IP6FlowID new_out_flow = IP6FlowID(mai, mpi, ea, ep);
  IP6FlowID new_in_flow  = IP6FlowID(ea, ep, iai, ipi);
  Mapping *out_mapping = new Mapping;
  out_mapping->initialize(new_out_flow);
  Mapping *in_mapping = new Mapping;
  in_mapping->initialize(new_in_flow);
  if (lookup_direction == 0)
    {
      if (mpi==_mportl) //the first port mapping
	{
	  _rover = out_mapping;
	  _rover->set_next(_rover);
	  _rover2= in_mapping;
	  _rover2->set_next(_rover2);
	}
      else
	{
	  out_mapping->set_next(_rover->get_next());
	  _rover->set_next(out_mapping);
	  _rover = out_mapping;
	  in_mapping->set_next(_rover2->get_next());
	  _rover2->set_next(in_mapping);
	  _rover2 = in_mapping;
	}
    }
  else
    {
      if (ipi == _mportl)
	{
	  _rover = in_mapping;
	  _rover->set_next(_rover);
	  _rover2 = out_mapping;
	  _rover2->set_next(_rover2);
	}
      else
	{
	  in_mapping->set_next(_rover->get_next());
	  _rover->set_next(in_mapping);
	  _rover = in_mapping;
	  out_mapping->set_next(_rover2->get_next());
	  _rover2->set_next(out_mapping);
	  _rover2 = out_mapping;
	}
    }

  _out_map.insert(orig_out_flow, out_mapping);
  _in_map.insert(orig_in_flow, in_mapping);
  _nmappings++;
  _nmappings2++;
  return true;

}


void
AddressTranslator::push(int port, Packet *p)
{
  if (port == 0)
    handle_outward(p);
  else
    handle_inward(p);
}



void
AddressTranslator::handle_outward(Packet *p)
{
  click_ip6 *ip6 = (click_ip6 *)p->data();
  unsigned char *start = (unsigned char *)p->data();
  IP6Address ip6_src = IP6Address(ip6->ip6_src);
  IP6Address ip6_msrc;
  IP6Address ip6_dst = IP6Address(ip6->ip6_dst);
  uint16_t sport;
  uint16_t dport;
  uint16_t mport;

  WritablePacket *q= Packet::make(p->length());
  if (q==0) {
    click_chatter("can not make packet!");
    assert(0);
  }


  // create the new packet
  // replace src IP6, src port, and tcp checksum fields in the packet
  unsigned char *start_new = (unsigned char *)q->data();
  memset(q->data(), '\0', q->length());
  memcpy(start_new, start, p->length());
  click_ip6 *ip6_new = (click_ip6 * )q->data();

  if (ip6_new->ip6_nxt ==0x3a) //the upper layer is an icmp6 packet
    {

      unsigned char * icmp6_start = (unsigned char *)(ip6_new +1);
      if (lookup(ip6_src, dport, ip6_msrc, mport, ip6_dst, sport, 0))
	{

	  ip6_new->ip6_src = ip6_msrc;
	  switch (icmp6_start[0])  {
	  case 1  : ;
	  case 3  : ;
	  case 128: ;
	  case 129: {
	    click_icmp6_echo * icmp6 = (click_icmp6_echo *) (ip6_new+1);
	    icmp6->icmp6_cksum=0;
	    icmp6->icmp6_cksum= htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	  }
	  break;
	  case 2: {
	    click_icmp6_pkttoobig * icmp6 = (click_icmp6_pkttoobig *)(ip6_new +1);
	    icmp6->icmp6_cksum=0;
	    icmp6->icmp6_cksum= htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	  }
	  break;
	  case 4: {
	    click_icmp6_paramprob * icmp6 = (click_icmp6_paramprob *)(ip6_new +1);
	    icmp6->icmp6_cksum=0;
	    icmp6->icmp6_cksum= htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	  }
	  break;
	  default: ; break;
	  }

	  p->kill();
	  output(0).push(q);
	}
      else
	{
	  //click_chatter(" failed for mapping the ip6 address and port for an icmpv6 packet ");
	  p->kill();
	}
    }


  else if (ip6_new->ip6_nxt ==0x6) //the upper layer is a tcp packet
    {
       click_tcp *tcp_new  = (click_tcp *)(ip6_new+1);
       sport = ntohs(tcp_new->th_sport);
       dport = ntohs(tcp_new->th_dport);

      if (lookup(ip6_src, sport, ip6_msrc, mport, ip6_dst, dport, 0))
	{
	  ip6_new->ip6_src = ip6_msrc;
	  tcp_new->th_sport = htons(mport);
	  //recalculate the checksum for TCP, deal with fragment later
	  tcp_new->th_sum = 0;
	  tcp_new->th_sum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, ip6_new->ip6_nxt, tcp_new->th_sum, (unsigned char *)tcp_new, ip6_new->ip6_plen));
	  p->kill();
	  output(0).push(q);
	}
      else
	{
	  //click_chatter(" failed to map the ip6 address and port for a tcp packet");
	  p->kill();
	}
    }

  else if (ip6_new->ip6_nxt ==0x11) //the upper layer is a udp packet
    {
      click_udp *udp_new  = (click_udp *)(ip6_new+1);
      sport = ntohs(udp_new->uh_sport);
      dport = ntohs(udp_new->uh_dport);

      if (lookup(ip6_src, sport, ip6_msrc, mport, ip6_dst, dport, 0))
	{
	  ip6_new->ip6_src = ip6_msrc;
	  udp_new->uh_sport = htons(mport);
	  //recalculate the checksum for UDP, deal with fragment later
	  udp_new->uh_sum = 0;
	  udp_new->uh_sum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, ip6_new->ip6_nxt, udp_new->uh_sum, (unsigned char *)udp_new, ip6_new->ip6_plen));
	  p->kill();
	  output(0).push(q);
	}
      else
	{
	  //click_chatter(" failed to map the ip6 address and port for a udp packet");
	  p->kill();
	}
    }

  else //discard other packets
    {
      click_chatter(" discard the packet, protocol unrecognized");
      p->kill();
    }

}

void
AddressTranslator::handle_inward(Packet *p)
{
click_ip6 *ip6 = (click_ip6 *)p->data();
  unsigned char *start = (unsigned char *)p->data();
  IP6Address ip6_src = IP6Address(ip6->ip6_src);
  IP6Address ip6_mdst = IP6Address(ip6->ip6_dst);
  IP6Address ip6_dst ;
  uint16_t sport;
  uint16_t mport;
  uint16_t dport;


  WritablePacket *q= Packet::make(p->length());
  if (q==0) {
    click_chatter("can not make packet!");
    assert(0);
  }


  // create the new packet
  // replace src ip6, src port, and tcp checksum  fields in the packet
  unsigned char *start_new = (unsigned char *)q->data();
  memset(q->data(), '\0', q->length());
  memcpy(start_new, start, p->length());
  click_ip6 *ip6_new = (click_ip6 * )q->data();

  if (ip6_new->ip6_nxt ==0x3a) //the upper layer is an icmp6 packet

     {
      unsigned char * icmp6_start = (unsigned char *)(ip6_new +1);
      //unsigned char *ip6_new2 = 0;

      if (lookup(ip6_dst, dport, ip6_mdst, mport, ip6_src, sport, 1))
	{
	   ip6_new->ip6_dst = ip6_dst;
	   switch (icmp6_start[0])  {
	   case 1  : ;  //need to change
	   case 3  : ;  //need to change
	   case 128: ;
	   case 129: {
	     click_icmp6_echo * icmp6 = (click_icmp6_echo *)(ip6_new +1);
	     icmp6->icmp6_cksum = 0;
	     icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	   }
	   break;
	   case 2: {
	     click_icmp6_pkttoobig * icmp6 = (click_icmp6_pkttoobig *)(ip6_new +1);
	     icmp6->icmp6_cksum = 0;
	     icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	   }
	   break;
	   case 4: {
	     click_icmp6_paramprob * icmp6 = (click_icmp6_paramprob *)(ip6_new +1);
	     icmp6->icmp6_cksum = 0;
	     icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	   }
	   break;
	   default: ;
	   }

	  p->kill();
	  output(1).push(q);
	}
      else
	{
	  //click_chatter(" failed for mapping the dst ip6 address and port for an icmpv6 packet -inward");
	  p->kill();
	}
    }


  else if (ip6_new->ip6_nxt ==0x6) //the upper layer is a tcp packet
    {
       click_tcp *tcp_new  = (click_tcp *)(ip6_new+1);
       sport = ntohs(tcp_new->th_sport);
       mport = ntohs(tcp_new->th_dport);
      if (lookup(ip6_dst, dport,ip6_mdst, mport, ip6_src, sport, 1))
	{
	  ip6_new->ip6_dst = ip6_dst;
	  tcp_new->th_dport = htons(dport);
	  //recalculate the checksum for TCP, deal with fragment later
	  tcp_new->th_sum = 0;
	  tcp_new->th_sum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, ip6_new->ip6_nxt, tcp_new->th_sum, (unsigned char *)tcp_new, ip6_new->ip6_plen));


	  p->kill();
	  output(1).push(q);
	}
       else
	{
	  //click_chatter(" failed for mapping the dst ip6 address and port for a tcp packet -inward");
	  p->kill();
	}
    }


  else if (ip6_new->ip6_nxt ==0x11) //the upper layer is a udp packet
    {
       click_udp *udp_new  = (click_udp *)(ip6_new+1);
       sport = ntohs(udp_new->uh_sport);
       mport = ntohs(udp_new->uh_dport);
      if (lookup(ip6_dst, dport,ip6_mdst, mport, ip6_src, sport, 1))
	{
	  ip6_new->ip6_dst = ip6_dst;
	  udp_new->uh_dport = htons(dport);
	  //recalculate the checksum for TCP, deal with fragment later
	  udp_new->uh_sum = 0;
	  udp_new->uh_sum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, ip6_new->ip6_nxt, udp_new->uh_sum, (unsigned char *)udp_new, ip6_new->ip6_plen));

	  p->kill();
	  output(1).push(q);
	}
       else
	{
	  //click_chatter(" failed for mapping the dst ip6 address and port for a udp packet - inward");
	  p->kill();
	}
    }

  else //discard other packets
    {
      click_chatter(" discard the packet, protocol unrecognized");
      p->kill();
    }
}

EXPORT_ELEMENT(AddressTranslator)
CLICK_ENDDECLS

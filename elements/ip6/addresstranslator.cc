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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "addresstranslator.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/click_ip.h>
#include <click/click_ip6.h>
#include <click/click_icmp.h>
#include <click/click_icmp6.h>
#include <click/click_tcp.h>
#include <click/click_udp.h>


AddressTranslator::AddressTranslator()
{
  add_input(); /*IPv6 arriving outward packets */
  add_input(); /*IPv6 arriving inward packets */
  add_output(); /* IPv6 outgoing outward packets with mapped addresses and port*/
  add_output(); /* IPv6 outgoing inward packets with mapped address and port */
}


AddressTranslator::~AddressTranslator()
{ }

//add an entry to the mapping table for dynamic mapping
void
AddressTranslator::add_map(IP6Address &mai, unsigned short mpi,  bool binding)
{ 
  struct EntryMap e;

   if (_dynamic_mapping_allocation_direction ==0)  //outward address allocation
    {
      e._mai = mai;
      e._mpi = mpi;
    }
   else //inward address allocation
     {
       e._iai = mai;
       e._ipi = mpi;
     }
  e._binding = binding;
  e._static = false;
  _v.push_back(e);
}

//add an entry to the mapping table for static mapping
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
AddressTranslator::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  _v.clear();
  int before = errh->nerrors();
  int s = 0;
  IP6Address ia, ma, ea;
  int ip, mp, ep;

 
  //get the static mapping entries for the mapping table
  if (!cp_integer(conf[0], &_number_of_smap))
    {
      errh->error("argument %d should be : integer", 0);
      return (before ==errh->nerrors() ? 0: -1);
    }
  

  if (_number_of_smap==0)
    s = 1;
  else 
    {
      s = _number_of_smap+2;
      if (!cp_bool(conf[1], &_static_portmapping))
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
	    cp_integer(words0[++j], &ip);
	  if (_static_mapping[2] ==1)
	    cp_ip6_address(words0[++j],(unsigned char *)&ma);
	  if (_static_mapping[3] ==1)
	    cp_integer(words0[++j], &mp);
	  add_map(ia, ip, ma, mp, ea, ep, 1);
	}
    }
  
  //get the dynamic mapping entries for the mapping table
  if (!cp_bool(conf[s], &_dynamic_mapping))
    errh->error("argument %d should be : bool", s);
    
  if (!_dynamic_mapping)
    return (before ==errh->nerrors() ? 0: -1);

  _outwardLookupFields[0]=1;
  _outwardLookupFields[1]=0;
  _outwardLookupFields[2]=0;
  _outwardLookupFields[3]=0;
  _outwardLookupFields[4]=1; 
  _outwardLookupFields[5]=0;
   
  _inwardLookupFields[0]=0;
  _inwardLookupFields[1]=0;
  _inwardLookupFields[2]=1;
  _inwardLookupFields[3]=0;
  _inwardLookupFields[4]=1; 
  _inwardLookupFields[5]=0;

  if (!cp_bool(conf[s+1], &_dynamic_portmapping))
      errh->error("argument %d should be : bool", s+1);

  if (_dynamic_portmapping)
    {
      _outwardLookupFields[1] = 1;
      _outwardLookupFields[5] = 1;
      _inwardLookupFields[3] = 1;
      _inwardLookupFields[5] = 1;
    }
    
  if (!cp_bool(conf[s+2], &_dynamic_mapping_allocation_direction))
      errh->error("argument %d should be : bool", s+2);

 //get the rest of the arguments: Mapped_IP6Address port_start# port_end#, ...
  //unsigned char total_entry = 0;
  for (int i=3; i<(conf.size()-s); i++)
    {
      Vector<String> words1;
      IP6Address ipa6;
      int  port_start, port_end;
      cp_spacevec(conf[s+i], words1);
      
      if (cp_ip6_address(words1[0], (unsigned char *)&ipa6) && cp_integer(words1[1], &port_start) && cp_integer(words1[2], &port_end))
	{
	  //total_entry += ((port_end) - (port_start));
	  for (int j=port_start; j<=(port_end); j++)
	    add_map(ipa6, j, 0);
	}     
      else
	  errh->error("argument %d should be : <IP6Address> <unsigned char> <unsigned char>", i);
	
    }
 
  return (before ==errh->nerrors() ? 0: -1);
}

bool
AddressTranslator::lookup(IP6Address &iai, unsigned short &ipi, IP6Address &mai, unsigned short &mpi, IP6Address &ea, unsigned short &ep, bool lookup_direction)
{ 
  bool lookupFields[6];
  if (lookup_direction ==0) {
    for (int i = 0; i<6; i++)
      {
	lookupFields[i] = _outwardLookupFields[i];
      }
  }
  else {
    for (int i = 0; i<6; i++)
      {
	lookupFields[i] = _inwardLookupFields[i];
      }
  }

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

      else if (_v[i]._binding) //this is a dynamic binding entry
	{
	  match = true;
	  match = match && (!lookupFields[0] || (lookupFields[0] && (_v[i]._iai == iai)));
	  match = match && (!lookupFields[1] || (lookupFields[1] && (_v[i]._ipi == ipi)));
	  match = match && (!lookupFields[2] || (lookupFields[2] && (_v[i]._mai == mai)));
	  match = match && (!lookupFields[3] || (lookupFields[3] && (_v[i]._mpi == mpi)));
	  match = match && (!lookupFields[4] || (lookupFields[4] && (_v[i]._ea == ea)));
	  match = match && (!lookupFields[5] || (lookupFields[5] && (_v[i]._ep == ep)));	
	  if (match)
	    {
	      if (lookup_direction==0) //outward packet
		{
		  mai = _v[i]._mai;
		  mpi = _v[i]._mpi;
		}
	      else //inward packet
		{
		  iai = _v[i]._iai;
		  ipi = _v[i]._ipi;
		}
		  
	      _v[i]._t = time(NULL); 
	      return (true);
	    }
	  else
	    {

	    }
	
	}
    }

  // no match found, 
  // if it is the allocation direction, then allocate a free entry for the connection.
  if (_dynamic_mapping_allocation_direction == lookup_direction) { 
    
    //check if there's unbinded entry available or 
    //there's an outdated entry need to be unbinded then bind
    time_t  t = time(NULL) - 10000000; 
    for (int i=0; i<_v.size(); i++) {  
      if ((_v[i]._binding == false ) || ((_v[i]._t < t) && (!_v[i]._static)))
	{
	  
	  if (lookup_direction == 0) //outward packet
	    {
	      _v[i]._iai = iai;
	      _v[i]._ipi = ipi;
	      mai = _v[i]._mai;
	      mpi = _v[i]._mpi;
	    }
	  else  //inward packet
	    {
	      _v[i]._mai = mai;
	      _v[i]._mpi = mpi;
	      iai = _v[i]._iai;
	      ipi = _v[i]._ipi;
	    }

	  _v[i]._ea = ea;
	  _v[i]._ep = ep;
	  
	  _v[i]._binding = true;
	  _v[i]._t = time(NULL); 

	  return (true);
	}
    } 

  }

  return (false);
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
  unsigned short sport;
  unsigned short dport;
  unsigned short mport;
  
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
      //unsigned char * ip6_new2 = 0;
       if (lookup(ip6_src, dport, ip6_msrc, mport, ip6_dst, sport, _dynamic_mapping_allocation_direction))
	{
	 
	  ip6_new->ip6_src = ip6_msrc;
	  switch (icmp6_start[0])  {
	  case 1  : ;
	  case 3  : ;
	  case 128: ;
	  case 129: {
	    icmp6_echo * icmp6 = (icmp6_echo *) (ip6_new+1); 
	    icmp6->icmp6_cksum=0;
	    icmp6->icmp6_cksum= htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	  }
	  break;
	  case 2: {
	    icmp6_pkt_toobig * icmp6 = (icmp6_pkt_toobig *)(ip6_new +1); 
	    icmp6->icmp6_cksum=0;
	    icmp6->icmp6_cksum= htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	  }
	  break;
	  case 4: {
	    icmp6_param * icmp6 = (icmp6_param *)(ip6_new +1); 
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

      if (lookup(ip6_src, sport, ip6_msrc, mport, ip6_dst, dport, _dynamic_mapping_allocation_direction))
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

      if (lookup(ip6_src, sport, ip6_msrc, mport, ip6_dst, dport, _dynamic_mapping_allocation_direction))
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
  unsigned short sport;
  unsigned short mport;
  unsigned short dport;

  
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
	     icmp6_echo * icmp6 = (icmp6_echo *)(ip6_new +1); 
	     icmp6->icmp6_cksum = 0;
	     icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	   }
	   break;
	   case 2: { 
	     icmp6_pkt_toobig * icmp6 = (icmp6_pkt_toobig *)(ip6_new +1); 
	     icmp6->icmp6_cksum = 0;
	     icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	   }
	   break;
	   case 4: {
	     icmp6_param * icmp6 = (icmp6_param *)(ip6_new +1); 
	     icmp6->icmp6_cksum = 0;
	     icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, 0, (unsigned char *)icmp6, ip6_new->ip6_plen));
	   }
	   break;
	   default: ;
	   }

//    	  Packet *q2 = 0;
//    	  q2= q;
  	  p->kill();
//  	  q->kill();
	  // output(1).push(q2);
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

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AddressTranslator)

// generate Vector template instance
#include <click/vector.cc>
template class Vector<AddressTranslator::EntryMap>;

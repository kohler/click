/*
 * addresstranslator{cc,hh} -- element that translates an IPv6 packet to 
 * an IP6 packet with src and dst addresses mapped with ipv4-mapped ipv6
 * addresses. 
 * 
 * Peilei Fan
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

//add an entry to the table for dynamic mapping
void
AddressTranslator::add_map(IP6Address &mai, unsigned short mpi,  bool binding)
{ 
  struct EntryMap e;

   if (_direction ==0)  //outward address allocation
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

//add an entry for the table for static mapping
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
  //int number_of_smap; 
  IP6Address ia, ma, ea;
  int ip, mp, ep;

  for (int i=0; i<6; i++)
    _static_mapping[i] = 0;

 
  //get the static mapping set up
  if (cp_integer(conf[0], &_number_of_smap))
    {
      Vector<String> words3;
      cp_spacevec(conf[1], words3);
      for (int i = 0; i<6; i++)
	{ if (cp_bool(words3[i], (bool*)&_static_mapping[i]))
		    ;
	}
      if (_number_of_smap==0)
	s = 1;
      else 
	s = _number_of_smap+2;
      for (int i = 0; i<_number_of_smap; i++)
	{
	  Vector<String> words;
	  cp_spacevec(conf[2+i], words);
	  int j = 0;
	  if (_static_mapping[0] ==1)
	    cp_ip6_address(words[j],(unsigned char *)&ia);
	  if (_static_mapping[1] ==1)
	    cp_integer(words[++j], &ip);
	  if (_static_mapping[2] ==1)
	    cp_ip6_address(words[++j],(unsigned char *)&ma);
	  if (_static_mapping[3] ==1)
	    cp_integer(words[++j], &mp);
	  if (_static_mapping[4] ==1)
	    cp_ip6_address(words[++j],(unsigned char *)&ea);
	  if (_static_mapping[5] ==1)
	    cp_integer(words[++j], &ep);
	  add_map(ia, ip, ma, mp, ea, ep, 1);
	}
    }
  else
    errh->error("argument %d should be : integer", 0);
  
  //get the dynamic mapping set up
  bool fields[4][6];
  //get first 2 arguments: _outwardLookupFields,  _inwardLookupFields, 
  for (int i=0; i<2; i++)
    { 
      Vector<String> words;
      cp_spacevec(conf[s+i], words);
      
       for (int j=0; j<words.size(); j++)
	 {
	   if (cp_bool(words[j], (bool *)&fields[i][j]))
	   ;
	   else
	     {
	       errh->error("argument %d should be : bool bool bool bool bool bool", i);
	     }
	 }
    }

  for (int i=0; i<6; i++)
    _outwardLookupFields[i] =  fields[0][i];
  for (int i=0; i<6; i++)
    _inwardLookupFields[i] = fields[1][i];
  
  //get the rest of the arguments, direction, IP6Address port# port#, ...
  Vector<String> words;
  cp_spacevec(conf[s+2], words);
  //click_chatter(words[0]);
  if (cp_bool(words[0], &_direction))
    ;
  else
    {
      errh->error("argument %d should be : bool", 5);
    }
 
  unsigned char total_entry = 0;
  for (int i=3; i<(conf.size()-s); i++)
    {
      Vector<String> words;
      IP6Address ipa6;
      int  port_start, port_end;
      cp_spacevec(conf[s+i], words);
      
      if (cp_ip6_address(words[0], (unsigned char *)&ipa6) && cp_integer(words[1], &port_start) && cp_integer(words[2], &port_end))
	{
	  total_entry += ((port_end) - (port_start));
	  for (int j=port_start; j<=(port_end); j++)
	    {
	      add_map(ipa6, j, 0);
	    }
	}     
      else
	{
	  errh->error("argument %d should be : <IP6Address> <unsigned char> <unsigned char>", i);
	}
    }
 
  return (before ==errh->nerrors() ? 0: -1);

}

bool
AddressTranslator::lookup(IP6Address &iai, unsigned short &ipi, IP6Address &mai, unsigned short &mpi, IP6Address &ea, unsigned short &ep, bool lookup_direction)
{ 
  bool lookupFields[6];
  //bool mapFields[6];
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
	  
	  if (_static_mapping[0]  && (lookup_direction ==_direction))
	    match =  match && (_v[i]._iai == iai);
	  if (_static_mapping[1]  && (lookup_direction ==_direction))
	    match =  match && (_v[i]._ipi == ipi);
	  if (_static_mapping[0] && (lookup_direction != _direction))
  	    match =  match && (_v[i]._mai == mai);
  	  if (_static_mapping[1] && (lookup_direction != _direction))
  	    match =  match && (_v[i]._mpi == mpi); 
	  
	  if (match)
	    {
	      if (_static_mapping[2] && (lookup_direction == _direction))
		mai = _v[i]._mai;
	      else if (_static_mapping[2] && (lookup_direction != _direction))
		iai = _v[i]._iai;
	      else if (lookup_direction == _direction)
 		mai = iai;
	      else if (lookup_direction != _direction)
  		iai = _v[i]._iai;

	      if (_static_mapping[3])
		mpi = _v[i]._mpi;
	      else if (lookup_direction == _direction) 
		mpi = ipi;
	      else if (lookup_direction != _direction) 
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
	     
	      iai = _v[i]._iai;
	      ipi = _v[i]._ipi;
	      mai = _v[i]._mai;
	      mpi = _v[i]._mpi;
	      _v[i]._t = time(NULL); 

	      return (true);
	    }
	}
    
    }

  // if it is the allocation direction, then allocate a free entry for the connection.
  if (_direction == lookup_direction) { 
    
    //check if there's unbinded entry available or there's an outdated entry need to be unbinded then bind
    time_t  t = time(NULL) -120;
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
  unsigned short th_sport;
  unsigned short th_mport;
  unsigned short th_dport;

  
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
    {}
    /*
      {
      click_chatter("a icmp6 packet");
      unsigned char * icmp6_start = (unsigned char *)(ip6_new +1);
      click_ip6 * ip6_new2 = 0;
      
      switch (icmp6_start[0])  {
      case 1  : ;
      case 3  : ;
      case 128: ;
      case 129: {
	icmp6_generic * icmp6 = (icmp6_generic *)(ip6_new +1); 
	ip6_new2 = (click_ip6 *)(icmp6+1); }
	break;
      case 2: {
	icmp6_pkt_toobig * icmp6 = (icmp6_pkt_toobig *)(ip6_new +1); 
	ip6_new2 = (click_ip6 *)(icmp6+1); }
	break;
      case 4: {
	icmp6_param * icmp6 = (icmp6_param *)(ip6_new +1); 
	ip6_new2 = (click_ip6 *)(icmp6+1); }
	break;
      default: ; break;
      }
	
 
      //replace with the mapped port in the tcp packet  
      // click_ip6 *ip6_new2 = (click_ip6 *)(icmp6+1);
      unsigned char * tcp2 = (unsigned char *)(ip6_new2+1);
      th_sport = tcp2[0];
      th_dport = tcp2[1];
      

      if (lookup(ip6_src, th_dport, ip6_msrc, th_mport, ip6_dst, th_sport, _direction))
	{
	  ip6_new->ip6_src = ip6_msrc.in6_addr();
	  ip6_new2->ip6_dst = ip6_msrc.in6_addr();
	  tcp2[1]=th_mport;   
	  
	  WritablePacket *q2 = 0;
	  q2= Packet::make(q->length());
	  memset(q2->data(), '\0', q2->length());
	  memcpy(q2->data(), q->data(), q2->length());
	  p->kill();
	  q2= q;
	  //q->kill();
	  output(0).push(q);
	}
      else
	{
	  click_chatter(" failed for mapping the dst ip6 address and port");
	  p->kill();
	}
    }

  */

  else //the upper layer is an tcp/udp packet
    {
       click_tcp *tcp_new  = (click_tcp *)(ip6_new+1);
       th_sport = ntohs(tcp_new->th_sport);
       th_dport = ntohs(tcp_new->th_dport);

      if (lookup(ip6_src, th_sport, ip6_msrc, th_mport, ip6_dst, th_dport, _direction))
	{
	  ip6_new->ip6_src = ip6_msrc;
	  tcp_new->th_sport = htons(th_mport);
	  //recalculate the checksum for TCP, deal with fragment later
	  //tcp_new->th_sum = 0;
	  tcp_new->th_sum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, ip6_new->ip6_nxt, tcp_new->th_sum, (unsigned char *)tcp_new, ip6_new->ip6_plen));
	 
	 
	  WritablePacket *q2 =0;
	  q2 = Packet::make(q->length());
	  //unsigned char x = q->length();
	  memset(q2->data(), '\0', q2->length());
	  memcpy(q2->data(), q->data(), q2->length());
	  q2= q;
	  p->kill();
	  //q->kill(); //why I can't kill q?
	  
	  output(0).push(q2);
	}
       else
	{
	  click_chatter(" failed for mapping the dst ip6 address and port");
	  p->kill();
	}
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
  unsigned short th_sport;
  unsigned short th_mport;
  unsigned short th_dport;

  
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
      icmp6_generic * icmp6;
      //click_ip6 *ip6_new2 = 0;
      unsigned char *ip6_new2 = 0;
      switch (icmp6_start[0])  {
      case 1  : ;
      case 3  : ;
      case 128: ;
      case 129: {
	icmp6 = (icmp6_generic *)(ip6_new +1); 
	//ip6_new2 = (click_ip6 *)(icmp6+1); 
       }
      break;
      case 2: { 
	icmp6_pkt_toobig * icmp6 = (icmp6_pkt_toobig *)(ip6_new +1); 
	//ip6_new2 = (click_ip6 *)(icmp6+1); 
        ip6_new2 = (unsigned char *)(icmp6+1); }
      break;
      case 4: {
	icmp6_param * icmp6 = (icmp6_param *)(ip6_new +1); 
	//ip6_new2 = (click_ip6 *)(icmp6+1); 
	ip6_new2 = (unsigned char *)(icmp6+1); }
      break;
      default: ;
      }
	 
      //replace with the mapped port in the tcp packet  
     
      //unsigned char * tcp2 = (unsigned char *)(ip6_new2+1);
      //th_sport = tcp2[0];
      //th_mport = tcp2[1];

      if (lookup(ip6_dst, th_dport, ip6_mdst, th_mport, ip6_src, th_sport, 1))
	{
	  ip6_new->ip6_dst = ip6_dst;
	  //ip6_new2->ip6_src = ip6_dst;
	  //tcp2[0]=th_dport;   
	 
	  icmp6->icmp6_cksum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, 0x3a, icmp6->icmp6_cksum, (unsigned char *)icmp6, ip6_new->ip6_plen));

	  Packet *q2 = 0;
	  q2= q;
	  p->kill();
	  //q->kill();
	  output(1).push(q2);
	}
      else
	{
	  click_chatter(" failed for mapping the dst ip6 address and port -1");
	  p->kill();
	}
    }
    

  else //the upper layer is an tcp/udp packet
    {
       click_tcp *tcp_new  = (click_tcp *)(ip6_new+1);
       th_sport = tcp_new->th_sport;
       th_mport = tcp_new->th_dport;
      if (lookup(ip6_dst, th_dport,ip6_mdst, th_mport, ip6_src, th_sport, 1))
	{
	  ip6_new->ip6_dst = ip6_dst;
	  tcp_new->th_dport = th_dport;
	  //recalculate the checksum for TCP, deal with fragment later
	  //tcp_new->th_sum = 0;
	  tcp_new->th_sum = htons(in6_fast_cksum(&ip6_new->ip6_src, &ip6_new->ip6_dst, ip6_new->ip6_plen, ip6_new->ip6_nxt, tcp_new->th_sum, (unsigned char *)tcp_new, ip6_new->ip6_plen));
	 
	  WritablePacket *q2 = 0;
	  q2 = Packet::make(q->length());
	  memset(q2->data(), '\0', q2->length());
	  memcpy(q2->data(), q->data(), q2->length());
	  q2= q;
	  p->kill();
	  output(1).push(q2);
	}
       else
	{
	  click_chatter(" failed for mapping the dst ip6 address and port -2 ");
	  p->kill();
	}
    }
 
}

ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(AddressTranslator)

// generate Vector template instance
#include <click/vector.cc>
template class Vector<AddressTranslator::EntryMap>;

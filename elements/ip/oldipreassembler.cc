/*
 * oldipreassembler.{cc,hh} -- defragments IP packets
 * Alexander Yip
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#ifdef HAVE_CONFIG_H
#include <click/config.h>
#endif
#include <click/config.h>
#include <click/package.hh>
#include "oldipreassembler.hh"
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/error.hh>
#include <click/glue.hh>

OldIPReassembler::OldIPReassembler()
  : _expire_timer(expire_hook, (void *) this)
{
  MOD_INC_USE_COUNT;
  add_input();
  add_output();
  _mem_used = 0;
  for(int i=0; i<NMAP; i++)
    _map[i] = 0;

}

OldIPReassembler::~OldIPReassembler()
{
  MOD_DEC_USE_COUNT;
}

OldIPReassembler *
OldIPReassembler::clone() const
{
  return new OldIPReassembler;
}

int
OldIPReassembler::initialize(ErrorHandler *)
{
  _expire_timer.initialize(this);
  _expire_timer.schedule_after_ms(EXPIRE_TIMER_INTERVAL_MS);

  return 0;
}
void
OldIPReassembler::cleanup(CleanupStage)
{
  for (int i = 0; i < NMAP; i++) {
    for (IPQueue *t = _map[i]; t; ) {
      IPQueue *n = t->next;
      if (t)
	queue_free(t, 1);

      t = n;
    }
    _map[i] = 0;
  }
}

Packet *
OldIPReassembler::simple_action(Packet *p_in)
{
  struct IPQueue *qp;
  unsigned int flags, offset;
  int i, ihl, end;
  struct FragEntry *prev, *next, *tmp, *tfp;
  Packet *outPacket = NULL;
  unsigned char const *ptr;

  // clean up memory if necessary
  if (_mem_used > IPFRAG_HIGH_THRESH)
    queue_evictor();
  
  // look for the IPQueue matching this packet
  qp = queue_find(p_in->ip_header());

  // figure out offset & flags
  offset = ntohs(p_in->ip_header()->ip_off );
  flags  = offset & ~IP_OFFMASK;
  offset &= IP_OFFMASK;
  offset <<= 3;
  ihl = (int)p_in->ip_header()->ip_hl << 2;

  // check if we need to create a new IPQueue
  if (qp) {
    // IPQueue already exists
    if (offset == 0) {
      //saw first fragment
      if ((flags & IP_MF) == 0) 
	goto out_freequeue;
      qp->ihlen = ihl;
      qp->iph = p_in->ip_header(); // save header info
    }
  } else {
    // fragmented frame replaced by unfragmented copy?
    if ((offset == 0) && (flags & IP_MF)==0){
      outPacket = p_in;
      goto out_skb; // this packet is not a fragment, just send it along
    }  

    // create a new IPQueue
    qp = queue_create(p_in->ip_header());
    if (!qp)
      goto out_freeskb;
  }

  // reject oversized packets
  if ((ntohs(p_in->ip_header()->ip_len) + (int)offset) > 65535){
    click_chatter(" rejected because oversized");
    goto out_oversize;
  }


  // find right place for packet
  end = offset + ntohs(p_in->ip_header()->ip_len) - ihl;

  // check if last packet, set total packet length
  if ((flags & IP_MF) == 0)
    qp->len = end;

  // Find out which fragments are in front and at the back of us
  // in the chain of fragments so far. We must know where to put
  // this fragment, right?
  prev = NULL;
  for (next = qp->frags; next != NULL; next = next->next) {
    if (next->offset >= offset)
      break; // bingo !
    prev = next;
  }

  // point into the IP datagram 'data' part
  ptr = p_in->data() + ((int)p_in->ip_header()->ip_hl << 2);

  // We found where to put this one. Check for overlap with
  // preceding fragment, and, if needed, align things so that
  // any overlaps are eliminated.
  if ((prev != NULL) && (offset < prev->end)) {
    i = prev->end - offset;
    offset += i;  // ptr into datagram
    ptr += i;     // ptr into fragment data
  }

  // Look for overlap with succeeding segments.
  // If we can merge fragments, do it.
  for (tmp = next; tmp != NULL; tmp = tfp) {
    tfp = tmp->next;
    if (tmp->offset >= end)
      break;   // no overlaps

    // cut tmp (cut existing fragment)
    i = end - next->offset;  // overlap is 'i' bytes
    tmp->len -= i;    // 
    tmp->offset += i;
    tmp->ptr += i;

    // If we get a frag size <= 0, remove it, and the packet 
    // that goes with it.
    if (tmp->len <= 0) {
      if (tmp->prev != NULL) 
	tmp->prev->next = tmp->next;
      else
	qp->frags = tmp->next;

      if (tmp->next != NULL)
	tmp->next->prev = tmp->prev;

      // We have killed the original next frame.
      next = tfp;

      // kill that frag packet & it's FragEntry
      tmp->p->kill();
      delete(tmp);
    }
  }

  // Create a FragEntry to hold this frag if theres enough memory
  tfp = frag_create(offset, end, p_in);
  if (!tfp)
    goto out_freeskb;

  // Insert this fragment in the chain of fragments
  tfp->prev = prev;
  tfp->next = next;
  if (prev != NULL)
    prev->next = tfp;
  else
    qp->frags = tfp;

  if (next != NULL) 
    next->prev = tfp;

  // OK, so we inserted this new fragment into the chain.
  // Check if we now have a full IP datagram which we can
  // bump up to the IP layer...
  if (queue_done(qp)){
    
    // create a new packet containing all the fragments
    outPacket = queue_glue(qp);
    
 out_freequeue:
    // free everything except the first packet if outPacket exists
    // free everything if the outPacket is NULL
    queue_free(qp, outPacket == NULL);
      
 out_skb:
    return outPacket;
  }
  
  
 out_timer:
  qp->last_touched_jiffy = click_jiffies(); // refresh this queue

 out:
  return NULL;
  
 out_oversize:
  

 out_freeskb:
  // the queue is still active... reset its timer
  p_in->kill();
  if (qp)
    goto out_timer;
  
  goto out;

}


void 
OldIPReassembler::queue_free(struct IPQueue *qp, int free_first) {
  
  struct FragEntry *fp;
    
  // remove this entry from the incomplete datagrams queue
  if (qp->next)
    qp->next->pprev = qp->pprev;

  *qp->pprev = qp->next;

  // release all fragment data
  fp = qp->frags;
  while(fp) {
    struct FragEntry *xp = fp->next;

    // free this memory
    _mem_used -= fp->original_size;
    
    // dont free first packet unless specified (first packet 
    //  is usually pushed onto the next click element
    if (free_first || fp != qp->frags){
      fp->p->kill();  
    }

    // free FragEntry
    delete(fp);
    
    fp = xp;
  }    
  
  // finally release the IPQueue
  delete(qp);
  return;
  
}

struct OldIPReassembler::FragEntry *
OldIPReassembler::frag_create(int offset, int end, Packet *p) 
{
  FragEntry *fp = new FragEntry;

  fp->original_size = ntohs(p->ip_header()->ip_len);
  fp->offset = offset;
  fp->end = end;
  fp->len = end-offset;
  fp->p = p;
  fp->ptr = p->data() + ((int)p->ip_header()->ip_hl << 2) ;
  fp->next = fp->prev = NULL;

  // remember how much memory this frag takes up
  _mem_used += fp->original_size;
  
  return fp;
}

struct OldIPReassembler::IPQueue *
OldIPReassembler::queue_create(const click_ip *ipheader){
  int hashvalue;

  IPQueue *qp = new IPQueue;

  qp->iph = ipheader;
  qp->frags = NULL;
  qp->len = 0;
  qp->ihlen = ipheader->ip_hl * 4; 
  qp->next = NULL;
  qp->last_touched_jiffy = click_jiffies(); // touched
  
  // add this entr to the queue
  hashvalue = hashfn(ipheader->ip_id, ipheader->ip_src.s_addr, 
		     ipheader->ip_dst.s_addr, ipheader->ip_p);
  
  if ((qp->next = _map[hashvalue]) != NULL)
    qp->next->pprev = &qp->next;

  _map[hashvalue] = qp;
  qp->pprev = &_map[hashvalue];

  return qp;
}
  


struct OldIPReassembler::IPQueue * 
OldIPReassembler::queue_find(const struct click_ip *iph) 
{
  unsigned int id = iph->ip_id;
  unsigned long  saddr = iph->ip_src.s_addr;
  unsigned long  daddr = iph->ip_dst.s_addr;
  unsigned short protocol = iph->ip_p;
  IPQueue *qp ;

  unsigned int hashvalue;

  hashvalue = hashfn(id, saddr, daddr, protocol);

  for (qp = _map[hashvalue]; qp; qp = qp->next) {
  
    if ((qp->iph->ip_id == id) &&
	(qp->iph->ip_src.s_addr == saddr) &&
 	(qp->iph->ip_dst.s_addr == daddr) &&
	(qp->iph->ip_p == protocol)) {
      
      break;
    }
  }
  return qp;
}



/**
 * returns 0 if the queue does not contain all fragments.
 * returns non-zero if the queue contains a complete packet.
 */
int
OldIPReassembler::queue_done(IPQueue *qp) 
{
  
  FragEntry *fp;
  int offset;
  
  // only possible if we received the final fragment
  if (qp->len == 0)
    return 0;

  // check all fragment offsets to see if they connect
  fp = qp->frags;
  offset = 0;

  //  if (!fp)

  //no frags in queue
  while (fp) {
    if (fp->offset > offset)
      return(0);  // fragment missing
    offset = fp->end;
    fp = fp->next;
  }
  
  return 1; // all fragments are present
}


/**
 * returns a packet composed of all the fragments in <q>
 */
Packet *
OldIPReassembler::queue_glue(IPQueue *qp)
{
  int len, count;
  FragEntry *first, *next;
  WritablePacket *wp;

  next = qp->frags;

  
#if 0
  while(next){
    
    pos = 0;
    for (unsigned i = 0; i < 32 && i < next->p->length(); i++) {
      sprintf(_buf + pos, "%02x", next->p->data()[i] & 0xff);
      pos += 2;
      if ((i % 4) == 3) _buf[pos++] = ' ';
    }
    _buf[pos++] = '\0';
    click_chatter(" glue: (%d) %s", next->p->length(),  _buf);
    

    next = next->next;
  }
#endif

  // reject if oversized
  len = qp->len + qp->ihlen;
  if (len > 65535)
    goto out_oversize;

  // use header in first packet
  first = qp->frags;

  // resize first packet, and copy info from other fragments
  first->p = first->p->put(qp->len - first->len);
  wp = (WritablePacket *)first->p;

  count = qp->ihlen + first->len;
  next = first->next;

  while(next) {

    if ((next->len <= 0) || (count + next->len) > len) {
      //click_chatter(" invalid packet ");
      goto out_invalid;
    }
    
    // copy from frag into final packet.
    memcpy(wp->data() + next->offset + qp->ihlen, 
	   next->ptr, next->len);
    count += next->len;
    next = next->next;
  }

  // fix up header
  wp->ip_header()->ip_hl = qp->ihlen >> 2;
  wp->ip_header()->ip_len = htons(count);
  wp->ip_header()->ip_off = 0;
  wp->ip_header()->ip_sum = 0;
  wp->ip_header()->ip_sum = click_in_cksum((unsigned char*)(wp->data()), qp->ihlen);
  wp->set_ip_header(wp->ip_header(), count);

  return wp;
 out_invalid:
 out_oversize:
  return NULL;
}

void
OldIPReassembler::queue_evictor()
{
  int i, progress;

 restart:
  progress = 0;
  
  for(i=0; i< NMAP; i++) {
    struct IPQueue *qp;

    // if we've cleared enough memory, exit
    if (_mem_used <= IPFRAG_LOW_THRESH)
      return;

    qp = _map[i];
    if (qp) {
      while (qp->next) // find the last IPQueue
	qp = qp->next;

      queue_free(qp, 1); // free that one

      progress = 1;
    }
  }
  if (progress)
    goto restart;

  click_chatter(" panic! queue_evictor: memcount");

}


void
OldIPReassembler::expire_hook(Timer *, void *thunk)
{
  // look at all queues. If no activity for 30 seconds, kill that queue

  OldIPReassembler *ipr = (OldIPReassembler *) thunk;
  int jiff = click_jiffies();
  int gap;

  for (int i=0; i<NMAP; i++) {
    IPQueue *prev = 0;

    while(1) {
      IPQueue *qp = (prev ? prev->next : ipr->_map[i]);
      if (!qp)
	break;
      
      gap = jiff - qp->last_touched_jiffy;
      
      // if idle for 30 seconds get rid of it.
      if (gap > EXPIRE_TIMEOUT*CLICK_HZ){
	ipr->queue_free(qp, 1); 
      }
      prev = qp;
    }    
  }
  ipr->_expire_timer.schedule_after_ms(EXPIRE_TIMER_INTERVAL_MS);
}

int 
OldIPReassembler::hashfn(unsigned short id, unsigned src, unsigned dst, unsigned char prot) 
{ 
  return ((((id) >> 1) ^ (src) ^ (dst) ^ (prot)) & (NMAP - 1));
}

ELEMENT_REQUIRES(false)
EXPORT_ELEMENT(OldIPReassembler)

#ifndef TCPBUFFER_HH
#define TCPBUFFER_HH
#include <click/element.hh>
#include <click/click_tcp.h>

/*
 * =c
 * TCPBuffer([SKIP])
 * =s TCP
 * buffer TCP packets
 * =d
 * provides in order TCP buffer. expects TCP packets on input.
 *
 * packets arriving at the input push port are inserted into a linked list,
 * sorted increasingly on the tcp sequence number. packets with a sequence
 * number already on the list will be dropped.
 *
 * packets are pulled out of TCPBuffer. pull will return packets in order. if
 * SKIP is false, and there is a packet missing in the middle of a sequence,
 * TCPBuffer will return 0 until that packet arrives. SKIP is false by
 * default. setting SKIP to true allow puller to skip missing packets, but
 * still get packets in order.
 *
 * if a packet arrives at TCPBuffer, but it's sequence number is smaller than
 * that of the first packet on the linked list, the packet is deleted. in this
 * case, TCPBuffer assumes the packet is either a retransmit (if SKIP is
 * false) or the puller is no longer interested in it (if SKIP is true).
 *
 * the first packet arrives at TCPBuffer gets to set the initial sequence
 * number. it is expected that this packet will be either a SYN or a SYN ACK
 * packet.
 *
 * TODO
 *   deal with sequence number wrap arounds
 */

class TCPBuffer : public Element {
 private:

  static const int _capacity = 128;

  class TCPBufferElt {
   private:
    Packet *_packet;
    TCPBufferElt **_chain_ptr;
    TCPBufferElt *_next;
    TCPBufferElt *_prev;

   public:
    TCPBufferElt(TCPBufferElt **chain, Packet *p);

    TCPBufferElt *next() const		{ return _next; }
    TCPBufferElt *prev() const		{ return _prev; }
    Packet* packet() const		{ return _packet; }
    
    Packet* kill_elt();
  };

  TCPBufferElt *_chain;
  unsigned _initial_seq;
  unsigned _first_seq;
  
  bool _skip;

 public:
  
  TCPBuffer();
  ~TCPBuffer();
  
  const char *class_name() const		{ return "TCPBuffer"; }
  const char *processing() const		{ return PUSH_TO_PULL; }
  
  TCPBuffer *clone() const			{ return new TCPBuffer; }
  int initialize(ErrorHandler *);
  void uninitialize();
  int configure(const Vector<String> &conf, ErrorHandler *errh);
  
  void push(int, Packet *);
  Packet *pull(int);
};

inline
TCPBuffer::TCPBufferElt::TCPBufferElt(TCPBufferElt **chain_ptr, Packet *p)
{
  const click_tcp *tcph = reinterpret_cast<const click_tcp *>(p->transport_header());
  unsigned int seqn = ntohl(tcph->th_seq);
    
  _chain_ptr = chain_ptr;
  _packet = p;

  if (*chain_ptr == 0) {
    *chain_ptr = this;
    _next = 0;
    _prev = 0;
  }
  else {
    TCPBufferElt *list = *chain_ptr;
    TCPBufferElt *lprev = 0L;
    do {
      Packet *pp = list->packet();
      const click_tcp *tcph_pp = reinterpret_cast<const click_tcp *>(pp->transport_header());
      if (seqn < ntohl(tcph_pp->th_seq)) {
	/* insert here */
	_next = list;
	_prev = list->_prev;
	_next->_prev = this;
	if (_prev) 
	  _prev->_next = this;
	if (list == *chain_ptr)
          *chain_ptr = this;
	return;
      }
      else if (seqn == ntohl(tcph_pp->th_seq)) {
        p->kill();
	delete this;
	return;
      }
      lprev = list;
      list = list->_next;
    } while(list);
    if (!list) {
      /* add to end of list */
      _next = 0;
      _prev = lprev;
      lprev->_next = this;
    }
  }
}

inline Packet *
TCPBuffer::TCPBufferElt::kill_elt()
{
  Packet *p = _packet;
  if (_chain_ptr && *_chain_ptr == this) {
    /* head of chain */
    if (_next) 
      _next->_prev = 0;
    *_chain_ptr = _next;
  }
  else if (_prev || _next) {
    if (_prev) 
      _prev->_next = _next;
    if (_next)
      _next->_prev = _prev;
  }
  _prev = 0;
  _next = 0;
  _packet = 0;
  delete this;
  return p;
}

#endif


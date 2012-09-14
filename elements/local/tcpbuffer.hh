#ifndef CLICK_TCPBUFFER_HH
#define CLICK_TCPBUFFER_HH
#include <click/element.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

/*
 * =c
 * TCPBuffer([SKIP])
 * =s tcp
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
 *   prevent packets with bad seq number range from corrupting queue;
 *   should reject packets with overlaping seq number range
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
  bool _start_push;
  bool _start_pull;

  bool _skip;
  void dump();

public:

  TCPBuffer() CLICK_COLD;
  ~TCPBuffer() CLICK_COLD;

  const char *class_name() const		{ return "TCPBuffer"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH_TO_PULL; }

  int initialize(ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;
  int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;

  void push(int, Packet *);
  Packet *pull(int);

  /* if there is a missing sequence, set seqno to
   * that sequence number. returns false if no packets
   * have arrived at the buffer. true otherwise. */
  bool first_missing_seq_no(unsigned& seqno);

  /* if there is a missing sequence after pos, set seqno
   * to that sequence number. returns false if no packets
   * have arrived at the buffer. true otherwise. */
  bool next_missing_seq_no(unsigned pos, unsigned &seqno);

  static unsigned seqlen(Packet *);
  static unsigned seqno(Packet *);
};

inline
TCPBuffer::TCPBufferElt::TCPBufferElt(TCPBufferElt **chain_ptr, Packet *p)
{
  unsigned int seqn = seqno(p);
  _chain_ptr = chain_ptr;
  _packet = p;

  if (*chain_ptr == 0) {
    *chain_ptr = this;
    _next = 0;
    _prev = 0;
    return;
  }
  else {
    TCPBufferElt *list = *chain_ptr;
    TCPBufferElt *lprev = 0L;
    do {
      Packet *pp = list->packet();
      if (SEQ_LT(seqn,seqno(pp))) {
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
      else if (seqn == seqno(pp)) {
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
      return;
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

inline bool
TCPBuffer::first_missing_seq_no(unsigned& sn)
{
  if (!_chain && !_start_pull)
    return false;
  unsigned expect =
    _start_pull ? _first_seq : seqno(_chain->packet());
  return next_missing_seq_no(expect, sn);
}

inline bool
TCPBuffer::next_missing_seq_no(unsigned pos, unsigned& sn)
{
  TCPBufferElt *elt = _chain;
  unsigned expect = _first_seq;
  if (elt) {
    Packet *p = elt->packet();
    expect = _start_pull ? _first_seq : seqno(p);
    while(elt) {
      Packet *p = elt->packet();
      if (seqno(p) != expect) {
	if (SEQ_GEQ(expect,pos)) {
	  sn = expect;
	  return true;
	}
	else if (SEQ_GT(seqno(p), pos)) {
	  sn = pos;
	  return true;
	}
      }
      expect = seqno(p) + seqlen(p);
      elt = elt->next();
    }
  }
  if (_start_pull || _chain) {
    if (SEQ_GEQ(expect,pos)) {
      sn = expect;
      return true;
    }
    else {
      sn = pos;
      return true;
    }
  }
  return false;
}

inline unsigned
TCPBuffer::seqlen(Packet *p)
{
  const click_ip *iph = p->ip_header();
  const click_tcp *tcph =
    reinterpret_cast<const click_tcp *>(p->transport_header());
  unsigned seqlen = (ntohs(iph->ip_len)-(iph->ip_hl<<2)-(tcph->th_off<<2));
  if ((tcph->th_flags&TH_SYN) || (tcph->th_flags&TH_FIN)) seqlen++;
  return seqlen;
}

inline unsigned
TCPBuffer::seqno(Packet *p)
{
  const click_tcp *tcph =
    reinterpret_cast<const click_tcp *>(p->transport_header());
  return ntohl(tcph->th_seq);
}

CLICK_ENDDECLS
#endif

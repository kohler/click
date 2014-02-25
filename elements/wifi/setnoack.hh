#ifndef CLICK_SETNOACK_HH
#define CLICK_SETNOACK_HH
#include <click/element.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
=c
SetNoAck()

=s Wifi

Sets the No-Ack bit in tx-flags

=d

Sets the WIFI_EXTRA_TX_NOACK in wifi_extra_anno. This will make 
RadiotapEncap() set the TX flag IEEE80211_RADIOTAP_F_TX_NOACK, 
and that again will make mac80211 based drivers set the 
IEEE80211_TX_CTL_NO_ACK flag in the annotation of the outgoing packet. 
Rate control is bypassed, and the packet is sent only once with 
the rate specified by SetTXRate(). If the receiver is a monitor, there 
will be no ACK.

*/

class SetNoAck : public Element { public:

  SetNoAck() CLICK_COLD;
  ~SetNoAck() CLICK_COLD;

  const char *class_name() const		{ return "SetNoAck"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }

  Packet *simple_action(Packet *);

private:
};

CLICK_ENDDECLS
#endif

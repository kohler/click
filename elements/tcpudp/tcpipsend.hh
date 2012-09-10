#ifndef CLICK_TCPIPSEND_HH
#define CLICK_TCPIPSEND_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/tcp.h>
CLICK_DECLS

/*
 * =c
 * TCPIPSend()
 * =s tcp
 * generates TCP/IP packets when requested
 * =d
 *
 * Sends TCP/IP packets when asked to do so. No inputs. One output.
 *
 * =e
 *
 * =h send write-only
 *
 * Expects a string "saddr sport daddr dport seqno ackno bits [count stop]".
 * "saddr sport daddr dport seqno ackno" have their obvious meanings.  "bits"
 * is the value of the 6 TCP flags.  The optional "count" argument is the
 * number of packets to send.  The optional "stop" argument is a Boolean; if
 * true, stops the router after sending the packets.
 */

class TCPIPSend : public Element {
public:
  TCPIPSend() CLICK_COLD;
  ~TCPIPSend() CLICK_COLD;

  const char *class_name() const	{ return "TCPIPSend"; }
  const char *port_count() const	{ return PORTS_0_1; }
  const char *processing() const	{ return PUSH; }

private:
  void add_handlers() CLICK_COLD;
  static int send_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  Packet * make_packet(unsigned int, unsigned int, unsigned short,
                       unsigned short, unsigned, unsigned, unsigned char);
};

CLICK_ENDDECLS
#endif

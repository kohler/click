#ifndef TCPIPSEND_HH
#define TCPIPSEND_HH

/*
 * =c
 * TCPIPSend()
 * =d
 *
 * Sends TCP/IP packets when asked to do so. No inputs. One output.
 *
 * =e
 *
 * =h send (write)
 * Expects a string "saddr sport daddr dport seqn ackn bits" with their
 * obvious meaning. Bits is the value of the 6 TCP flags.
 *
 * =a
 */

#include "element.hh"
#include "glue.hh"
#include "click_tcp.h"

class TCPIPSend : public Element {
public:
  TCPIPSend();
  ~TCPIPSend();
  
  const char *class_name() const	{ return "TCPIPSend"; }
  const char *processing() const	{ return PUSH; }
  
  TCPIPSend *clone() const;

private:
  void add_handlers();
  static int send_write_handler
    (const String &conf, Element *e, void *, ErrorHandler *errh);
  Packet * make_packet(unsigned int, unsigned int, unsigned short,
                       unsigned short, unsigned, unsigned, unsigned char);
};



#endif // TCPIPSEND_HH

#ifndef CLICK_RFC2507D_HH
#define CLICK_RFC2507D_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
CLICK_DECLS

/* =c
 * RFC2507Decomp
 *
 * =s tcp
 * RFC2507 IPv4/TCP header decompressor.
 *
 * =d
 * Input packets should be as produced by RFC2507c.
 * Spits out IP packets.
 *
 * =a
 * RFC2507Comp
 */

class RFC2507d : public Element {
public:

  /* used to store context as well as hash key */
  struct tcpip {
    click_ip _ip;
    click_tcp _tcp;
    operator bool() const { return(_ip.ip_src.s_addr != 0); }
    tcpip() { _ip.ip_src.s_addr = 0; }
  };

  RFC2507d();
  ~RFC2507d();

  const char *class_name() const		{ return "RFC2507Decomp"; }
  const char *port_count() const		{ return PORTS_1_1; }

  Packet *simple_action(Packet *);

private:
  /* constants specified in the RFC */
  enum { TCP_SPACE = 15 }; /* max CID value for TCP. 3..255 are legal. */

  /* first byte of packet indicates type */
  enum { PT_OTHER = 0, /* ordinary packet (not compressed, no CID) */
	 PT_FULL_HEADER = 1, /* one byte CID, then full ip/tcp */
	 PT_COMPRESSED_TCP = 2 }; /* CID, compressed packet */

  /* per-connection control block, indexed by CID */
  struct ccb {
    struct tcpip _context;
  };
  struct ccb _ccbs[TCP_SPACE];

  void decode(const u_char * &in, unsigned short &);
  void decode(const u_char * &in, unsigned int &);
};

CLICK_ENDDECLS
#endif

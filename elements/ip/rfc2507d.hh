#ifndef RFC2507D_HH
#define RFC2507D_HH
#include "element.hh"

/*
 * RFC2507 IPv4/TCP header de-compressor.
 * Input packets should be as produced by RFC2507c.
 * Spits out IP packets.
 */

#include "hashmap.hh"
#include "glue.hh"
#include "click_ip.h"
#include "click_tcp.h"

class RFC2507d : public Element {
public:
  RFC2507d();
  ~RFC2507d();

  const char *class_name() const		{ return "RFC2507Decomp"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  RFC2507d *clone() const;

  Packet *simple_action(Packet *);

private:
  /* constants specified in the RFC */
  static const int TCP_SPACE = 15; /* max CID value for TCP. 3..255 are legal. */

  /* first byte of packet indicates type */
  static const int PT_OTHER = 0; /* ordinary packet (not compressed, no CID) */
  static const int PT_FULL_HEADER = 1; /* one byte CID, then full ip/tcp */
  static const int PT_COMPRESSED_TCP = 2; /* CID, compressed packet */

  /* used to store context as well as hash key */
  struct tcpip {
    click_ip _ip;
    struct tcp_header _tcp;
    operator bool() const { return(_ip.ip_src.s_addr != 0); }
    tcpip() { _ip.ip_src.s_addr = 0; }
    int hashcode();
  };

  /* per-connection control block, indexed by CID */
  struct ccb {
    struct tcpip _context;
  };
  struct ccb _ccbs[TCP_SPACE];

  void decode(u_char * &in, unsigned short &);
  void decode(u_char * &in, unsigned int &);
};

inline bool
operator==(struct RFC2507d::tcpip &a, struct RFC2507d::tcpip &b)
{
  return(memcmp(&a._ip, &b._ip, sizeof(a._ip)) == 0 &&
         memcmp(&a._tcp, &b._tcp, sizeof(a._tcp)) == 0);
}

#endif


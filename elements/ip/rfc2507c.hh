#ifndef RFC2507C_HH
#define RFC2507C_HH
#include "element.hh"

/*
 * RFC2507 IPv4/TCP header compressor.
 * Input packets need to be IPv4 (no ether header &c).
 * It's OK if they're not TCP.
 * 
 * How to specify various kinds of output? There is not really
 * a useful general standard. Perhaps emit each different format
 * of output on a different output() and let further modules
 * sort it out. Then we wouldn't be AGNOSTIC...
 */

#include "hashmap.hh"
#include "glue.hh"
#include "click_ip.h"
#include "click_tcp.h"

class RFC2507c : public Element {
public:
  RFC2507c();
  ~RFC2507c();

  const char *class_name() const		{ return "RFC2507Comp"; }
  Processing default_processing() const	{ return AGNOSTIC; }
  RFC2507c *clone() const;

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

  /* map packet header ID fields to CID */
  HashMap<struct tcpip, int> _map;

  void make_key(const struct tcpip &from, struct tcpip &to);
  Packet *make_other(Packet *p);
  Packet *make_full(int cid, Packet *p);
  int encode16(int o, int n, char *p, int &i);
  int encode32(int o, int n, char *p, int &i);
  int encodeX(int o, int n, char *p, int &i);
  Packet *make_compressed(int cid, Packet *p);
};

inline bool
operator==(struct RFC2507c::tcpip &a, struct RFC2507c::tcpip &b)
{
  return(memcmp(&a._ip, &b._ip, sizeof(a._ip)) == 0 &&
         memcmp(&a._tcp, &b._tcp, sizeof(a._tcp)) == 0);
}

#endif


#ifndef RFC2507C_HH
#define RFC2507C_HH
#include <click/element.hh>

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

#include <click/hashmap.hh>
#include <click/glue.hh>
#include <click/ipflowid.hh>
#include <click/click_ip.h>
#include <click/click_tcp.h>

class RFC2507c : public Element {
public:

  /* used to store context as well as hash key */
  struct tcpip {
    click_ip _ip;
    click_tcp _tcp;
    operator bool() const { return(_ip.ip_src.s_addr != 0); }
    tcpip() { _ip.ip_src.s_addr = 0; }
    operator IPFlowID() const;
  };
  
  RFC2507c();
  ~RFC2507c();

  const char *class_name() const		{ return "RFC2507Comp"; }
  const char *processing() const		{ return AGNOSTIC; }
  RFC2507c *clone() const;

  Packet *simple_action(Packet *);

private:
  /* constants specified in the RFC */
  static const int TCP_SPACE = 15; /* max CID value for TCP. 3..255 are legal. */

  /* first byte of packet indicates type */
  static const int PT_OTHER = 0; /* ordinary packet (not compressed, no CID) */
  static const int PT_FULL_HEADER = 1; /* one byte CID, then full ip/tcp */
  static const int PT_COMPRESSED_TCP = 2; /* CID, compressed packet */
  
  /* per-connection control block, indexed by CID */
  struct ccb {
    struct tcpip _context;
  };
  struct ccb _ccbs[TCP_SPACE];

  /* map packet header ID fields to CID */
  HashMap<IPFlowID, int> _map;

  void make_key(const struct tcpip &from, struct tcpip &to);
  WritablePacket *make_other(Packet *p);
  WritablePacket *make_full(int cid, Packet *p);
  int encode16(int o, int n, char *p, int &i);
  int encode32(int o, int n, char *p, int &i);
  int encodeX(int o, int n, char *p, int &i);
  Packet *make_compressed(int cid, Packet *p);
};

#endif

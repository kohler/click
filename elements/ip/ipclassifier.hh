#ifndef IPCLASSIFIER_HH
#define IPCLASSIFIER_HH
#include "elements/standard/classifier.hh"

/*
 * =c
 * IPClassifier(PATTERN_1, ..., PATTERN_N)
 * =d
 *
 * Classifies IP packets according to tcpdump(1)-like patterns. The
 * IPClassifier has N outputs, each associated with the corresponding pattern
 * from the configuration string. The input packets must have their IP header
 * annotation set; CheckIPHeader and MarkIPHeader do this.
 *
 * Patterns are built from <i>pattern primitives</i>. The primitives
 * IPClassifier understands are:
 *
 * <b>ip proto PROTO</b>, where PROTO is an IP protocol name (see below) or a
 * valid IP protocol number. Matches packets of the given protocol.
 *
 * <b>[SRCORDST] host IPADDR</b>, where IPADDR is an IP address and SRCORDST
 * is `src', `dst', `src or dst', or `src and dst'. (If SRCORDST is missing,
 * `src or dst' is assumed.) Matches packets sent to and/or from the given
 * machine.
 *
 * <b>[SRCORDST] net NETADDR</b>, where NETADDR is an IP network address
 * (either CIDR-style `IPADDR/BITS' or `IPADDR mask MASK') and SRCORDST is as
 * above. Matches packets sent to and/or from the given network.
 *
 * <b>[SRCORDST] [tcp | udp] port PORT</b>, where PORT is a TCP or UDP port
 * name (see below) or number and SRCORDST is as above. Matches packets sent
 * to and/or from the given TCP or UDP port. If you leave out `tcp' or `udp',
 * then either TCP or UDP is accepted.
 *
 * <b>tcp opt TCPOPT</b>, where TCPOPT is a TCP option name (see below).
 * Matches TCP packets with the given option.
 *
 * These primitives can be combined with the connectives `and', `or', and
 * `not' (synonyms `&&', `||', and `!'), and with parentheses. For example,
 * `(dst port www or dst port ssh) and tcp opt syn'.
 *
 * You can omit a lot of this syntax. For example, instead of `ip proto tcp',
 * you can just say `tcp'; and similarly for `port www' (just say `www'), `tcp
 * opt syn' (just say `syn'), or `net 10.0.0.0/24' (just say `10.0.0.0/24').
 * You can often eliminate repetitive qualifiers, too: `src port 80 or 81' is
 * the same as `src port 80 or src port 81'.
 *
 * As a special case, a pattern consisting of "-" matches every packet.
 *
 * The patterns are scanned in order, and the packet is sent to the output
 * corresponding to the first matching pattern. Thus more specific patterns
 * should come before less specific ones. You will get a warning if no packet
 * will ever match a pattern. Usually, this is because an earlier pattern is
 * more general, or because your pattern is contradictory (`src port www and
 * src port ftp').
 *
 * =n
 *
 * Valid IP port names: `echo', `discard', `daytime', `chargen', `ftp-data',
 * `ftp', `ssh', `telnet', `smtp', `finger', `www'
 *
 * Valid IP protocol names: `icmp', `igmp', `ipip', `tcp', `udp'
 *
 * Valid TCP options: `syn', `fin', `ack', `rst', `psh', `urg'
 *
 * This element correctly handles IP packets with options.
 *
 * =e
 * For example,
 *
 * = IPClassifier(10.0.0.0/24 and syn,
 * =              10.0.0.0/24 and fin ack,
 * =              10.0.0.0/24 and tcp,
 * =              -);
 *
 * creates an element with four outputs. The first three outputs are for TCP
 * packets from net 10.0.0.x. SYN packets are sent to output 0, FIN packets
 * with the ACK bit set to output 1, and all other TCP packets to output 2.
 * The last output is for all other IP packets, and non-TCP packets from net
 * 10.0.0.x.
 *
 * =h program read-only
 * Returns a human-readable definition of the program the IPClassifier element
 * is using to classify packets. At each step in the program, four bytes
 * of packet data are ANDed with a mask and compared against four bytes of
 * classifier pattern.
 *
 * =a Classifier
 * =a CheckIPHeader MarkIPHeader CheckIPHeader2
 * =a tcpdump(1) */

class IPClassifier : public Classifier {

  enum {
    UNKNOWN = -1000,
    NONE = 0,
    
    TYPE_HOST = 1, TYPE_NET = 2, TYPE_PORT = 3, TYPE_PROTO = 4,
    TYPE_TCPOPT = 5,
    
    SD_SRC = 1, SD_DST = 2, SD_AND = 3, SD_OR = 4,
    
    PROTO_IP = 1,
    
    DATA_NONE = 0, DATA_IP = 1, DATA_IPMASK = 2, DATA_PROTO = 3,
    DATA_PORT = 4, DATA_INT = 5, DATA_TCPOPT = 6,
    
    IP_PROTO_TCP_OR_UDP = 0x10000,

    TRANSP_FAKE_OFFSET = 64,
  };

  struct Primitive {
    
    int _type;
    int _srcdst;
    int _net_proto;
    int _transp_proto;
    
    int _data;
    union {
      struct in_addr ip;
      struct {
	struct in_addr ip, mask;
      } ipnet;
      int i;
    } _u;
    
    Primitive()	{ clear(); }
    
    void clear();
    void set_type(int, int slot, ErrorHandler *);
    void set_srcdst(int, int slot, ErrorHandler *);
    void set_net_proto(int, int slot, ErrorHandler *);
    void set_transp_proto(int, int slot, ErrorHandler *);
    
    int check(int slot, const Primitive &, ErrorHandler *);
    void add_exprs(Classifier *, Vector<int> &, bool) const;
    
  };
  
 public:
  
  IPClassifier();
  ~IPClassifier();
  
  const char *class_name() const		{ return "IPClassifier"; }
  const char *processing() const		{ return PUSH; }
  
  IPClassifier *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int port, Packet *);
  
};

#endif

#ifndef IPFILTER_HH
#define IPFILTER_HH
#include "elements/standard/classifier.hh"

/*
 * =c
 * IPFilter(ACTION_1 PATTERN_1, ..., ACTION_N PATTERN_N)
 * =s
 * filters IP packets by contents
 * V<classification>
 * =d
 *
 * Filters IP packets. IPFilter can have an arbitrary number of filters, which
 * are ACTION-PATTERN pairs. The ACTIONs describe what to do with packets,
 * while the PATTERNs are tcpdump(1)-like patterns; see IPClassifier(n) for a
 * description of their syntax. Packets are tested against the filters in
 * order, and are processed according to the ACTION in the first filter that
 * matched.
 *
 * Each ACTION is either a port number, which specifies that the packet should
 * be sent out on that port; `C<allow>', which is equivalent to `C<0>';
 * `C<drop>', which means drop the packet; or `C<deny>', which is equivalent
 * to `C<1>' if the element has at least two outputs and `C<drop>' if it does
 * not.
 *
 * The IPFilter element has an arbitrary number of outputs. Input packets must
 * have their IP header annotation set; CheckIPHeader and MarkIPHeader do
 * this.
 *
 * =n
 * Every IPFilter element has an equivalent corresponding IPClassifier element
 * and vice versa. Use the element whose syntax is more convenient for your
 * needs.
 *
 * =e
 *
 * This large IPFilter implements the incoming packet filtering rules for the
 * "Interior router" described on pp691-692 of I<Building Internet Firewalls,
 * Second Edition> (Elizabeth D. Zwicky, Simon Cooper, and D. Brent Chapman,
 * O'Reilly and Associates, 2000). The captialized words (C<INTERNALNET>,
 * C<BASTION>, etc.) are addresses that have been registered with
 * AddressInfo(n). The rule FTP-7 has a port range that cannot be implemented
 * with IPFilter.
 *
 *   IPFilter(// Spoof-1:
 *            deny src INTERNALNET,
 *            // HTTP-2:
 *            allow src BASTION && dst INTERNALNET
 *               && tcp && src port www && dst port > 1023 && ack,
 *            // Telnet-2:
 *            allow dst INTERNALNET
 *               && tcp && src port 23 && dst port > 1023 && ack,
 *            // SSH-2:
 *            allow dst INTERNALNET && tcp && src port 22 && ack,
 *            // SSH-3:
 *            allow dst INTERNALNET && tcp && dst port 22,
 *            // FTP-2:
 *            allow dst INTERNALNET
 *               && tcp && src port 21 && dst port > 1023 && ack,
 *            // FTP-4:
 *            allow dst INTERNALNET
 *               && tcp && src port > 1023 && dst port > 1023 && ack,
 *            // FTP-6:
 *            allow src BASTION && dst INTERNALNET
 *               && tcp && src port 21 && dst port > 1023 && ack,
 *            // FTP-7 omitted
 *            // FTP-8:
 *            allow src BASTION && dst INTERNALNET
 *               && tcp && src port > 1023 && dst port > 1023,
 *            // SMTP-2:
 *            allow src BASTION && dst INTERNAL_SMTP
 *               && tcp && src port 25 && dst port > 1023 && ack,
 *            // SMTP-3:
 *            allow src BASTION && dst INTERNAL_SMTP
 *               && tcp && src port > 1023 && dst port 25,
 *            // NNTP-2:
 *            allow src NNTP_FEED && dst INTERNAL_NNTP
 *               && tcp && src port 119 && dst port > 1023 && ack,
 *            // NNTP-3:
 *            allow src NNTP_FEED && dst INTERNAL_NNTP
 *               && tcp && src port > 1023 && dst port 119,
 *            // DNS-2:
 *            allow src BASTION && dst INTERNAL_DNS
 *               && udp && src port 53 && dst port 53,
 *            // DNS-4:
 *            allow src BASTION && dst INTERNAL_DNS
 *               && tcp && src port 53 && dst port > 1023 && ack,
 *            // DNS-5:
 *            allow src BASTION && dst INTERNAL_DNS
 *               && tcp && src port > 1023 && dst port 53,
 *            // Default-2:
 *            deny all);
 *
 * =h program read-only
 * Returns a human-readable definition of the program the IPFilter element
 * is using to classify packets. At each step in the program, four bytes
 * of packet data are ANDed with a mask and compared against four bytes of
 * classifier pattern.
 *
 * =a
 * IPClassifier, Classifier, CheckIPHeader, MarkIPHeader, CheckIPHeader2,
 * AddressInfo, tcpdump(1) */

class IPFilter : public Classifier {

  enum {
    UNKNOWN = -1000,
    NONE = 0,
    
    TYPE_HOST = 1, TYPE_NET = 2, TYPE_PORT = 3, TYPE_PROTO = 4,
    TYPE_TCPOPT = 5, TYPE_TOS = 6, TYPE_DSCP = 7, TYPE_ICMP_TYPE = 8,
    TYPE_IPFRAG = 9, TYPE_IPUNFRAG = 10,
    
    SD_SRC = 1, SD_DST = 2, SD_AND = 3, SD_OR = 4,

    OP_EQ = 0, OP_GT = 1, OP_LT = 2,
    
    PROTO_IP = 1,
    
    DATA_NONE = 0, DATA_IP = 1, DATA_IPNET = 2, DATA_PROTO = 3,
    DATA_PORT = 4, DATA_INT = 5, DATA_TCPOPT = 6, DATA_ICMP_TYPE = 7,
    
    IP_PROTO_TCP_OR_UDP = 0x10000,

    // if you change this, change click-fastclassifier.cc also
    TRANSP_FAKE_OFFSET = 64,
  };

  struct Primitive {
    
    int _type;
    int _srcdst;
    int _op;
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
    int _mask;
    bool _negated;
    bool _op_negated;
    
    Primitive()	{ clear(); }
    
    void clear();
    void set_type(int, int slot, ErrorHandler *);
    void set_srcdst(int, int slot, ErrorHandler *);
    void set_net_proto(int, int slot, ErrorHandler *);
    void set_transp_proto(int, int slot, ErrorHandler *);
    
    int set_mask(int *data, int full_mask, int shift, int, ErrorHandler *);
    int check(int slot, const Primitive &, ErrorHandler *);
    void add_exprs(Classifier *, Vector<int> &) const;
    
  };

  void length_checked_push(Packet *);
  
 public:
  
  IPFilter();
  ~IPFilter();
  
  const char *class_name() const		{ return "IPFilter"; }
  const char *processing() const		{ return PUSH; }

  void notify_noutputs(int);
  IPFilter *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int port, Packet *);
  
};

#endif

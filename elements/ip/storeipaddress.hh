#ifndef CLICK_STOREIPADDRESS_HH
#define CLICK_STOREIPADDRESS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
=c
StoreIPAddress(OFFSET)
StoreIPAddress(ADDR, OFFSET)
=s ip
stores IP address in packet
=d

The one-argument form writes the destination IP address annotation into the
packet at offset OFFSET, usually an integer. But if the annotation is zero, it
doesn't change the packet.

The two-argument form writes IP address ADDR into the packet at offset
OFFSET. ADDR can be zero.

The OFFSET argument may be the special string 'src' or 'dst'.  In this case,
incoming packets must be IP packets.  StoreIPAddress writes the address into
either the source or destination field of the IP packet header, as specified,
and incrementally updates the IP checksum (and, if appropriate, the TCP/UDP
checksum) to account for the change.

If OFFSET is out of range, the packet is dropped or emitted on optional output
1.

=n

Unless you use a special OFFSET of 'src' or 'dst', this element doesn't
recalculate any checksums.  If you store the address into an existing IP
packet, the packet's checksum will need to be set -- for example, with
SetIPChecksum. And don't forget that you might need to recalculate TCP and UDP
checksums as well. Here's a useful compound element:

  elementclass FixIPChecksums {
      // fix the IP checksum, and any embedded checksums that
      // include data from the IP header (TCP and UDP in particular)
      input -> SetIPChecksum
	  -> ipc :: IPClassifier(tcp, udp, -)
	  -> SetTCPChecksum
	  -> output;
      ipc[1] -> SetUDPChecksum -> output;
      ipc[2] -> output
  }

=a SetIPChecksum, SetTCPChecksum, SetUDPChecksum, IPAddrPairRewriter,
IPAddrRewriter
*/

class StoreIPAddress : public Element { public:

    StoreIPAddress() CLICK_COLD;
    ~StoreIPAddress() CLICK_COLD;

    const char *class_name() const		{ return "StoreIPAddress"; }
    const char *port_count() const		{ return PORTS_1_1X2; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    int _offset;
    IPAddress _address;
    bool _use_address;

};

CLICK_ENDDECLS
#endif

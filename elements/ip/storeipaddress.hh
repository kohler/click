#ifndef STOREADDRESS_HH
#define STOREADDRESS_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/*
=c
StoreIPAddress(OFFSET)
StoreIPAddress(ADDRESS, OFFSET)
=s IP, annotations
stores IP address in packet
=d
The one-argument form writes the destination IP address annotation into the
packet at offset OFFSET. But if the annotation is zero, it doesn't change
the packet.

The two-argument form writes ADDRESS into the packet at offset OFFSET. ADDRESS
can be zero.

=n
This element doesn't recalculate any checksums, so if you store the address
into an existing IP packet, the packet's checksum will need to be set
-- for example, with SetIPChecksum. And don't forget that transport protocols
might include IP header info in their checksums: TCP and UDP do, for example.
You'll need to recalculate their checksums as well. Here's a useful compound
element:

  elementclass FixChecksums {
      // fix the IP checksum, and any embedded checksums that
      // include data from the IP header (TCP and UDP in particular)
      input -> SetIPChecksum
	  -> ipc :: IPClassifier(tcp, udp, -)
	  -> SetTCPChecksum
	  -> output;
      ipc[1] -> SetUDPChecksum -> output;
      ipc[2] -> output
  }

=a
SetIPChecksum, SetTCPChecksum, SetUDPChecksum
*/

class StoreIPAddress : public Element { public:
  
  StoreIPAddress();
  ~StoreIPAddress();
  
  const char *class_name() const		{ return "StoreIPAddress"; }
  const char *processing() const		{ return AGNOSTIC; }
  StoreIPAddress *clone() const			{ return new StoreIPAddress; }
  
  int configure(Vector<String> &, ErrorHandler *);
  
  Packet *simple_action(Packet *);

 private:

  unsigned _offset;
  IPAddress _address;
  bool _use_address;
  
};

CLICK_ENDDECLS
#endif

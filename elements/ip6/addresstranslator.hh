#ifndef ADDRESSTRANSLATOR_HH
#define ADDRESSTRANSLATOR_HH

#include <click/ip6address.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/element.hh>
#include <time.h>


/*
 * =c
 * AddressTranslator(number_of_static_Mapping,
 *                   StaticPortMapping,
 *                   StaticMapping1,...
 *                   StaticMappingm,
 *                   DynamicMapping,
 *                   DynamicPortMapping,
 *                   AddressAllocationDirection,
 *                   Mapped_IP6Address1 Port_start1 Port_end1, ...
 *                   Mapped_IP6Addressn Port_startn Port_endn)   
 *
 *
 * =s IPv6
 * translates IP/ICMP, TCP, and UDP packets' addresses and ports
 *
 * =d
 * Translates IP/ICMP, TCP, and UDP packets by changing their source address, 
 * source port, destination address, and/or destination port.
 * 
 * Has one or more inputs and one or more outputs. Input packets must have
 * their IP6 header annotations set. Output packets are valid IP6 packets; for
 * instance, translated packets have their checksums updated.
 *
 * AddressTranslator maintains a table of mappings, which contains fields such 
 * as _iai, _ipi, _mai, _mpi, _ea, _ep, _t, _binding, _state, _static.  
 * For static mapping, the addresses (and ports) are mapped when the AddressTranslator 
 * is initiated. For dynamic mapping, mappings are created on the fly as new flows 
 * arrives from the direction that can be allocate new mapped address/port.
 * 
 * When a packet arrives, it will first check entries in the table, see if there 
 * exists an entry for the flow that the packet belongs to.  If there's 
 * such an entry,  then the packet's source address (and port) will be replaced with 
 * the mapped address (and port) of the entry for the outward packet and the packet's 
 * destination address (and port) will be replaced with the inner host's address (and 
 * port) for the inward packet.
 * If there is no such an entry, then the translator will create a binding for the new 
 * mapping if the flow comes from the right direction (the direction that allocates a 
 * new mapping is allowed).
 *
 * =a ProtocolTranslator64, ProtocolTranslator46 */

class AddressTranslator : public Element {
  
 public:
  
  AddressTranslator();
  ~AddressTranslator();
  
  const char *class_name() const		{ return "AddressTranslator"; }
  const char *processing() const	{ return AGNOSTIC; }
  AddressTranslator *clone() const { return new AddressTranslator; }
  int configure(const Vector<String> &, ErrorHandler *);
  void push(int port, Packet *p);

  void handle_outward(Packet *p);
  void handle_inward(Packet *p);
  
  bool lookup(IP6Address &, unsigned short &, IP6Address &, unsigned short &, IP6Address &, unsigned short &, bool);

private:
  struct EntryMap {
    IP6Address _iai;
    unsigned short _ipi;
    IP6Address _mai;
    unsigned short _mpi;
    IP6Address _ea;
    unsigned short _ep;
    time_t  _t;  //the last time that the packet passed the address translator
                 //later: the time when FIN received from both direction for TCP
                 //the time that the UDP packet has been sent for this session.
    unsigned char _state; 
    bool _binding;
    bool _static;
};
  Vector<EntryMap> _v;
  
  int _number_of_smap; // number of static-mapping entry
  bool _static_portmapping;
  bool _dynamic_mapping; 
  bool _dynamic_portmapping;
  bool _dynamic_mapping_allocation_direction;

  //the index of the following bool array corresponds to the colums of 
  //_iai, _ipi, _mai, _mpi, _ea, _ep
  bool _outwardLookupFields[6];
  bool _inwardLookupFields[6];
  bool _static_mapping[6];

  void add_map(IP6Address &mai, unsigned short mpi, bool binding);
  void add_map(IP6Address &iai, unsigned short ipi, IP6Address &mai, unsigned short mpi, IP6Address &ea, unsigned short ep, bool binding);

};
		   

#endif

#ifndef CLICK_ADDRESSTRANSLATOR_HH
#define CLICK_ADDRESSTRANSLATOR_HH
#include <click/ip6address.hh>
#include <click/ipaddress.hh>
#include <click/vector.hh>
#include <click/element.hh>
#include <click/bighashmap.hh>
#include <click/ip6flowid.hh>
CLICK_DECLS

//#include <time.h>


/*
 * =c
 * AddressTranslator(number_of_static_Mapping,
 *                   StaticPortMapping,
 *                   StaticMapping1,...
 *                   StaticMappingm,
 *                   DynamicMapping,
 *                   DynamicPortMapping,
 *                   AddressAllocationDirection,
 *                   Mapped_IP6Address Port_start Port_end)
 *
 *
 * =s ip6
 * translates IPv6/ICMPv6, TCP, and UDP packets' addresses and ports
 *
 * =d
 * Translates IPv6/ICMPv6, TCP, and UDP packets by changing their source address,
 * source port, destination address, and/or destination port.
 *
 * Has one or more inputs and one or more outputs. Input packets must have
 * their IP6 header annotations set. Output packets are valid IP6 packets; for
 * instance, translated packets have their checksums updated.
 *
 * AddressTranslator maintains a table of mappings for static address and port
 * mapping and dynamic address mapping.  It contains fields such
 * as _iai, _ipi, _mai, _mpi, _ea, _ep, _t, _binding, _state, _static.
 * For static mapping, the addresses (and ports) are mapped when the AddressTranslator
 * is initiated. For dynamic mapping, mappings are created on the fly as new flows
 * arrives from the direction that can be allocate new mapped address/port.
 *
 * For dynamic address and port configuration, the AddressTranslator maintains two tables
 * _in_map and _out_map to map flow ID for outward packet and inward packet respectively.
 *
 * If the AddressTranslator is configured as static mapping or dynamic address mapping,
 * when a packet arrives, the AddressTranslator will first check entries in the
 * address-mapping * table, see if there exists an entry for the flow that the packet
 * belongs to.  If there's such an entry,then the packet's source address (and port)
 * will be replaced with the mapped address (and port) of the entry for the outward packet
 * and the packet's destination address (and port) will be replaced with the inner host's
 * address (and port) for the inward packet.
 *
 * If there is no such an entry and it is configured for dynamic address mapping, then the
 * translator will create a binding for the new mapping if the flow comes from the right direction (the direction that allocates a new mapping is allowed). Otherwise, the packet is discarded.
 *
 * If the AddressTranslator is configured for dynamic address and port mapping,the
 * AddressTranslator will first check entries in the _in_map or _out_map table, depending on
 * packet's direction. It checks if the table has an entry whose flowID is the same as the packet.
 * If there is,  use the mapped flowID of that entry for the packet.  Otherwise, it will try to
 * find an unsed port and create a mapped flowID for the flow and insert the entry, if the packet
 * comes from the right direction.

 *
 * =a ProtocolTranslator64, ProtocolTranslator46 */

class AddressTranslator : public Element {

 public:

  class Mapping;

  AddressTranslator() CLICK_COLD;
  ~AddressTranslator() CLICK_COLD;

  const char *class_name() const		{ return "AddressTranslator"; }
  const char *port_count() const		{ return "2/2"; }
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  void push(int port, Packet *p);
  void add_map(IP6Address &mai,  bool binding);
  void add_map(IP6Address &iai, unsigned short ipi, IP6Address &mai, unsigned short mpi, IP6Address &ea, unsigned short ep, bool binding);
  void handle_outward(Packet *p);
  void handle_inward(Packet *p);

  bool lookup(IP6Address &, unsigned short &, IP6Address &, unsigned short &, IP6Address &, unsigned short &, bool);
  void cleanup(CleanupStage) CLICK_COLD;

protected:

 struct EntryMap {
    IP6Address _iai;
    unsigned short _ipi;
    IP6Address _mai;
    unsigned short _mpi;
    IP6Address _ea;
    unsigned short _ep;
    //long unsigned int _t;
     //time_t  _t;  //the last time that the packet passed the address translator
                 //later: the time when FIN received from both direction for TCP
                 //the time that the UDP packet has been sent for this session.
   //unsigned char _state;
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
  bool _static_mapping[6];


  //using new approach
  typedef HashMap<IP6FlowID, Mapping *> Map6;
  void clean_map(Map6 &, bool);
  unsigned short find_mport( );
  void mapping_freed(Mapping *, bool);

  Map6 _in_map;
  Map6 _out_map;
  IP6Address _maddr;
  unsigned short _mportl, _mporth;
  Mapping *_rover;
  Mapping *_rover2;
  int _nmappings;
  int _nmappings2;

};

class AddressTranslator::Mapping {

 public:

  Mapping() CLICK_COLD;
  void initialize(const IP6FlowID & new_flow) { _mapto = new_flow;     }
  const IP6FlowID &flow_id() const    {  return _mapto;        }
  unsigned short sport() const        { return _mapto.sport(); }
  unsigned short dport() const        { return _mapto.dport(); }
  //String s() const                  { return " ";    }
  Mapping * get_next()                { return _next; }
  Mapping * get_prev()                { return _prev; }
  void set_next(Mapping * next)       { _next = next; }
  void set_prev(Mapping * prev)       { _prev = prev; }
  Mapping *free_next() const		{ return _free_next; }
  void set_free_next(Mapping *m)      {_free_next = m;}


 protected:
  //long unsigned int _t;
  IP6FlowID _mapto;
  Mapping *_prev;
  Mapping *_next;
  Mapping *_free_next;

  friend class AddressTranslator;
};

CLICK_ENDDECLS
#endif

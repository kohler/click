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
 *                   StaticMappingFields,
 *                   StaticMapping1,..
 *                   StaticMappingm,
 *                   OutwardLookupFields,
 *                   InwardLookupFields,
 *                   Direction,
 *                   Mapped_IP6Address1 Port_start1 Port_end1, ..
 *                   Mapped_IP6Addressn Port_startn Port_endn)   
 *
 *
 * =s
 * translates UDP/TCP packets' addresses and ports
 *
 * =d
 * Rewrites UDP and TCP flows by changing their source address, source port,
 * destination address, and/or destination port.
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
 * When a packet arrives, it will first check every entry in the table, see if there 
 * exists an entry for the lookupfields, and do the translation to the address and port.  
 * If there is no such an entry, then the translator will create a binding for the new 
 * mapping if the flow comes from the right direction (the direction that allocate a 
 * new mapping is allowed).
 */

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

//    bool outward_lookup(IP6Address &, unsigned short &, IP6Address &, unsigned short &, IP6Address &, unsigned short &);
//    bool inward_lookup(IP6Address &, unsigned short &, IP6Address &, unsigned short &, IP6Address &, unsigned short &);
  
  bool lookup(IP6Address &, unsigned short &, IP6Address &, unsigned short &, IP6Address &, unsigned short &, bool);

private:
  struct EntryMap {
    IP6Address _iai;
    unsigned short _ipi;
    IP6Address _mai;
    unsigned short _mpi;
    IP6Address _ea;
    unsigned short _ep;
    time_t  _t;  //the time when FIN received from both direction for TCP
                //the time that the UDP packet has been sent for this session.
    unsigned char _state; 
    bool _binding;
    bool _static;
};
  Vector<EntryMap> _v;
  
  //bool _has_static_mapping;
  int _number_of_smap;
  bool _direction;
 
  //the index of the following bool array corresponds to the colums of 
  //_iai, _ipi, _mai, _mpi, _ea, _ep
  bool _outwardLookupFields[6];
  //bool _outwardMapFields[6];
  bool _inwardLookupFields[6];
  //bool _inwardMapFields[6];
  bool _static_mapping[6];

  void add_map(IP6Address &mai, unsigned short mpi, bool binding);
  void add_map(IP6Address &iai, unsigned short ipi, IP6Address &mai, unsigned short mpi, IP6Address &ea, unsigned short ep, bool binding);

};
		   

#endif

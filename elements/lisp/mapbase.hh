/*
 * mapbase.{cc,hh} -- Map-Server mapping database (LISPMappingDB)
 * Franck Messaoudi
 *
 * Copyright (c) 2014-2026 Franck Messaoudi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

/** @file mapbase.hh
 * @brief Map-Server mapping database (LISPMappingDB). */

#ifndef CLICK_MAPBASE_HH
#define CLICK_MAPBASE_HH

#include <click/element.hh>
#include <click/atomic.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/vector.hh>
#include <click/hashtable.hh>
#include "lispcommon.hh"
CLICK_DECLS

/*
=c

LISPMappingDB(I<keywords> ENTRY_CAPACITY, INTERVAL, REG_TIMEOUT)

=s lisp

Map-Server database: registered EID prefixes and locator sets

=d

Filled by LISPMapServer, searched by LISPMapResolver (longest-prefix match).
Mappings of an ETR that sent no Map-Register for REG_TIMEOUT seconds
(default 180) are removed.  Has no ports.

=h table read-only

The registered mappings.

=h entries, etrs, expired read-only

Counters.

=a LISPMapServer, LISPMapResolver
*/
/** @brief Map-Server database: registered EID prefixes and locator sets (Click element LISPMappingDB).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPMappingDB(7click) man page. */
class LISPMappingDB : public Element {
  public:
    LISPMappingDB();
    ~LISPMappingDB();

    /// Click Element interface
    const char *class_name() const { return "LISPMappingDB"; }
    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);

    /** @brief Record a Map-Register from an ETR.
       * @return EQL, MODIF or CRD (see LISPRegistrations::lookup) */
    int ms_lookup(IPAddress etr_rloc, uint16_t map_vrs, int *indice);
    /** @brief Store the records of a Map-Register.
       * @param rec_count number of records announced by the message
       * @param rec first record
       * @param indice index of the registering ETR
       * @param end end of the packet: records are not read past it */
    int ms_register(
	uint8_t rec_count, Record *rec, int indice, const uint8_t *end);
    LISPPrefixLengths _lens; ///< prefix lengths in mbase
    /** @brief Find the mapping that covers an EID (longest-prefix match).
       * @param eid requested EID
       * @param mask_len requested prefix length (not used: the match uses the address)
       * @param[out] aut_bit authoritative bit of the answer (always false)
       * @param[out] rloc locator chosen by priority and weight
       * @return 1 if found */
    int mr_lookup(IPAddress eid, uint16_t mask_len, bool *aut_bit,
	IPAddress *rloc /*,WritablePacket **p*/);

    /// Click Element interface
    int initialize(ErrorHandler *);
    /// Click Element interface
    void run_timer(Timer *);

    /// Click Element interface
    static String read_handler(Element *, void *);
    /// Click Element interface
    void add_handlers();

    /************************************************************/
    /** @brief A registered mapping, keyed by its EID prefix. */

    struct MapEntry : public LISPMapping {
	LISPPrefix prefix;   ///< key
	MapEntry *_hashnext; ///< HashContainer chain link

	typedef LISPPrefix key_type; ///< HashContainer key type
	typedef const LISPPrefix
	    &key_const_reference; ///< HashContainer key reference type
	/** @brief HashContainer key of this entry. */
	key_const_reference hashkey() const { return prefix; }

	/** @brief Entry for prefix @a p. */
	MapEntry(const LISPPrefix &p)
	  : prefix(p), _hashnext(0)
	{
	    eid_prefix = p.addr;
	    mask_len = p.len;
	}
    };

    enum { EQL, MODIF, ERR, CRD };
    enum { h_base, h_cnt };

  private:
    LISPRegistrations _regs; ///< ETRs that registered
    Timer _timer;            ///< expires stale registrations
    uint32_t _reg_timeout;   ///< seconds (REG_TIMEOUT)
    atomic_uint32_t _expired_count;

    typedef HashContainer<MapEntry> MBase;
    MBase _mbase;
    SizedHashAllocator<sizeof(MapEntry)> _alloc;

    // Timer _timer;

    LISPLock _lock;

    uint32_t _interval;
    atomic_uint32_t _entry_count;
    uint32_t _mbase_capacity;
};

CLICK_ENDDECLS
#endif

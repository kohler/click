/*
 * etrbase.{cc,hh} -- ETR database of the site's EID prefixes (LISPEtrDatabase)
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

/** @file etrbase.hh
 * @brief ETR database of the site's EID prefixes (LISPEtrDatabase). */

#ifndef CLICK_ETRBASE_HH
#define CLICK_ETRBASE_HH

#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/sync.hh>
#include <click/ipaddress.hh>
#include <click/list.hh>
#include "lispcommon.hh"
CLICK_DECLS

/*
=c

LISPEtrDatabase(RLOC [, EID1, EID2, EID3, EID4], I<keywords> EID, TTL)

=s lisp

ETR database: the EID prefixes of this site

=d

RLOC is this ETR's locator.  The positional EIDs are host EIDs (/32);
each EID keyword adds a prefix (for example C<EID 10.0.2.0/24>).  TTL is
the record TTL in minutes (default 600).  Used by LISPMapReplier,
LISPMapRegistrar and LISPMigrationUpdater; has no ports.

=h base read-only

The EID prefixes and the ITRs that asked for them.

=h entries, map_version, replies read-only

Counters.

=h add write-only

C<PREFIX [TTL]>: add an EID prefix.

=h remove write-only

C<PREFIX>: remove an EID prefix.

=a LISPMapReplier, LISPMapRegistrar, LISPMigrationUpdater
*/
/** @brief ETR database: the EID prefixes of this site (Click element LISPEtrDatabase).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPEtrDatabase(7click) man page. */
class LISPEtrDatabase : public Element {
  public:
    LISPEtrDatabase();
    ~LISPEtrDatabase();

    /// Click Element interface
    const char *class_name() const { return "LISPEtrDatabase"; }

    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    int initialize(ErrorHandler *errh);

    /** @brief Number of EID prefixes. */
    int size_table();
    /** @brief Add an EID prefix (address, prefix length, TTL in minutes).
     * @return 1, or 0 if out of memory */
    int insert(IPAddress, int, uint32_t);
    /** @brief Remove the host EID (/32) of a VM that left.
     * @return the ITRs that asked for it, to send them SMRs */
    Vector<IPAddress> remove_host(IPAddress);
    /// Click Element interface
    void cleanup(CleanupStage);
    /** @brief Record answering a Map-Request for an EID (longest-prefix match);
     * remembers the requesting ITR.
     * @return a packet holding the record, or null if no prefix covers the EID */
    Packet *reply_record(IPAddress, IPAddress);
    /** @brief Records for a Map-Register, one per EID prefix.
     * @param[out] rec_count number of records */
    Packet *register_records(uint8_t *);
    /** @brief True if a prefix covers the address. */
    bool find_ip(IPAddress);
    /// Click Element interface
    static String read_handler(Element *e, void *user_data);
    /// Click Element interface
    void add_handlers();

    /// Click Element interface
    static int write_handler(const String &, Element *, void *, ErrorHandler *);
    /** @brief Remove an EID prefix. @return 1 if it existed */
    int remove(const LISPPrefix &);
    enum { h_base, h_count, h_add, h_remove };
    /** @brief One EID prefix of the site, with the ITRs that asked for it. */

    struct BaseEntry {
	LISPPrefix prefix;    ///< key
	IPAddress eid_prefix; ///< prefix.addr
	int mask_len;         ///< prefix.len
	uint32_t ttl;         ///< minutes
	ItrRlocList list; ///< ITRs that asked for this prefix (to send SMRs)
	BaseEntry *_hashnext;        ///< HashContainer chain link
	typedef LISPPrefix key_type; ///< HashContainer key type
	typedef const LISPPrefix
	    &key_const_reference; ///< HashContainer key reference type
	/** @brief HashContainer key of this entry. */
	key_const_reference hashkey() const { return prefix; }

	/** @brief Entry for prefix @a p. */
	BaseEntry(const LISPPrefix &p)
	  : prefix(p), eid_prefix(p.addr), mask_len(p.len), ttl(0), _hashnext(0)
	{
	}

	/** @brief Remember an ITR that asked for this mapping (once per ITR). */
	void insert_itr(IPAddress itr_rloc)
	{
	    for (ItrRloc *r = list.front(); r; r = r->link.next())
		if (r->itr_addr == itr_rloc)
		    return;
	    ItrRloc *nw_itr = new ItrRloc();
	    nw_itr->itr_addr = itr_rloc;
	    list.push_back(nw_itr);
	}
	/** @brief Free the ITR list. */
	void free_itrs()
	{
	    while (ItrRloc *r = list.front()) {
		list.pop_front();
		delete r;
	    }
	}
    };

  private:
    LISPLock _lock;
    IPAddress _my_addr, _adr1, _adr2, _adr3,
	_adr4;                ///< adrN: host EIDs (/32), historical form
    Vector<LISPPrefix> _eids; ///< EID keyword prefixes
    uint32_t _ttl;            ///< record TTL, minutes
    LISPPrefixLengths _lens;
    SizedHashAllocator<sizeof(BaseEntry)> _alloc;
    typedef HashContainer<BaseEntry> Base;
    Base _base;
    atomic_uint32_t _entry_count;
    atomic_uint32_t _response;
    atomic_uint32_t _map_version;
};

CLICK_ENDDECLS
#endif

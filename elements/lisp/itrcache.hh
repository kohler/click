/*
 * itrcache.{cc,hh} -- ITR map-cache: EID prefix -> locator set (LISPMapCache)
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

/** @file itrcache.hh
 * @brief ITR map-cache: EID prefix -> locator set (LISPMapCache). */

#ifndef CLICK_ITR_CACHE_HH
#define CLICK_ITR_CACHE_HH

#include <click/element.hh>
#include <click/atomic.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
#include "lispcommon.hh"
CLICK_DECLS

/*
=c

LISPMapCache(I<keywords> RQST_TIMEOUT, CAPACITY, ENTRY_CAPACITY)

=s lisp

ITR map-cache: EID prefix -> locator set

=d

Holds the mappings learnt from Map-Replies (longest-prefix match, up to 5
locators, chosen by priority then weight per flow) and the pending
Map-Requests with the packets waiting for them.  A timer expires requests
without reply (RQST_TIMEOUT, jiffies, default 300) and mappings whose
record TTL (minutes) has passed.  Negative mappings (no locator) make
LISPMapRequester forward natively.  CAPACITY is the maximum number of queued
packets (default 2048), ENTRY_CAPACITY the maximum number of entries (0:
no limit).  Used by LISPMapRequester; has no ports.

=h cache read-only

Resolved mappings: prefix, RLOC, TTL, version.

=h entries, queued, replies, drops, modified read-only

Counters.

=a LISPMapRequester
*/
/** @brief ITR map-cache: EID prefix -> locator set (Click element LISPMapCache).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPMapCache(7click) man page. */
class LISPMapCache : public Element {
  public:
    /// Click Element interface
    const char *class_name() const { return "LISPMapCache"; }
    LISPMapCache();
    ~LISPMapCache();

    /// Click Element interface
    int configure(Vector<String> &conf, ErrorHandler *errh);
    /// Click Element interface
    static String read_handler(Element *e, void *user_data);
    /// Click Element interface
    int initialize(ErrorHandler *);
    /// Click Element interface
    void cleanup(CleanupStage);
    /// Click Element interface
    void run_timer(Timer *);
    /// Click Element interface
    void add_handlers();

    /** @brief Look up the mapping for an EID.
       * @param eid destination EID
       * @param[out] rloc locator chosen with @a flow_hash
       * @param flow_hash hash of the packet's flow (lisp_flow_hash())
       * @return VALID_REPLY, CAN_REQUEST (a request is pending), NATIVE_FWD (negative
       *         mapping) or NOT_EXIST */
    int lookup(IPAddress, IPAddress *, uint32_t flow_hash = 0);
    /** @brief Record a Map-Request sent for an EID: its UDP source port and nonces.
       * @return 1, or 0 if the cache is full */
    int insert(IPAddress, uint16_t, uint32_t, uint32_t);
    /** @brief Queue a packet until the Map-Reply for its EID arrives; dropped if
       * no request is pending. */
    void queue_packet(IPAddress, Packet *);

    /** @brief Find the pending request a Map-Reply record answers, by nonce
       * (the record's prefix may cover the requested EID without being equal).
       * @param eid,mask_len the record's EID prefix
       * @param nonce1,nonce2 nonce of the Map-Reply
       * @param[out] port UDP source port of the request
       * @return 1 if a pending request matches */
    int reply_check(IPAddress eid, int mask_len, uint32_t nonce1,
	uint32_t nonce2, uint16_t *port);
    /** @brief Store the mapping of a valid Map-Reply record.
       * @param rec the record
 * @param nonce1,nonce2 nonce of the Map-Reply
 * @param[out] locator locator for the released packets (0: negative mapping)
       * @return the packets that were waiting, linked by next() */
    Packet *insert_response(
	const Record *, uint32_t nonce1, uint32_t nonce2, IPAddress *);
    /** @brief True if @a rloc is a locator of the mapping that covers @a eid. */
    bool has_locator(IPAddress eid, IPAddress rloc);

    /** @brief Remove the resolved mapping that covers an EID. */
    void remove_mapping(IPAddress);

    /** @brief Remove every entry; queued packets are freed. */
    void clear();
    /** @brief Remove expired mappings (the caller holds the lock). */
    void resp_clean();
    /** @brief Remove requests without reply and drop excess queued packets
       * (the caller holds the lock). */
    void rqst_clean();

    /*************************/

    /** @brief Set the request timeout. */
    void set_timeout(const Timestamp &tmeout)
    {
	_rqst_timeout = tmeout.jiffies();
    }

    /*************************/

    /** @brief Number of queued packets. */
    uint32_t count() { return _packet_count; }

    /*************************/

    /** @brief Number of entries. */
    uint32_t length() { return _entry_count; }

    /*************************/

    enum { h_cache, h_insert, h_clear };
    enum {
	NOT_EXIST,
	VALID_REPLY,
	NOT_VALID,
	VALID_REQUEST,
	CAN_REQUEST,
	WITHOUT_RESPONSE,
	NATIVE_FWD
    };

    /**********************************************/
    /** @brief A pending Map-Request (key: requested EID/32) or a resolved mapping (key: mapped prefix). */

    struct CacheEntry {
	LISPPrefix
	    prefix; ///< key: requested EID/32 while pending, mapped prefix once resolved
	IPAddress eid_prefix;        ///< prefix.addr
	IPAddress rloc;              ///< first locator
	Locator locs[LISP_MAX_LOC];  ///< locator set (resolved entries)
	int nlocs;                   ///< number of locators in locs
	unsigned version_num : 12;   ///< map-version of the Map-Reply
	uint32_t rec_ttl;            ///< record TTL, minutes
	click_jiffies_t insert_time; ///< time the mapping was stored
	click_jiffies_t send_time;   ///< time the Map-Request was sent
	Packet *list_head;           ///< first packet waiting for the Map-Reply
	Packet *list_tail;           ///< last packet waiting for the Map-Reply

	uint32_t randnonce1; ///< nonce of the pending Map-Request (first half)
	uint32_t randnonce2; ///< nonce of the pending Map-Request (second half)
	uint16_t randport;   ///< UDP source port of the pending Map-Request
	bool complete;       ///< true once a Map-Reply resolved the entry
	CacheEntry *_hashnext;        ///< HashContainer chain link
	List_member<CacheEntry> link; ///< _rqst_list or _resp_list link

	/*******************************************************/

	typedef LISPPrefix key_type; ///< HashContainer key type
	typedef const LISPPrefix
	    &key_const_reference; ///< HashContainer key reference type
	/** @brief HashContainer key of this entry. */
	key_const_reference hashkey() const { return prefix; }
	/** @brief Filter for lisp_lpm(): resolved entries only. */
	static bool is_complete(const CacheEntry *e) { return e->complete; }

	/*************************************************************************/
	/** @brief True if a new Map-Request may be sent (LISP_REQUEST_INTERVAL_MSEC elapsed). */
	bool can_request(click_jiffies_t now)
	{
	    return click_jiffies_less(
		send_time + LISP_REQUEST_INTERVAL_JIFFIES, now);
	}

	// Record TTL is in minutes (RFC 9301)
	/** @brief True if the record TTL (minutes) has passed. */
	bool resp_expired(click_jiffies_t now)
	{
	    return click_jiffies_less(
		insert_time + (click_jiffies_t)rec_ttl * 60 * CLICK_HZ, now);
	}

	/** @brief True if no Map-Reply came within @a tmeout jiffies. */
	bool rqst_expired(click_jiffies_t now, uint32_t tmeout) const
	{
	    return click_jiffies_less(send_time + tmeout, now);
	}

	/** @brief Entry for @a key. */
	CacheEntry(const LISPPrefix &key)
	  : prefix(key),
	    eid_prefix(key.addr),
	    nlocs(0),
	    rec_ttl(0),
	    insert_time(0),
	    send_time(0),
	    list_head(0),
	    list_tail(0),
	    complete(false),
	    _hashnext(0)
	{
	}
	/*************************************************************************/
    };

    /** @brief Find or create the pending entry for an EID (takes the lock). */
    CacheEntry *pending_entry(IPAddress eid);
    CacheEntry *pending_entry_locked(IPAddress eid); ///< caller holds _lock
    /** @brief Pending request covered by @a prefix with these nonces (the caller holds the lock). */
    CacheEntry *pending_for(
	const LISPPrefix &prefix, uint32_t nonce1, uint32_t nonce2);

  private:
    LISPLock _lock;
    typedef HashContainer<CacheEntry> Cache;
    Cache _cache;
    typedef List<CacheEntry, &CacheEntry::link> EntryList;
    EntryList _resp_list;
    EntryList _rqst_list;
    SizedHashAllocator<sizeof(CacheEntry)> _alloc;
    LISPPrefixLengths _lens;
    Timer _timer; ///< expires requests and mappings every second
    uint32_t
	_rqst_timeout; ///< If we don't receive a Map-Reply after _rqst_timeout, we remove the entry
    atomic_uint32_t _entry_count;
    atomic_uint32_t _rply_count;
    uint32_t _entry_capacity;
    atomic_uint32_t _packet_count;
    uint32_t _packet_capacity;
    atomic_uint32_t _drops;
    atomic_uint32_t _modif_entry;
};

/* ---- inline functions ---- */

/****************************/

inline int
LISPMapCache::lookup(IPAddress eid, IPAddress *rloc, uint32_t flow_hash)
{
    _lock.acquire_read();
    int rslt = NOT_EXIST;
    if (_entry_count) {
	// resolved mapping: longest-prefix match, then pick a locator
	CacheEntry *e = lisp_lpm(_cache, _lens, eid, &CacheEntry::is_complete);
	int li = e ? lisp_choose_locator(e->locs, e->nlocs, flow_hash) : -1;
	if (li >= 0) {
	    *rloc = e->locs[li].locator;
	    rslt = VALID_REPLY;
	} else if (e && e->nlocs == 0) {
	    rslt = NATIVE_FWD; // negative mapping: not a LISP destination
	} else if ((e = _cache.get(LISPPrefix(eid, 32))) && !e->complete
	    && !e->rqst_expired(click_jiffies(), _rqst_timeout)) {
	    rslt = CAN_REQUEST; // a request is already pending
	}
    }
    _lock.release_read();
    return rslt;
}

/****************************/

inline int
LISPMapCache::reply_check(IPAddress eid, int mask_len, uint32_t nonce1,
    uint32_t nonce2, uint16_t *port)
{
    _lock.acquire_read();
    CacheEntry *e = pending_for(LISPPrefix(eid, mask_len), nonce1, nonce2);
    if (e)
	*port = e->randport;
    _lock.release_read();
    return e != 0;
}

CLICK_ENDDECLS
#endif

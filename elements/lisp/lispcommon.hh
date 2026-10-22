/*
 * lispcommon.{cc,hh} -- code shared by the LISP elements: IP/UDP header builder,
 * prefix tables, locator choice, SHA-256 and HMAC, locking
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

#ifndef CLICK_LISPCOMMON_HH
#define CLICK_LISPCOMMON_HH
/** @file lispcommon.hh
 * @brief C++ types and helpers shared by the LISP elements.
 *
 * Wire formats are in <clicknet/lisp.h>.  The out-of-line parts
 * (LISPIPHeader, SHA-256 / HMAC) are in lispcommon.cc; elements that use
 * them declare ELEMENT_REQUIRES(LISPCommon).
 */
#include <click/glue.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/list.hh>
#include <click/packet.hh>
#include <click/hashcontainer.hh>
#include <click/straccum.hh>
#include <click/string.hh>
#include <click/hashtable.hh>
#include <click/vector.hh>
#include <click/atomic.hh>
#include <click/args.hh>
#include <click/sync.hh>
#include <clicknet/udp.h>
#include <clicknet/lisp.h>
CLICK_DECLS

/** @brief Default Map-Register interval once a Map-Notify confirmed it (seconds, RFC 9301). */
#define STABLE_P 60
/** @brief Default Map-Register retry interval until a Map-Notify arrives (seconds). */
#define CHANGE_P 20

/** @brief Minimum interval between two Map-Requests for the same EID (milliseconds). */
#define LISP_REQUEST_INTERVAL_MSEC 1
/** @brief LISP_REQUEST_INTERVAL_MSEC in jiffies, rounded up. */
#define LISP_REQUEST_INTERVAL_JIFFIES                                          \
    ((click_jiffies_t)((LISP_REQUEST_INTERVAL_MSEC * CLICK_HZ + 999) / 1000))

/* ---- header access ---- */

/** @brief IP header of a received packet.
 *
 * The IP header annotation when it is set (CheckIPHeader, MarkIPHeader, or
 * the element that built the packet) and inside the packet data, otherwise
 * the first byte of the packet. */
static inline const click_ip *
lisp_ip_header(const Packet *p)
{
    if (p->has_network_header() && p->network_header() >= p->data()
	&& p->network_header() < p->end_data())
	return p->ip_header();
    return reinterpret_cast<const click_ip *>(p->data());
}
/** @brief Writable variant of lisp_ip_header(const Packet *). */
static inline click_ip *
lisp_ip_header(Packet *p)
{
    return const_cast<click_ip *>(
	lisp_ip_header(static_cast<const Packet *>(p)));
}
/** @brief UDP header after @a iph, using the real IP header length (IP options). */
static inline const click_udp *
lisp_udp_header(const click_ip *iph)
{
    return reinterpret_cast<const click_udp *>(
	reinterpret_cast<const uint8_t *>(iph) + (iph->ip_hl << 2));
}
/** @brief Writable variant of lisp_udp_header(const click_ip *). */
static inline click_udp *
lisp_udp_header(click_ip *iph)
{
    return reinterpret_cast<click_udp *>(
	reinterpret_cast<uint8_t *>(iph) + (iph->ip_hl << 2));
}

/* ---- record helpers ---- */

/** @brief Record TTL in minutes (host order). */
static inline uint32_t
lisp_record_ttl(const Record *r)
{
    return ntohl(r->rec_ttl);
}
/** @brief 12-bit map-version of a record (host order). */
static inline uint16_t
lisp_record_version(const Record *r)
{
    return ntohs(r->version) & LISP_REC_VERSION_MASK;
}
/** @brief A bit (authoritative) of a record. */
static inline bool
lisp_record_auth(const Record *r)
{
    return ntohs(r->act_a_reserved) & LISP_REC_F_A;
}

/** @brief Fill a record with an IPv4 EID and one IPv4 locator, in network order.
 * @param r         record to fill (28 bytes)
 * @param ttl       record TTL, minutes
 * @param mask_len  EID prefix length
 * @param version   12-bit map-version
 * @param auth      A bit
 * @param eid       EID prefix
 * @param priority  locator priority (255: unusable)
 * @param weight    locator weight
 * @param rloc      locator address
 * @param loc_flags LISP_LOC_F_* bits; an ETR describing its own locator
 *                  sets LISP_LOC_F_LOCAL | LISP_LOC_F_REACH */
static inline void
lisp_record_fill(Record *r, uint32_t ttl, uint8_t mask_len, uint16_t version,
    bool auth, IPAddress eid, uint8_t priority, uint8_t weight, IPAddress rloc,
    uint16_t loc_flags = 0)
{
    r->rec_ttl = htonl(ttl);
    r->loc_count = 1;
    r->mask_len = mask_len;
    r->act_a_reserved = htons(auth ? LISP_REC_F_A : 0);
    r->version = htons(version & LISP_REC_VERSION_MASK);
    r->eid_afi = htons(LISP_AFI_IP);
    r->eid_prefix = eid.in_addr();
    r->loc.priority = priority;
    r->loc.weight = weight;
    r->loc.m_priority = 0;
    r->loc.m_weight = 0;
    r->loc.flags = htons(loc_flags);
    r->loc.loc_afi = htons(LISP_AFI_IP);
    r->loc.locator = rloc.in_addr();
}

/** @brief Bits that must be zero in the first word of a control message.
 * @param type message type (LISP_MAP_REQUEST, ...)
 * @return mask in host order (RFC 9301 section 5); 0 for unknown types */
static inline uint32_t
lisp_ctrl_reserved_mask(unsigned type)
{
    switch (type) {
    case LISP_MAP_REQUEST:
	return 0x000F8000; /* Rsvd, bits 19-15 */
    case LISP_MAP_REPLY:
	return 0x01FFFF00; /* bits 24-8 */
    case LISP_MAP_REGISTER:
	return 0x01FFE000; /* bits 24-13 */
    case LISP_MAP_NOTIFY:
	return 0x03FFFF00; /* bits 25-8 */
    case LISP_SHARED_EXTENSION:
	return 0x0000FF00; /* bits 15-8 */
    case LISP_ECM:
	return 0x00FFFFFF; /* bits 23-0 */
    default:
	return 0;
    }
}

/** @brief Write the Source-EID of a Map-Request (not 4-byte aligned). */
static inline void
map_rqst_set_source_eid(click_lisp_map_request *m, IPAddress a)
{
    uint32_t v = a.addr();
    memcpy(m->source_eid, &v, 4);
}
/** @brief Read the Source-EID of a Map-Request. */
static inline IPAddress
map_rqst_source_eid(const click_lisp_map_request *m)
{
    uint32_t v;
    memcpy(&v, m->source_eid, 4);
    return IPAddress(v);
}

/* ---- prefixes and longest-prefix match ---- */

/** @brief An EID prefix, used as the key of the mapping tables.
 *
 * The address is always masked, so equal prefixes have equal keys. */
struct LISPPrefix {
    IPAddress addr; ///< prefix address, masked to @a len bits
    int len;        ///< prefix length, 0-32
    LISPPrefix()
      : len(0)
    {
    } ///< empty prefix 0.0.0.0/0
    /** @brief Prefix of length @a l containing @a a (the address is masked). */
    LISPPrefix(IPAddress a, int l)
      : addr(a & IPAddress::make_prefix(l)), len(l)
    {
    }
    hashcode_t hashcode() const
    {
	return addr.hashcode() + (hashcode_t)len * 0x9E3779B1U;
    } ///< hash for HashContainer
    /** @brief True if address @a a is inside this prefix. */
    bool contains(IPAddress a) const
    {
	return (a & IPAddress::make_prefix(len)) == addr;
    }
    IPAddress mask() const
    {
	return IPAddress::make_prefix(len);
    } ///< netmask of the prefix
};
/** @brief Equal address and length. */
inline bool
operator==(const LISPPrefix &a, const LISPPrefix &b)
{
    return a.addr == b.addr && a.len == b.len;
}
/** @brief Print as ADDR/LEN. */
inline StringAccum &
operator<<(StringAccum &sa, const LISPPrefix &p)
{
    return sa << p.addr << '/' << p.len;
}

/** @brief Number of prefixes of each length in a table.
 *
 * Lets lisp_lpm() probe only the prefix lengths that are in use. */
class LISPPrefixLengths {
  public:
    LISPPrefixLengths() { clear(); }
    void clear() { memset(_n, 0, sizeof(_n)); } ///< forget every length
    void add(int len) { ++_n[len]; } ///< a prefix of length @a len was added
    void remove(int len)
    {
	if (_n[len])
	    --_n[len];
    } ///< a prefix of length @a len was removed
    /** @brief Largest prefix length <= @a len that is in use, or -1. */
    int next(int len) const
    {
	for (; len >= 0; --len)
	    if (_n[len])
		return len;
	return -1;
    }

  private:
    uint32_t _n[33];
};

/** @brief Longest-prefix match in a HashContainer keyed by LISPPrefix.
 * @param table  the table
 * @param lens   prefix lengths present in @a table
 * @param a      address to look up
 * @param accept optional filter; entries it rejects are skipped
 * @return the most specific entry that covers @a a, or null */
template <typename T>
inline T *
lisp_lpm(const HashContainer<T> &table, const LISPPrefixLengths &lens,
    IPAddress a, bool (*accept)(const T *) = 0)
{
    for (int l = lens.next(32); l >= 0; l = lens.next(l - 1))
	if (T *e = table.get(LISPPrefix(a, l)))
	    if (!accept || accept(e))
		return e;
    return 0;
}

/* ---- locator sets ---- */

/** @brief Size in bytes of a record with an IPv4 EID and loc_count IPv4 locators. */
static inline unsigned
lisp_record_size(const Record *r)
{
    return 16 + 12 * r->loc_count;
}
/** @brief Locator number @a i of a record (0 <= i < loc_count). */
static inline const Locator *
lisp_record_locator(const Record *r, int i)
{
    return &r->loc + i;
}

/** @brief Choose a locator from a locator set (RFC 9300 section 10).
 *
 * Takes the lowest priority below 255, then picks by weight using @a hash,
 * so that one flow always uses the same RLOC.
 * @return index into @a locs, or -1 if no locator is usable */
static inline int
lisp_choose_locator(const Locator *locs, int n, uint32_t hash)
{
    int best = 255;
    for (int i = 0; i < n; ++i)
	if (locs[i].priority < best)
	    best = locs[i].priority;
    if (best == 255)
	return -1;
    uint32_t total = 0, count = 0;
    for (int i = 0; i < n; ++i)
	if (locs[i].priority == best)
	    total += locs[i].weight, ++count;
    uint32_t pick = total ? hash % total : hash % count;
    for (int i = 0; i < n; ++i)
	if (locs[i].priority == best) {
	    uint32_t w = total ? locs[i].weight : 1;
	    if (pick < w)
		return i;
	    pick -= w;
	}
    return -1;
}

/** @brief Hash of a packet's flow: addresses, protocol and, for unfragmented
 * TCP and UDP, ports.
 * @param in  IP header
 * @param len number of bytes available from @a in */
static inline uint32_t
lisp_flow_hash(const click_ip *in, uint32_t len)
{
    if (len < sizeof(click_ip))
	return 0;
    uint32_t h = in->ip_src.s_addr ^ in->ip_dst.s_addr ^ in->ip_p;
    uint32_t hl = in->ip_hl << 2;
    if ((in->ip_p == IP_PROTO_TCP || in->ip_p == IP_PROTO_UDP) && !IP_ISFRAG(in)
	&& len >= hl + 4) {
	const uint16_t *ports = reinterpret_cast<const uint16_t *>(
	    reinterpret_cast<const uint8_t *>(in) + hl);
	h ^= ports[0] ^ ((uint32_t)ports[1] << 16);
    }
    h ^= h >> 16;
    h ^= h >> 8;
    return h;
}

/** @brief IP/UDP header of the packets the LISP elements send.

   Usage in configure():
   @code
       Args args(conf, this, errh);
       args.read_mp(...element positional arguments...);
       _hdr.read_args(args);          // PROTO, SRC, DST + TOS TTL DSCP ECT CE DF
       args.read(...element keywords...);
       if (args.complete() < 0 || _hdr.finish_configure(this, errh) < 0)
           return -1;
   @endcode
   Building a packet: make (or push) room for the headers, write the LISP
   message after them, then fill() and finally finish_udp(). */
class LISPIPHeader {
  public:
    LISPIPHeader();
    /** @brief Read PROTO, SRC, DST (positional) and TOS, TTL, DSCP, ECT, CE, DF. */
    void read_args(Args &args);
    /** @brief Check the arguments read by read_args() and build the template.
     * @return 0 on success, negative on error (reported to @a errh) */
    int finish_configure(Element *e, ErrorHandler *errh);
    /** @brief Set the template directly, for elements without PROTO/SRC/DST. */
    void set(IPAddress src, IPAddress dst, int ttl = 250);

    /** @brief Write the IP and UDP headers at the start of @a q.
     * @param q     packet whose first 28 bytes are the headers
     * @param dst   explicit destination; if 0, the destination annotation
     *              (DST_ANNO) or DST is used
     * @param sport UDP source port, network order
     * @param dport UDP destination port, network order */
    void fill(WritablePacket *q, IPAddress dst, uint16_t sport, uint16_t dport);
    /** @brief Compute the UDP checksum, last, over the finished packet. */
    static void finish_udp(WritablePacket *q);

    IPAddress src() const { return _iph.ip_src; } ///< source address (SRC)
    bool use_dst_anno() const { return _use_dst_anno; } ///< DST was DST_ANNO
    static const unsigned headers
	= sizeof(click_ip) + sizeof(click_udp); ///< bytes written by fill()

  private:
    click_ip _iph;
    bool _use_dst_anno;
    atomic_uint32_t _id;
    int _proto, _tos, _dscp;
    bool _ce, _df;
    String _ect_str, _dst_str;
};

/* ---- authentication (lispcommon.cc; ELEMENT_REQUIRES(LISPCommon)) ---- */

/** @brief SHA-256 (FIPS 180-4) of @a data. */
void lisp_sha256(const uint8_t *data, size_t len, uint8_t out[32]);
/** @brief HMAC-SHA-256 (RFC 2104) of @a data with @a key. */
void lisp_hmac_sha256(const uint8_t *key, size_t keylen, const uint8_t *data,
    size_t len, uint8_t out[32]);
/** @brief Sign a control message with HMAC-SHA-256-128 (RFC 9301 Key ID 2).
 * @param msg      message, from its first word
 * @param len      message length
 * @param auth_off offset of the 16-byte authentication field (set to 0,
 *                 then to the first 16 bytes of the HMAC)
 * @param key      shared secret */
void lisp_auth_sign(
    uint8_t *msg, size_t len, size_t auth_off, const String &key);
/** @brief Check a control message signed by lisp_auth_sign().
 * @return true if the HMAC matches (compared in constant time) */
bool lisp_auth_verify(
    const uint8_t *msg, size_t len, size_t auth_off, const String &key);

/** @brief Lock for the LISP tables.

     Click's ReadWriteLock only locks in the SMP
   Linux kernel module; Spinlock also locks in multithreaded userlevel
   builds, and may be taken again by the thread that holds it.  Readers and
   writers are serialized: the tables are small. */
class LISPLock {
  public:
    void acquire_read() { _l.acquire(); }  ///< lock for reading
    void release_read() { _l.release(); }  ///< unlock after acquire_read()
    void acquire_write() { _l.acquire(); } ///< lock for writing
    void release_write() { _l.release(); } ///< unlock after acquire_write()
  private:
    Spinlock _l;
};

/* ---- mapping database shared by LISPMappingDB and FMCCDatabase ---- */

/** @brief One registered mapping: EID prefix -> locator set. */
struct LISPMapping {
    IPAddress eid_prefix;       ///< EID prefix (or VM address in FMCCDatabase)
    uint16_t mask_len;          ///< prefix length
    unsigned version_num;       ///< map-version of the last registration
    bool aut_bit;               ///< A bit of the last registration
    uint32_t rec_ttl;           ///< record TTL, minutes
    Locator loc;                ///< first locator (locs[0])
    Locator locs[LISP_MAX_LOC]; ///< locator set
    int nlocs;                  ///< number of locators in locs
    int reg_idce; ///< index of the registering ETR in LISPRegistrations

    LISPMapping()
      : mask_len(0),
	version_num(0),
	aut_bit(false),
	rec_ttl(0),
	nlocs(0),
	reg_idce(-1)
    {
	memset(&loc, 0, sizeof(loc));
    }
    IPAddress regster_adr() const { return loc.locator; } ///< first locator
    /** @brief Copy up to LISP_MAX_LOC locators from record @a r. */
    void copy_locators(const Record *r)
    {
	nlocs = r->loc_count < LISP_MAX_LOC ? r->loc_count : LISP_MAX_LOC;
	for (int i = 0; i < nlocs; ++i)
	    locs[i] = *lisp_record_locator(r, i);
	if (nlocs)
	    loc = locs[0];
	else
	    memset(&loc, 0, sizeof(loc));
    }
    /** @brief Initialize from a registered record; @a indice is the ETR's index. */
    void set_with(const Record *r, int indice)
    {
	mask_len = r->mask_len;
	version_num = lisp_record_version(r);
	aut_bit = lisp_record_auth(r);
	rec_ttl = lisp_record_ttl(r);
	reg_idce = indice;
	copy_locators(r);
    }
    /** @brief Update from a new registration of the same prefix.
     * @return true if the first locator changed (the EID moved) */
    bool update_with(const Record *r, int indice)
    {
	bool changed = IPAddress(loc.locator) != IPAddress(r->loc.locator);
	if (changed)
	    reg_idce = indice;
	copy_locators(r);
	mask_len = r->mask_len;
	version_num = lisp_record_version(r);
	rec_ttl = lisp_record_ttl(r);
	return changed;
    }
};

/** @brief ETRs that sent Map-Registers, with the map-version of their last one.
 *
 * Not locked: the owner (LISPMappingDB, FMCCDatabase) holds its own lock. */
class LISPRegistrations {
  public:
    /** @brief Results of lookup(); LISPMapServer and FMCCManager use the
     * same values. */
    enum { EQL, MODIF, ERR, CRD };

    /** @brief One ETR. */
    struct EtrRegistration {
	IPAddress etr_rloc;   ///< the ETR's RLOC (source of its Map-Registers)
	uint16_t map_version; ///< map-version of its last Map-Register
	click_jiffies_t registr_time; ///< time of its last Map-Register
	/** @brief True if no Map-Register arrived for @a timeout jiffies. */
	bool reg_expired(click_jiffies_t now, click_jiffies_t timeout) const
	{
	    return click_jiffies_less(registr_time + timeout, now);
	}
    };

    LISPRegistrations()
      : _etr_count(0)
    {
    }

    /** @brief Record a Map-Register from @a etr_rloc.
     * @param etr_rloc  the ETR
     * @param map_vrs   map-version of the message
     * @param[out] indice index of the ETR in _etrs
     * @return EQL (same map-version as last time), MODIF (new version) or
     *         CRD (new ETR) */
    int lookup(IPAddress etr_rloc, uint16_t map_vrs, int *indice)
    {
	HashTable<IPAddress, int>::iterator found = _index.find(etr_rloc);
	if (found) {
	    int i = found.value();
	    _etrs[i].registr_time = click_jiffies();
	    *indice = i;
	    int r = _etrs[i].map_version == map_vrs ? EQL : MODIF;
	    _etrs[i].map_version = map_vrs;
	    return r;
	}
	EtrRegistration e;
	e.etr_rloc = etr_rloc;
	e.map_version = map_vrs;
	e.registr_time = click_jiffies();
	_etrs.push_back(e);
	++_etr_count;
	*indice = _etrs.size() - 1;
	_index.set(etr_rloc, *indice);
	return CRD;
    }

    Vector<EtrRegistration> _etrs; ///< indexed by LISPMapping::reg_idce
    uint32_t _etr_count;           ///< number of ETRs seen
  private:
    HashTable<IPAddress, int> _index;
};

/* ---- internal types ---- */

/** @brief Migration decision taken by FMCCManager. */
struct Decision {
    bool decision;         ///< migrate or not
    IPAddress dc_src_rloc; ///< RLOC of the data center the VM leaves
    IPAddress dc_dst_rloc; ///< RLOC of the data center the VM goes to
    IPAddress vm_addr;     ///< the VM's EID
    uint16_t mask;         ///< EID prefix length (32 for a VM)
};

/** @brief ITRs that asked an ETR for a mapping (used to send SMRs). */
struct ItrRloc {
    IPAddress itr_addr;        ///< the ITR's RLOC
    List_member<ItrRloc> link; ///< list link
};
/** @brief List of ItrRloc: the ITRs that asked an ETR for a prefix. */
typedef List<ItrRloc, &ItrRloc::link> ItrRlocList;

CLICK_ENDDECLS
#endif

#ifndef CLICKNET_LISP_H
#define CLICKNET_LISP_H
#include <clicknet/ip.h> /* struct in_addr */

/** @file clicknet/lisp.h
 * @brief LISP wire formats (RFC 9300, RFC 9301) and the FMCC Map-Migrate
 * extension: byte-exact structs, integers in network byte order.
 *
 * Build the first 32-bit word of a message in host order with the masks
 * below, then store it with htonl(). */

/*
 * <clicknet/lisp.h> -- LISP wire formats (RFC 9300, RFC 9301) and the
 * FMCC Map-Migrate extension.  Plain C: fixed-width fields, no bit-fields.
 * Integers are in network byte order; addresses are struct in_addr.
 *
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
 *
 * Data-plane header (RFC 9300 section 5.3):
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |N|L|E|V|I|flags|            Nonce/Map-Version                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                 Instance ID/Locator-Status-Bits               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Both words are in network byte order.  Build them in host order with
 * the masks below, then store with htonl().
 */

/** @brief LISP data-plane header, 8 bytes (RFC 9300 section 5.3). */
struct click_lisp {
    uint32_t
	lisp_flags_nonce; /**< 0-3  N L E V I flags(3) | nonce or map-versions */
    uint32_t lisp_iid_lsb; /**< 4-7  instance ID(24) + LSB(8), or LSB(32) */
};

#define LISP_F_N        0x80000000U /**< nonce present */
#define LISP_F_L        0x40000000U /**< locator-status-bits valid */
#define LISP_F_E        0x20000000U /**< echo-nonce request */
#define LISP_F_V        0x10000000U /**< map-version present */
#define LISP_F_I        0x08000000U /**< instance ID present */
#define LISP_F_FLAGS    0x07000000U /**< reserved flags */
#define LISP_NONCE_MASK 0x00FFFFFFU /**< 24-bit nonce (with LISP_F_N) */
#define LISP_SRC_MAPVER_SHIFT                                                  \
    12 /**< source map-version position (with LISP_F_V) */
#define LISP_MAPVER_MASK 0x0FFFU /**< 12-bit map-version */
#define LISP_IID_SHIFT   8       /**< instance ID position in the second word */
#define LISP_LSB8_MASK                                                         \
    0x000000FFU /**< 8 locator-status-bits when an instance ID is present */

/* Ports, address families, control message types. */
#define LISP_DATA_PORT        4341 /**< UDP port of LISP data packets */
#define LISP_CONTROL_PORT     4342 /**< UDP port of LISP control messages */
#define LISP_AFI_IP           1    /**< address family IPv4 */
#define LISP_AFI_IP6          2    /**< address family IPv6 (not supported) */
#define LISP_MAX_LOC          5 /**< maximum number of locators kept per mapping */
#define LISP_MAP_REQUEST      1  /**< control message type: Map-Request */
#define LISP_MAP_REPLY        2  /**< control message type: Map-Reply */
#define LISP_MAP_REGISTER     3  /**< control message type: Map-Register */
#define LISP_MAP_NOTIFY       4  /**< control message type: Map-Notify */
#define LISP_ECM              8  /**< Encapsulated Control Message */
#define LISP_SHARED_EXTENSION 15 /**< RFC 8113 shared extension message */
/* FMCC Map-Migrate is carried as a shared extension message with this
   sub-type.  Placeholder value: check the IANA "LISP Shared Extension
   Message Type Sub-types" registry before deploying it. */
#define LISP_FMCC_MIGRATE_SUBTYPE                                              \
    0xFC0 /**< type-15 sub-type of Map-Migrate (provisional) */

/* Common first 32-bit word of every control message (host order after ntohl):
   Type(4) | message-specific flags | Record Count(8). */
#define LISP_CTRL_TYPE_SHIFT 28 /**< position of the type in the first word */
#define LISP_CTRL_TYPE(w)                                                      \
    ((w)                                                                       \
	>> LISP_CTRL_TYPE_SHIFT) /**< type of a control message (first word, host order) */
#define LISP_CTRL_COUNT(w)                                                     \
    ((w) & 0xFF)                   /**< record count (first word, host order) */
#define LISP_CTRL_SUBTYPE_SHIFT 16 /**< type 15 only: 12-bit sub-type */
#define LISP_CTRL_SUBTYPE(w)                                                   \
    (((w) >> LISP_CTRL_SUBTYPE_SHIFT)                                          \
	& 0xFFF) /**< sub-type of a type-15 message (first word, host order) */

/* EID records and locators (RFC 9301 section 5.4). */
#define LISP_LOC_F_LOCAL 0x0004 /**< L: local locator */
#define LISP_LOC_F_PROBE 0x0002 /**< p: RLOC-probe reply */
#define LISP_LOC_F_REACH 0x0001 /**< R: reachable */

/** @brief IPv4 locator inside a record, 12 bytes (RFC 9301 section 5.4). */
typedef struct click_lisp_locator /* IPv4 locator, 12 bytes */
{
    uint8_t priority;       /**< 0 */
    uint8_t weight;         /**< 1 */
    uint8_t m_priority;     /**< 2 */
    uint8_t m_weight;       /**< 3 */
    uint16_t flags;         /**< 4-5   Unused(13) L p R, network order */
    uint16_t loc_afi;       /**< 6-7   network order */
    struct in_addr locator; /**< 8-11 */
} Locator;

#define LISP_REC_ACT_SHIFT 13     /**< in act_a_reserved (host order) */
#define LISP_REC_F_A       0x1000 /**< authoritative bit */
#define LISP_REC_VERSION_MASK                                                  \
    0x0FFF /**< 12-bit map-version in Record::version */

/** @brief EID record with an IPv4 EID and one IPv4 locator, 28 bytes; further locators follow when loc_count > 1. */
typedef struct click_lisp_record /* IPv4 EID record + one IPv4 locator, 28 bytes */
{
    uint32_t rec_ttl;        /**< 0-3   minutes, network order */
    uint8_t loc_count;       /**< 4 */
    uint8_t mask_len;        /**< 5 */
    uint16_t act_a_reserved; /**< 6-7   ACT(3) A(1) Reserved(12), network order */
    uint16_t version; /**< 8-9   Rsvd(4) Map-Version(12), network order */
    uint16_t eid_afi; /**< 10-11 network order */
    struct in_addr eid_prefix; /**< 12-15 */
    Locator loc;               /**< 16-27 */
} Record;

/*  Map-Request, RFC 9301 section 5.2.  This struct covers the form used by
    these elements: IPv4 Source-EID, one IPv4 ITR-RLOC, one IPv4 record.
    32 bytes, all integers in network byte order.

        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |Type=1 |A|M|P|S|p|s|R|R|  Rsvd   |L|D|   IRC   | Record Count  |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         Nonce . . .                           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         . . . Nonce                           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |         Source-EID-AFI        |   Source EID Address  ...     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |         ITR-RLOC-AFI 1        |    ITR-RLOC Address 1  ...    |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     / |   Reserved    | EID mask-len  |        EID-Prefix-AFI         |
   Rec +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     \ |                       EID-Prefix  ...                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

#define LISP_MREQ_F_A       (1U << 27) /**< authoritative */
#define LISP_MREQ_F_M       (1U << 26) /**< map-data present */
#define LISP_MREQ_F_P       (1U << 25) /**< probe */
#define LISP_MREQ_F_S       (1U << 24) /**< solicit-map-request (SMR) */
#define LISP_MREQ_F_PITR    (1U << 23) /**< p: sent by a PITR */
#define LISP_MREQ_F_SMR_INV (1U << 22) /**< s: SMR-invoked */
#define LISP_MREQ_IRC_SHIFT 8 /**< position of IRC (number of ITR-RLOCs - 1) */
#define LISP_MREQ_IRC_MASK  0x1F /**< IRC mask */

/** @brief Map-Request with an IPv4 Source-EID, one IPv4 ITR-RLOC and one IPv4 record, 32 bytes (RFC 9301 section 5.2). */
struct click_lisp_map_request {
    uint32_t type_flags;      /**< 0-3   network order */
    uint32_t nonce1;          /**< 4-7   opaque */
    uint32_t nonce2;          /**< 8-11  opaque */
    uint16_t source_eid_afi;  /**< 12-13 network order */
    uint16_t source_eid[2];   /**< 14-17 IPv4 address (not 4-byte aligned) */
    uint16_t itr_rloc_afi;    /**< 18-19 network order */
    struct in_addr itr_rloc;  /**< 20-23 */
    uint8_t rec_reserved;     /**< 24 */
    uint8_t rec_eid_mask_len; /**< 25 */
    uint16_t rec_eid_afi;     /**< 26-27 network order */
    struct in_addr rec_eid_prefix; /**< 28-31 */
};

/*  Map-Reply header, RFC 9301 section 5.4.  12 bytes, followed by
    Record Count records (see Record above).

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |Type=2 |P|E|S|         Reserved                |  Record Count |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       Nonce . . .                             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |                       . . . Nonce                             |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

#define LISP_MREP_F_P (1U << 27) /**< probe */
#define LISP_MREP_F_E (1U << 26) /**< echo-nonce */
#define LISP_MREP_F_S (1U << 25) /**< security */

/** @brief Map-Reply header, 12 bytes, followed by the records (RFC 9301 section 5.4). */
struct click_lisp_map_reply {
    uint32_t type_flags; /**< 0-3  network order */
    uint32_t nonce1;     /**< 4-7  opaque, copied from the Map-Request */
    uint32_t nonce2;     /**< 8-11 */
};

/*  Map-Register header, RFC 9301 section 5.6.  16 bytes, followed by
    auth_data_len bytes of authentication data, then Record Count records.

      0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |Type=3 |P|S|I|        Reserved       |E|T|a|R|M| Record Count  |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                         Nonce . . .                           |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |                         . . . Nonce                           |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |            Key ID             |  Authentication Data Length   |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     ~                     Authentication Data                       ~
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

#define LISP_MREG_F_PROXY       (1U << 27) /**< P: proxy Map-Reply */
#define LISP_MREG_F_WANT_NOTIFY (1U << 8)  /**< M: want Map-Notify */

/** @brief Map-Register header, 16 bytes, followed by authentication data and records (RFC 9301 section 5.6). */
struct click_lisp_map_register {
    uint32_t type_flags;    /**< 0-3   network order */
    uint32_t nonce1;        /**< 4-7   opaque */
    uint32_t nonce2;        /**< 8-11 */
    uint16_t key_id;        /**< 12-13 network order */
    uint16_t auth_data_len; /**< 14-15 network order */
    /* 16-   auth_data_len bytes of authentication data */
};

/* Key IDs (RFC 9301 section 5.6) */
#define LISP_KEY_ID_NONE             0 /**< Key ID: no authentication */
#define LISP_KEY_ID_HMAC_SHA_256_128 2 /**< Key ID: HMAC-SHA-256-128 */
#define LISP_AUTH_LEN_HMAC_SHA_256_128                                         \
    16 /**< authentication data length for HMAC-SHA-256-128 */

/*  Map-Notify header, RFC 9301 section 5.7.  Same layout as Map-Register
    (16 bytes, then authentication data, then records), Type = 4.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |Type=4 |I|R|          Reserved                 | Record Count  |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         Nonce . . .                           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         . . . Nonce                           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |            Key ID             |  Authentication Data Length   |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       ~                     Authentication Data                       ~
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

/** @brief Map-Notify header, same layout as click_lisp_map_register (RFC 9301 section 5.7). */
struct click_lisp_map_notify {
    uint32_t type_flags;    /**< 0-3   network order */
    uint32_t nonce1;        /**< 4-7   opaque */
    uint32_t nonce2;        /**< 8-11 */
    uint16_t key_id;        /**< 12-13 network order */
    uint16_t auth_data_len; /**< 14-15 network order */
    /* 16-   auth_data_len bytes of authentication data */
};

/*  Map-Migrate: FMCC extension (not in the LISP RFCs), sent as an RFC 8113
    shared extension message (Type 15, Sub-type LISP_FMCC_MIGRATE_SUBTYPE).
    16-byte header, auth_data_len bytes of authentication data (HMAC-SHA-256-128
    as for Map-Register, or none), then a 16-byte body.  Integers are in
    network byte order.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |Type=15|       Sub-type        |   Reserved    | Record Count  |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                          Nonce . . .                          |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                       . . . Nonce                             |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |            Key ID             |  Authentication Data Length   |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       ~                     Authentication Data                       ~
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |         EID mask-len          |        EID-Prefix-AFI         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                 EID-Prefix (the migrating VM)                 |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |         Unused Flags          |           Loc-AFI             |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |          Locator (RLOC of the destination data center)        |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

/** @brief Map-Migrate header (FMCC extension, RFC 8113 type 15), followed by authentication data and a click_lisp_map_migrate_body. */
struct click_lisp_map_migrate {
    uint32_t type_flags;    /**< 0-3   network order */
    uint32_t nonce1;        /**< 4-7   opaque */
    uint32_t nonce2;        /**< 8-11 */
    uint16_t key_id;        /**< 12-13 network order */
    uint16_t auth_data_len; /**< 14-15 network order */
    /* 16-   auth_data_len bytes of authentication data, then click_lisp_map_migrate_body */
};

/** @brief Body of a Map-Migrate: the VM and its new data center, 16 bytes. */
struct click_lisp_map_migrate_body {
    uint16_t mask;           /**< 0-1   EID mask length, network order */
    uint16_t eid_afi;        /**< 2-3   network order */
    struct in_addr vm_addr;  /**< 4-7   EID-Prefix (the migrating VM) */
    uint16_t unused;         /**< 8-9 */
    uint16_t loc_afi;        /**< 10-11 network order */
    struct in_addr ddc_addr; /**< 12-15 RLOC of the new data center */
};

/*  Encapsulated Control Message, RFC 9301 section 5.8.  4 bytes, followed by
    an inner IP header, a UDP header (destination port 4342) and the
    control message (here, a Map-Request from an ITR to a Map-Resolver).

     0                   1                   2                   3
     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    |Type=8 |S|D|E|M|            Reserved                           |
    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ */

/** @brief Encapsulated Control Message header, 4 bytes (RFC 9301 section 5.8). */
struct click_lisp_ecm {
    uint32_t type_flags; /**< 0-3   network order */
};

#endif /* CLICKNET_LISP_H */

/*
 * lispclassifier.{cc,hh} -- elements classify LISP data and control packets for an
 * xTR, a Map-Server and the FMCC controller
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

/** @file lispclassifier.hh
 * @brief Elements classify LISP data and control packets for an xTR, a Map-Server and the FMCC controller. */

#ifndef CLICK_LISPCLASSIFIER_HH
#define CLICK_LISPCLASSIFIER_HH
#include <click/element.hh>
#include <click/atomic.hh>
#include <click/ipaddress.hh>
CLICK_DECLS

/** @brief Parsing shared by LISPClassifierxTR,
 * LISPClassifierMS and LISPClassifierFMCC.  Each element maps the message
 * kind to its own outputs (unchanged from the original elements).
 *
 * Keyword VERBOSE (bool, default false) prints one line per packet.
 */
class LISPClassifierBase : public Element {
  public:
    LISPClassifierBase();

    const char *processing() const { return PUSH; } ///< Click Element interface
    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    void push(int, Packet *);

    /** @brief What a packet is. */
    enum Kind {
	K_NOT_LISP,  // not UDP, too short for UDP, or not a LISP port
	K_DATA,      // UDP port 4341
	K_TOO_SHORT, // control port but no room for the first word
	K_RESERVED,  // reserved bits set
	K_MAP_REQUEST,
	K_SMR,
	K_MAP_REPLY,
	K_MAP_REGISTER,
	K_MAP_NOTIFY,
	K_MAP_MIGRATE, // type 15, FMCC sub-type
	K_ECM,         // type 8, Encapsulated Control Message
	K_UNKNOWN      // other control type (incl. other type-15 sub-types)
    };

  protected:
    // Returns the output for a packet; is_data/is_ctrl give the ports seen.
    /** @brief Output for a packet; is_data / is_ctrl: it uses port 4341 / 4342. */
    virtual int choose(bool is_data, bool is_ctrl, Kind kind) = 0;
    /** @brief Handlers shared by the three classifiers. */
    void add_common_handlers();

    atomic_uint32_t _request_received;  ///< Map-Requests seen
    atomic_uint32_t _register_received; ///< Map-Registers seen
    uint32_t _value_type;               ///< type of the last control message
    uint32_t _mapnotify;                ///< M bit of the last control message
    uint32_t _reserv; ///< reserved bits of the last control message
    uint32_t _info;   ///< first word of the last control message, raw
    uint32_t _smr;    ///< S bit of the last control message
    uint32_t _udp_sp; ///< UDP source port of the last packet
    uint32_t _udp_dp; ///< UDP destination port of the last packet
    bool _verbose;    ///< VERBOSE: one line per packet

  private:
    Kind control_kind(const uint32_t *msg);
};

/*
=c

LISPClassifierxTR([VERBOSE])

=s lisp

classifies LISP data and control packets on an xTR

=d

Input: IP packets.  Outputs: 0 plain IP (to encapsulate); 1 Map-Request;
2 Map-Reply; 3 Map-Notify; 4 Map-Migrate (FMCC); 5 LISP data (to
decapsulate); 6 error or unknown control message; 7 Solicit-Map-Request.
Never modifies packets.  VERBOSE (boolean) prints one line per packet.

=h requests read-only

Map-Requests seen.

=h registers read-only

Map-Registers seen.

=h type, want_notify, reserved, last_word, sport, dport read-only

Fields of the last packet.

=a LISPClassifierMS, LISPClassifierFMCC
*/
/** @brief Classifies LISP data and control packets on an xTR (Click element LISPClassifierxTR).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPClassifierxTR(7click) man page. */
class LISPClassifierxTR : public LISPClassifierBase {
  public:
    /// Click Element interface
    const char *class_name() const { return "LISPClassifierxTR"; }
    /// Click Element interface
    const char *port_count() const { return "1/0-9"; }
    /// Click Element interface
    void add_handlers();

  protected:
    int choose(bool, bool,
	Kind); ///< Click output for a packet kind (see the element documentation)
};

/*
=c

LISPClassifierMS([VERBOSE])

=s lisp

classifies LISP control packets on a Map-Server / Map-Resolver

=d

Outputs: 0 other or error; 1 Map-Request (plain or in an Encapsulated
Control Message); 2 Map-Register; 3 Map-Migrate.  VERBOSE as for
LISPClassifierxTR.

=h requests, registers, type, want_notify, reserved, last_word, smr read-only

Counters and fields of the last packet.

=a LISPClassifierxTR, LISPMapServer, LISPMapResolver
*/
/** @brief Classifies LISP control packets on a Map-Server / Map-Resolver (Click element LISPClassifierMS).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPClassifierMS(7click) man page. */
class LISPClassifierMS : public LISPClassifierBase {
  public:
    /// Click Element interface
    const char *class_name() const { return "LISPClassifierMS"; }
    /// Click Element interface
    const char *port_count() const { return "1/1-4"; }
    /// Click Element interface
    void add_handlers();

  protected:
    int choose(bool, bool,
	Kind); ///< Click output for a packet kind (see the element documentation)
};

/*
=c

LISPClassifierFMCC([VERBOSE])

=s lisp

classifies LISP packets on the FMCC controller

=d

Outputs: 0 LISP data; 1 Map-Register; 2 Map-Notify; 3 other.

=h requests, registers, type, want_notify, reserved, last_word read-only

Counters and fields of the last packet.

=a LISPClassifierxTR, FMCCManager, FMCCMigrator
*/
/** @brief Classifies LISP packets on the FMCC controller (Click element LISPClassifierFMCC).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPClassifierFMCC(7click) man page. */
class LISPClassifierFMCC : public LISPClassifierBase {
  public:
    /// Click Element interface
    const char *class_name() const { return "LISPClassifierFMCC"; }
    /// Click Element interface
    const char *port_count() const { return "1/0-4"; }
    /// Click Element interface
    void add_handlers();

  protected:
    int choose(bool, bool,
	Kind); ///< Click output for a packet kind (see the element documentation)
};

CLICK_ENDDECLS
#endif

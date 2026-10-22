/*
 * lispdecap.{cc,hh} -- element removes the LISP/UDP/IP encapsulation (RFC 9300)
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

/** @file lispdecap.hh
 * @brief Element removes the LISP/UDP/IP encapsulation (RFC 9300). */

#ifndef CLICK_LISPDECAP_HH
#define CLICK_LISPDECAP_HH

#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/atomic.hh>
#include <click/glue.hh>

CLICK_DECLS

/*
=c

LISPDecap([SRC])

=s lisp

removes the LISP/UDP/IP encapsulation (RFC 9300)

=d

Checks that the packet is a LISP data packet (UDP to port 4341, long
enough), removes the outer IP, UDP and LISP headers, and sets the IP header
annotation to the inner header.  Other packets go to output 1 if it is
connected, otherwise they are dropped.  SRC is accepted for compatibility
and ignored.

=h count read-only

Packets decapsulated.

=h drops read-only

Packets rejected.

=a LISPEncap, LISPClassifierxTR
*/
/** @brief Removes the LISP/UDP/IP encapsulation (RFC 9300) (Click element LISPDecap).
 *
 * Arguments, ports and handlers: see the element documentation comment
 * above, also available as the LISPDecap(7click) man page. */
class LISPDecap : public Element {
  public:
    LISPDecap();
    ~LISPDecap();

    /// Click Element interface
    const char *class_name() const { return "LISPDecap"; }
    /// Click Element interface
    const char *port_count() const { return "1/1-2"; }
    /// Click Element interface
    int configure(Vector<String> &, ErrorHandler *);
    /// Click Element interface
    void push(int, Packet *);
    /// Click Element interface
    void add_handlers();

  private:
    IPAddress _my_addr;
    atomic_uint32_t _packet_count;
    atomic_uint32_t _drops;
};

CLICK_ENDDECLS
#endif

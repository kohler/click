// -*- related-file-name: "../../../elements/standard/alignmentinfo.cc" -*-
#ifndef CLICK_ALIGNMENTINFO_HH
#define CLICK_ALIGNMENTINFO_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * AlignmentInfo(ELEMENT [MODULUS OFFSET ...], ...)
 * =s information
 * specifies alignment information
 * =io
 * None
 * =d
 * Provides information about the packet alignment specified elements can
 * expect. Each configuration argument has the form
 * `ELEMENT [MODULUS0 OFFSET0 MODULUS1 OFFSET1 ...]',
 * where there are zero or more MODULUS-OFFSET pairs.
 * All packets arriving at ELEMENT's
 * I<n>th input will start `OFFSETI<n>' bytes
 * off from a `MODULUSI<n>'-byte boundary.
 * =n
 * This element is inserted automatically by click-align(1).
 * =a Align, click-align(1)
 */

class AlignmentInfo : public Element { public:

    AlignmentInfo();

    const char *class_name() const	{ return "AlignmentInfo"; }
    int configure_phase() const		{ return CONFIGURE_PHASE_INFO; }
    int configure(Vector<String> &, ErrorHandler *);

    bool query1(const Element *e, int port, int &chunk, int &offset) const;
    static bool query(const Element *e, int port, int &chunk, int &offset);

  private:

    Vector<int> _elem_offset;
    Vector<int> _elem_icount;
    Vector<int> _chunks;
    Vector<int> _offsets;

};

CLICK_ENDDECLS
#endif

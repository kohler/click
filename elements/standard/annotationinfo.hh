#ifndef CLICK_ANNOTATIONINFO_HH
#define CLICK_ANNOTATIONINFO_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

AnnotationInfo(NAME OFFSET SIZE, ... [I<keyword> CHECK_OVERLAP ANNO...])

=s information

Define names for packet annotations.

=d

Defines new mnemonic names for packet annotations.  Each argument has the form
"NAME OFFSET [SIZE]", which defines NAME as an annotation starting at byte
offset OFFSET and with byte size SIZE.

The CHECK_OVERLAP argument lets you check whether a set of annotations
overlap.  CHECK_OVERLAP is followed by a list of annotation names; if any of
the named annotations overlap, AnnotationInfo reports an error and fails to
initialize.

Annotation names defined by default, such as PAINT, may not be redefined.
*/

class AnnotationInfo : public Element { public:

    AnnotationInfo();

    const char *class_name() const	{ return "AnnotationInfo"; }

    int configure_phase() const		{ return CONFIGURE_PHASE_FIRST; }
    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif

#ifndef CLICK_CLASSIFIER_HH
#define CLICK_CLASSIFIER_HH
#include <click/element.hh>
#include "classification.hh"
CLICK_DECLS

/*
 * =c
 * Classifier(pattern1, ..., patternN)
 * =s classification
 * classifies packets by contents
 * =d
 * Classifies packets. The Classifier has N outputs, each associated with the
 * corresponding pattern from the configuration string.
 * A pattern is a set of clauses, where each clause is either "offset/value"
 * or "offset/value%mask". A pattern matches if the packet has the indicated
 * value at each offset.
 *
 * The clauses in each pattern are separated
 * by spaces. A clause consists of the offset, "/", the value, and (optionally)
 * "%" and a mask. The offset is in decimal. The value and mask are in hex.
 * The length of the value is implied by the number of hex digits, which must
 * be even. "?" is also allowed as a "hex digit"; it means "don't care about
 * the value of this nibble".
 *
 * If present, the mask must have the same number of hex digits as the value.
 * The matcher will only check bits that are 1 in the mask.
 *
 * A clause may be preceded by "!", in which case the clause must NOT match
 * the packet.
 *
 * As a special case, a pattern consisting of "-" matches every packet.
 *
 * The patterns are scanned in order, and the packet is sent to the output
 * corresponding to the first matching pattern. Thus more specific patterns
 * should come before less specific ones. You will get a warning if no packet
 * could ever match a pattern. Usually, this is because an earlier pattern is
 * more general, or because your pattern is contradictory (`12/0806 12/0800').
 *
 * =n
 *
 * The IPClassifier and IPFilter elements have a friendlier syntax if you are
 * classifying IP packets.
 *
 * =e
 * For example,
 *
 *   Classifier(12/0806 20/0001,
 *              12/0806 20/0002,
 *              12/0800,
 *              -);
 *
 * creates an element with four outputs intended to process
 * Ethernet packets.
 * ARP requests are sent to output 0, ARP replies are sent to
 * output 1, IP packets to output 2, and all others to output 3.
 *
 * =h program read-only
 * Returns a human-readable definition of the program the Classifier element
 * is using to classify packets. At each step in the program, four bytes
 * of packet data are ANDed with a mask and compared against four bytes of
 * classifier pattern.
 *
 * The Classifier patterns above compile into the following program:
 *
 *   0  12/08060000%ffff0000  yes->step 1  no->step 3
 *   1  20/00010000%ffff0000  yes->[0]  no->step 2
 *   2  20/00020000%ffff0000  yes->[1]  no->[3]
 *   3  12/08000000%ffff0000  yes->[2]  no->[3]
 *   safe length 22
 *   alignment offset 0
 *
 * =a IPClassifier, IPFilter */

class Classifier : public Element { public:

    Classifier() CLICK_COLD;

    const char *class_name() const		{ return "Classifier"; }
    const char *port_count() const		{ return "1/-"; }
    const char *processing() const		{ return PUSH; }
    // this element needs AlignmentInfo, so supply the "A" flag
    const char *flags() const			{ return "A"; }
    bool can_live_reconfigure() const		{ return true; }

    int configure(Vector<String> &conf, ErrorHandler *errh) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void push(int port, Packet *);

    Classification::Wordwise::Program empty_program(ErrorHandler *errh) const;
    static void parse_program(Classification::Wordwise::Program &prog,
			      Vector<String> &conf, ErrorHandler *errh);

  protected:

    Classification::Wordwise::Program _prog;

    static String program_string(Element *, void *);

};

CLICK_ENDDECLS
#endif

/*
 * classifier.{cc,hh} -- element is a generic classifier
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2008 Regents of the University of California
 * Copyright (c) 2010 Meraki, Inc.
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

#include <click/config.h>
#include "classifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#if !HAVE_INDIFFERENT_ALIGNMENT
#include <click/router.hh>
#endif
#include <click/standard/alignmentinfo.hh>
CLICK_DECLS

Classifier::Classifier()
{
}

Classification::Wordwise::Program
Classifier::empty_program(ErrorHandler *errh) const
{
    // set align offset
    int c, o;
    if (AlignmentInfo::query(this, 0, c, o) && c >= 4)
	// want 'data - _align_offset' aligned at 4/(o%4)
	o = (4 - (o % 4)) % 4;
    else {
#if !HAVE_INDIFFERENT_ALIGNMENT
	if (errh) {
	    errh->warning("alignment unknown, but machine is sensitive to alignment");
	    void *&x = router()->force_attachment("Classifier alignment warning");
	    if (!x) {
		x = (void *) this;
		errh->message("(%s must be told how its input packets are aligned in memory.\n"
			      "Fix this error either by passing your configuration through click-align,\n"
			      "or by providing explicit AlignmentInfo. I am assuming the equivalent\n"
			      "of %<AlignmentInfo(%s 4 0)%>.)", class_name(), name().c_str());
	    }
	}
#else
	(void) errh;
#endif
	o = 0;
    }
    return Classification::Wordwise::Program(o);
}

static void
update_value_mask(int c, int shift, int &value, int &mask)
{
    int v = 0, m = 0xF;
    if (c == '?')
	v = m = 0;
    else if (c >= '0' && c <= '9')
	v = c - '0';
    else if (c >= 'A' && c <= 'F')
	v = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f')
	v = c - 'a' + 10;
    value |= (v << shift);
    mask |= (m << shift);
}

void
Classifier::parse_program(Classification::Wordwise::Program &prog,
			  Vector<String> &conf, ErrorHandler *errh)
{
    Vector<int> tree = prog.init_subtree();
    prog.start_subtree(tree);

    for (int slot = 0; slot < conf.size(); slot++) {
	int i = 0;
	int len = conf[slot].length();
	const char *s = conf[slot].data();
	bool empty = true;

	prog.start_subtree(tree);

	if (s[0] == '-' && len == 1)
	    // slot accepting everything
	    i = 1;

	while (i < len) {

	    while (i < len && isspace((unsigned char) s[i]))
		i++;
	    if (i >= len)
		break;

	    // negated?
	    bool negated = false;
	    if (s[i] == '!') {
		negated = true;
		i++;
		while (i < len && isspace((unsigned char) s[i]))
		    i++;
	    }

	    if (i >= len || !isdigit((unsigned char) s[i])) {
		errh->error("pattern %d: expected a digit", slot);
		break;
	    }

	    // read offset
	    int offset = 0;
	    while (i < len && isdigit((unsigned char) s[i])) {
		offset *= 10;
		offset += s[i] - '0';
		i++;
	    }

	    if (i >= len || s[i] != '/') {
		errh->error("pattern %d: expected %</%>", slot);
		break;
	    }
	    i++;

	    // scan past value
	    int value_pos = i;
	    while (i < len && (isxdigit((unsigned char) s[i]) || s[i] == '?'))
		i++;
	    int value_end = i;

	    // scan past mask
	    int mask_pos = -1;
	    int mask_end = -1;
	    if (i < len && s[i] == '%') {
		i++;
		mask_pos = i;
		while (i < len && (isxdigit((unsigned char) s[i]) || s[i] == '?'))
		    i++;
		mask_end = i;
	    }

	    // check lengths
	    if (value_end - value_pos < 2) {
		errh->error("pattern %d: value has less than 2 hex digits", slot);
		value_end = value_pos;
		mask_end = mask_pos;
	    }
	    if ((value_end - value_pos) % 2 != 0) {
		errh->error("pattern %d: value has odd number of hex digits", slot);
		value_end--;
		mask_end--;
	    }
	    if (mask_pos >= 0 && (mask_end - mask_pos) != (value_end - value_pos)) {
		bool too_many = (mask_end - mask_pos) > (value_end - value_pos);
		errh->error("pattern %d: mask has too %s hex digits", slot,
			    (too_many ? "many" : "few"));
		if (too_many)
		    mask_end = mask_pos + value_end - value_pos;
		else
		    value_end = value_pos + mask_end - mask_pos;
	    }

	    // add values to exprs
	    prog.start_subtree(tree);

	    bool first = true;
	    offset += prog.align_offset();
	    while (value_pos < value_end) {
		int v = 0, m = 0;
		update_value_mask(s[value_pos], 4, v, m);
		update_value_mask(s[value_pos+1], 0, v, m);
		value_pos += 2;
		if (mask_pos >= 0) {
		    int mv = 0, mm = 0;
		    update_value_mask(s[mask_pos], 4, mv, mm);
		    update_value_mask(s[mask_pos+1], 0, mv, mm);
		    mask_pos += 2;
		    m = m & mv & mm;
		}
		if (first || offset % 4 == 0) {
		    prog.add_insn(tree, (offset / 4) * 4, 0, 0);
		    first = empty = false;
		}
		prog.back().mask.c[offset % 4] = m;
		prog.back().value.c[offset % 4] = v & m;
		offset++;
	    }

	    // combine with "and"
	    prog.finish_subtree(tree, Classification::c_and);

	    if (negated)
		prog.negate_subtree(tree);
	}

	// add fake expr if required
	if (empty)
	    prog.add_insn(tree, 0, 0, 0);

	prog.finish_subtree(tree, Classification::c_and,
                            -slot, Classification::j_failure);
    }

    prog.finish_subtree(tree, Classification::c_or, Classification::j_never, Classification::j_never);

    // click_chatter("%s", prog.unparse().c_str());
    prog.optimize(0, 0, Classification::offset_max);
    // click_chatter("%s", prog.unparse().c_str());
}

int
Classifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() != noutputs())
	return errh->error("need %d arguments, one per output port", noutputs());

    Classification::Wordwise::Program prog = empty_program(errh);
    parse_program(prog, conf, errh);

    if (!errh->nerrors()) {
	prog.warn_unused_outputs(noutputs(), errh);
	_prog = prog;
	return 0;
    } else
	return -1;
}

String
Classifier::program_string(Element *element, void *)
{
    Classifier *c = static_cast<Classifier *>(element);
    return c->_prog.unparse();
}

void
Classifier::add_handlers()
{
    add_read_handler("program", Classifier::program_string, 0, Handler::CALM);
}

void
Classifier::push(int, Packet *p)
{
    checked_output_push(_prog.match(p), p);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(AlignmentInfo Classification)
EXPORT_ELEMENT(Classifier)
ELEMENT_MT_SAFE(Classifier)

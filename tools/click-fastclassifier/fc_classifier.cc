/*
 * fc_classifier.cc -- click-fastclassifier functions for Classifier
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2010 Intel Corporation
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

#include <click/straccum.hh>
#include "click-fastclassifier.hh"

static void
match_body(const Classifier_Program &c, StringAccum &source)
{
    source << "  if (p->length() < " << c.safe_length << ")\n    return ";
    if (c.unsafe_length_output_everything >= 0)
	source << c.unsafe_length_output_everything << ";\n";
    else
	source << "length_checked_match(p);\n";

    int align_off = c.align_offset;
    source << "  const unsigned *data = (const unsigned *)(p->data() - "
	   << align_off << ");\n";

    for (int i = 0; i < c.program.size(); i++) {
	const Classifier_Insn &in = c.program[i];
	StringAccum data_sa;
	data_sa << "data[" << ((in.offset + align_off) / 4) << "]";
	in.write_state(i, false, false,
		       data_sa.take_string(), "step_", source);
    }
}

static void
more(const Classifier_Program &c, const String &type_name,
     StringAccum &header, StringAccum &source)
{
    if (c.unsafe_length_output_everything >= 0)
	return;

    header << "  int length_checked_match(const Packet *p) const;\n";

    source << "int\n" << type_name << "::length_checked_match(const Packet *p) const\n{\n";
    int align_off = c.align_offset;
    source << "  const unsigned *data = (const unsigned *)(p->data() - "
	   << align_off << ");\n  int l = p->length();\n";

    for (int i = 0; i < c.program.size(); i++) {
	const Classifier_Insn &in = c.program[i];
	int want_l = in.required_length();
	StringAccum data_sa;
	data_sa << "data[" << ((in.offset + align_off) / 4) << "]";
	in.write_state(i, true, want_l >= c.safe_length,
		       data_sa.take_string(), "lstep_", source);
    }

    source << "}\n";
}

extern "C" void
add_fast_classifiers_1()
{
    add_classifier_type("Classifier", match_body, more);
}

/*
 * fc_ipclassifier.cc -- click-fastclassifier functions for IPFilter and
 * IPClassifier
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
 * legally binding. */

#include <click/config.h>

#include <click/straccum.hh>
#include "click-fastclassifier.hh"

// magic constants imported from Click itself
#define IPCLASSIFIER_TRANSP_FAKE_OFFSET 64

static void
match_body(const Classifier_Program &c, StringAccum &source)
{
    source << "  int l = p->network_length();\n\
  if (l > (int) p->network_header_length())\n\
    l += " << IPCLASSIFIER_TRANSP_FAKE_OFFSET << " - p->network_header_length();\n\
  if (l < " << c.safe_length << ")\n    return ";
    if (c.unsafe_length_output_everything >= 0)
	source << c.unsafe_length_output_everything << ";\n";
    else
	source << "length_checked_match(p, l);\n";
    source << "  const uint32_t *neth_data = reinterpret_cast<const uint32_t *>(p->network_header());\n\
  const uint32_t *transph_data = reinterpret_cast<const uint32_t *>(p->transport_header());\n";

    for (int i = 0; i < c.program.size(); i++) {
	const Classifier_Insn &in = c.program[i];
	StringAccum data_sa;
	if (in.offset >= IPCLASSIFIER_TRANSP_FAKE_OFFSET)
	    data_sa << "transph_data[" << ((in.offset - IPCLASSIFIER_TRANSP_FAKE_OFFSET) / 4) << "]";
	else
	    data_sa << "neth_data[" << (in.offset / 4) << "]";
	in.write_state(i, false, false, data_sa.take_string(), "step_", source);
    }
}

static void
more(const Classifier_Program &c, const String &type_name,
     StringAccum &header, StringAccum &source)
{
    if (c.unsafe_length_output_everything >= 0)
	return;

    header << "  int length_checked_match(const Packet *p, int l) const;\n";
    source << "int\n" << type_name << "::length_checked_match(const Packet *p, int l) const\n{\n"
	   << "  const uint32_t *neth_data = reinterpret_cast<const uint32_t *>(p->network_header());\n\
  const uint32_t *transph_data = reinterpret_cast<const uint32_t *>(p->transport_header());\n";

    for (int i = 0; i < c.program.size(); i++) {
	const Classifier_Insn &in = c.program[i];
	StringAccum data_sa;
	if (in.offset >= IPCLASSIFIER_TRANSP_FAKE_OFFSET)
	    data_sa << "transph_data[" << ((in.offset - IPCLASSIFIER_TRANSP_FAKE_OFFSET) / 4) << "]";
	else
	    data_sa << "neth_data[" << (in.offset / 4) << "]";
	int want_l = in.required_length();
	in.write_state(i, true, want_l >= c.safe_length,
		       data_sa.take_string(), "lstep_", source);
    }

    source << "}\n";
}

extern "C" void
add_fast_classifiers_2()
{
    add_classifier_type("IPClassifier", match_body, more);
    add_classifier_type("IPFilter", match_body, more);
}

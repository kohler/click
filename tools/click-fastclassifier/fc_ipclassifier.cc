/*
 * fc_ipclassifier.cc -- click-fastclassifier functions for IPFilter and
 * IPClassifier
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2010 Intel Corporation
 * Copyright (c) 2011 Regents of the University of California
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
#define IPCLASSIFIER_OFFSET_MAC 0
#define IPCLASSIFIER_OFFSET_NET 256
#define IPCLASSIFIER_OFFSET_TRANSP 512

enum {
    used_mach = 1, used_neth = 2, used_transph = 4
};

static void
get_data(StringAccum &data_sa, int &used, int offset)
{
    if (offset >= IPCLASSIFIER_OFFSET_TRANSP) {
	data_sa << "transph_data[" << ((offset - IPCLASSIFIER_OFFSET_TRANSP) / 4) << "]";
	used |= used_transph;
    } else if (offset >= IPCLASSIFIER_OFFSET_NET) {
	data_sa << "neth_data[" << ((offset - IPCLASSIFIER_OFFSET_NET) / 4) << "]";
	used |= used_neth;
    } else {
	data_sa << "mach_data[" << ((offset - IPCLASSIFIER_OFFSET_MAC) / 4) << "]";
	used |= used_mach;
    }
}

static void
finish(StringAccum &source, const StringAccum &program, int used)
{
    if (used & used_mach)
	source << "  const uint32_t *mach_data = reinterpret_cast<const uint32_t *>(p->mac_header() - 2);\n";
    if (used & used_neth)
	source << "  const uint32_t *neth_data = reinterpret_cast<const uint32_t *>(p->network_header());\n";
    if (used & used_transph)
	source << "  const uint32_t *transph_data = reinterpret_cast<const uint32_t *>(p->transport_header());\n";
    source << program;
}

static void
match_body(const Classifier_Program &c, StringAccum &source)
{
    source << "  int l = p->network_length();\n\
  if (l > (int) p->network_header_length())\n\
    l += " << IPCLASSIFIER_OFFSET_TRANSP << " - p->network_header_length();\n\
  else\n\
    l += " << IPCLASSIFIER_OFFSET_NET << ";\n\
  if (l < " << c.safe_length << ")\n    return ";
    if (c.unsafe_length_output_everything >= 0)
	source << c.unsafe_length_output_everything << ";\n";
    else
	source << "length_checked_match(p, l);\n";

    int used = 0;
    StringAccum program;
    for (int i = 0; i < c.program.size(); i++) {
	const Classifier_Insn &in = c.program[i];
	StringAccum data_sa;
	get_data(data_sa, used, in.offset);
	in.write_state(i, false, false,
		       data_sa.take_string(), "step_", program);
    }

    finish(source, program, used);
}

static void
more(const Classifier_Program &c, const String &type_name,
     StringAccum &header, StringAccum &source)
{
    if (c.unsafe_length_output_everything >= 0)
	return;

    header << "  int length_checked_match(const Packet *p, int l) const;\n";
    source << "int\n" << type_name << "::length_checked_match(const Packet *p, int l) const\n{\n";

    int used = 0;
    StringAccum program;
    for (int i = 0; i < c.program.size(); i++) {
	const Classifier_Insn &in = c.program[i];
	StringAccum data_sa;
	get_data(data_sa, used, in.offset);
	int want_l = in.required_length();
	in.write_state(i, want_l >= IPCLASSIFIER_OFFSET_NET,
		       want_l >= c.safe_length,
		       data_sa.take_string(), "lstep_", program);
    }

    finish(source, program, used);
    source << "}\n";
}

extern "C" void
add_fast_classifiers_2()
{
    add_classifier_type("IPClassifier", match_body, more);
    add_classifier_type("IPFilter", match_body, more);
}

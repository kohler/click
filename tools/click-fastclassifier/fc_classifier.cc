/*
 * fc_classifier.cc -- click-fastclassifier functions for Classifier
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

#include <click/config.h>

#include <click/straccum.hh>
#include "click-fastclassifier.hh"

static void
write_checked_body(const Classifier_Program &c, StringAccum &source)
{
  int align_off = c.align_offset;
  source << "  const unsigned *data = (const unsigned *)(p->data() - "
	 << align_off << ");\n  int l = p->length();\n";

  source << "  assert(l < " << c.safe_length << ");\n";

  for (int i = 0; i < c.program.size(); i++) {
    const Classifier_Insn &e = c.program[i];
    
    int want_l = e.offset + 4;
    if (!e.mask.c[3]) {
      want_l--;
      if (!e.mask.c[2]) {
	want_l--;
	if (!e.mask.c[1])
	  want_l--;
      }
    }
    
    bool switched = (e.yes == i + 1);
    int branch1 = (switched ? e.no : e.yes);
    int branch2 = (switched ? e.yes : e.no);
    
    source << " lstep_" << i << ":\n";
    
    int offset = (e.offset + align_off)/4;
    
    if (want_l >= c.safe_length) {
      branch2 = e.no;
      goto output_branch2;
    }

    if (switched)
      source << "  if (l < " << want_l << " || (data[" << offset << "] & "
	     << e.mask.u << "U) != " << e.value.u << "U)";
    else
      source << "  if (!(l < " << want_l << ") && (data[" << offset << "] & "
	     << e.mask.u << "U) == " << e.value.u << "U)";
    if (branch1 <= -c.noutputs)
      source << " {\n    p->kill();\n    return;\n  }\n";
    else if (branch1 <= 0)
      source << " {\n    output(" << -branch1 << ").push(p);\n    return;\n  }\n";
    else
      source << "\n    goto lstep_" << branch1 << ";\n";
    
   output_branch2:
    if (branch2 <= -c.noutputs)
      source << "  p->kill();\n  return;\n";
    else if (branch2 <= 0)
      source << "  output(" << -branch2 << ").push(p);\n  return;\n";
    else if (branch2 != i + 1)
      source << "  goto lstep_" << branch2 << ";\n";
  }
}

static void
write_unchecked_body(const Classifier_Program &c, StringAccum &source)
{
  int align_off = c.align_offset;
  source << "  const unsigned *data = (const unsigned *)(p->data() - "
	 << align_off << ");\n";

  for (int i = 0; i < c.program.size(); i++) {
    const Classifier_Insn &e = c.program[i];
    
    bool switched = (e.yes == i + 1);
    int branch1 = (switched ? e.no : e.yes);
    int branch2 = (switched ? e.yes : e.no);
    source << " step_" << i << ":\n";

    int offset = (e.offset + align_off)/4;
    
    if (switched)
      source << "  if ((data[" << offset << "] & " << e.mask.u
	     << "U) != " << e.value.u << "U)";
    else
      source << "  if ((data[" << offset << "] & " << e.mask.u
	     << "U) == " << e.value.u << "U)";
    if (branch1 <= -c.noutputs)
      source << " {\n    p->kill();\n    return;\n  }\n";
    else if (branch1 <= 0)
      source << " {\n    output(" << -branch1 << ").push(p);\n    return;\n  }\n";
    else
      source << "\n    goto step_" << branch1 << ";\n";
    if (branch2 <= -c.noutputs)
      source << "  p->kill();\n  return;\n";
    else if (branch2 <= 0)
      source << "  output(" << -branch2 << ").push(p);\n  return;\n";
    else if (branch2 != i + 1)
      source << "  goto step_" << branch2 << ";\n";
  }
}

static void
write_push_body(const Classifier_Program &c, StringAccum &sa)
{
  sa << "  if (p->length() < ";
  sa << c.safe_length;
  sa << ")\n\
    length_checked_push(p);\n\
  else\n\
    length_unchecked_push(p);\n";
}

extern "C" void
add_fast_classifiers_1()
{
  add_classifier_type("Classifier", 0,
		      write_checked_body,
		      write_unchecked_body,
		      write_push_body);
}

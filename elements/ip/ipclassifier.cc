/*
 * ipclassifier.{cc,hh} -- IP-packet classifier with tcpdumplike syntax
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include "ipclassifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>

IPClassifier::IPClassifier()
{
  // no MOD_INC_USE_COUNT; rely on Classifier
}

IPClassifier::~IPClassifier()
{
  // no MOD_DEC_USE_COUNT; rely on Classifier
}

IPClassifier *
IPClassifier::clone() const
{
  return new IPClassifier;
}

int
IPClassifier::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  set_noutputs(conf.size());

  // leverage IPFilter's parsing
  Vector<String> new_conf;
  for (int i = 0; i < conf.size(); i++)
    new_conf.push_back(String(i) + " " + conf[i]);
  return IPFilter::configure(new_conf, errh);
}

ELEMENT_REQUIRES(Classifier)
EXPORT_ELEMENT(IPClassifier)
ELEMENT_MT_SAFE(IPClassifier)

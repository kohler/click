/*
 * ipclassifier.{cc,hh} -- IP-packet classifier with tcpdumplike syntax
 * Eddie Kohler
 *
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
#include <click/package.hh>
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

/*
 * ipclassifier.{cc,hh} -- IP-packet classifier with tcpdumplike syntax
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipclassifier.hh"
#include "glue.hh"
#include "error.hh"
#include "confparse.hh"

IPClassifier::IPClassifier()
{
}

IPClassifier::~IPClassifier()
{
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

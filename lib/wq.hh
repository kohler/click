/*
 * wq.{hh} -- wait queues for elements
 * Benjie Chen
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */
#ifndef WQ_HH
#define WQ_HH

#ifdef __KERNEL__

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "element.hh"

extern "C" {
#include <linux/wait.h>
}

struct ElementWaitQueue 
{
  Element *element;
  struct wait_queue **element_wq;
  struct wait_queue thread_wq;
};

#endif

#endif


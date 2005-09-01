// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * tosocket.{cc,hh} -- element write data to socket
 * Mark Huang <mlhuang@cs.princeton.edu>
 *
 * Copyright (c) 2004  The Trustees of Princeton University (Trustees).
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
#include "tosocket.hh"

CLICK_DECLS

ToSocket::ToSocket()
{
  click_chatter("warning: ToSocket is deprecated, use Socket instead");
}

ToSocket::~ToSocket()
{
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Socket)
EXPORT_ELEMENT(ToSocket)

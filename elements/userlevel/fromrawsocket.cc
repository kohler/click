// -*- mode: c++; c-basic-offset: 2 -*-
/*
 * fromrawsocket.{cc,hh} -- element reads raw IPv4 socket data
 *
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
#include "fromrawsocket.hh"

CLICK_DECLS

FromRawSocket::FromRawSocket()
{
  click_chatter("warning: FromRawSocket is deprecated, use RawSocket instead");
}

FromRawSocket::~FromRawSocket()
{
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(RawSocket)
EXPORT_ELEMENT(FromRawSocket)

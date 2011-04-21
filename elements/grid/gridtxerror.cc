/*
 * gridtxerror.{cc,hh} -- log Grid packets that weren't sent successfully
 * Douglas S. J. De Couto
 *
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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
#include "gridtxerror.hh"
#include <click/error.hh>
#include <click/args.hh>
#include <click/packet_anno.hh>
#include "gridgenericlogger.hh"

CLICK_DECLS

GridTxError::GridTxError()
  : _log(0)
{
}

GridTxError::~GridTxError()
{
}

int
GridTxError::initialize(ErrorHandler *)
{
  return 0;
}

int
GridTxError::configure(Vector<String> &conf, ErrorHandler *errh)
{
    return Args(conf, this, errh)
	.read("LOG", reinterpret_cast<Element *&>(_log))
	.complete();
}

void
GridTxError::push(int, Packet *p)
{
  /* log the error */
  if (_log)
    _log->log_tx_err(p, SEND_ERR_ANNO(p), Timestamp::now());

  p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GridTxError);

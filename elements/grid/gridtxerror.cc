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
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#include "gridgenericlogger.hh"

CLICK_DECLS

GridTxError::GridTxError() 
  : Element(1, 0), _log(0)
{
  MOD_INC_USE_COUNT;
}

GridTxError::~GridTxError() 
{
  MOD_DEC_USE_COUNT;
}

int
GridTxError::initialize(ErrorHandler *)
{
  return 0;
}

int
GridTxError::configure(Vector<String> &conf, ErrorHandler *errh)
{
  int res = cp_va_parse(conf, this, errh,
			cpKeywords,
			"LOG", cpElement, "GridGenericLogger element", &_log,
			0);
  if (res < 0)
    return res;
  return 0;
}

void
GridTxError::push(int, Packet *p) 
{
  /* log the error */
  int err = SEND_ERR_ANNO(p);
  struct timeval tv;
  click_gettimeofday(&tv);

  if (_log)
    _log->log_tx_err(p, err, tv);

  p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(GridTxError);

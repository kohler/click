/*
 * DRR.{cc,hh} -- deficit round-robin scheduler
 * Robert Morris, Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include "drr.hh"

DRR::DRR()
{
  MOD_INC_USE_COUNT;
  add_output();
  _next = 0;
  _head = 0;
  _deficit = 0;
  _quantum = 500;
}

DRR::~DRR()
{
  MOD_DEC_USE_COUNT;
}

void
DRR::notify_ninputs(int i)
{
  int j;

  if(_head){
    for(j = 0; j < ninputs(); j++){
      if(_head[j]){
        _head[j]->kill();
        _head[j] = 0;
      }
    }
    delete _head;
  }
  if(_deficit)
    delete _deficit;

  set_ninputs(i);

  _head = new Packet * [ninputs()];
  _deficit = new unsigned [ninputs()];
  for(j = 0; j < ninputs(); j++){
    _head[j] = 0;
    _deficit[j] = 0;
  }
  _next = 0;
}

Packet *
DRR::pull(int)
{
  int n = ninputs();

  // Look at each input once, starting at the *same*
  // one we left off on last time.
  for (int j = 0; j < n; j++) {
    Packet *p;
    if(_head[_next]){
      p = _head[_next];
      _head[_next] = 0;
    } else {
      p = input(_next).pull();
    }
    if(p == 0){
      _deficit[_next] = 0;
    } else if(p->length() <= _deficit[_next]){
      _deficit[_next] -= p->length();
      return(p);
    } else {
      _head[_next] = p;
    }

    _next++;
    if(_next >= n)
      _next = 0;
    _deficit[_next] += _quantum;
  }
  return 0;
}

EXPORT_ELEMENT(DRR)

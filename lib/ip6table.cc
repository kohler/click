/*
 * ip6table.{cc,hh} -- a stupid IP6 routing table, best for small routing tables
 * Robert Morris / Peilei Fan
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ip6table.hh"

IP6Table::IP6Table()
{
}

IP6Table::~IP6Table()
{
}

bool
IP6Table::lookup(IP6Address dst, IP6Address &gw, int &index)
{
  int i, besti = -1;

  for(i = 0; i < _v.size(); i++){
    if(_v[i]._valid && IP6Address((dst & _v[i]._mask)) == _v[i]._dst){
      if(besti == -1 || _v[i]._mask >_v[besti]._mask){
        besti = i;
	//click_chatter("find the match in the routing table -entry is %d", besti);
      }
    }
  }

  if(besti == -1){
    return(false);
  } else {
    gw = _v[besti]._gw;
    
    index = _v[besti]._index;
    return(true);
  }
}

void
IP6Table::add(IP6Address dst, IP6Address mask, IP6Address gw, int index)
{
  struct Entry e;

  e._dst = dst;
  e._mask = mask;
  e._gw = gw;

  e._index = index;
  e._valid = 1;

  int i;
  for(i = 0; i < _v.size(); i++){
    if(_v[i]._valid == 0){
      _v[i] = e;
      return;
    }
  }
  _v.push_back(e);
}

void
IP6Table::del(IP6Address dst, IP6Address mask)
{
  int i;

  for(i = 0; i < _v.size(); i++){
    if(_v[i]._valid && (_v[i]._dst == dst) && (_v[i]._mask == mask)){
      _v[i]._valid = 0;
    }
  }
}

// generate Vector template instance
#include "vector.cc"
template class Vector<IP6Table::Entry>;

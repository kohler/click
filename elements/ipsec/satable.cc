/*
 * SAtable.{cc,hh} -- Implements IPsec Security Association Table
 * Dimitris Syrivelis <jsyr@inf.uth.gr>, Ioannis Avramopoulos <iavramop@cs.princeton.edu>
 *
 * Copyright (c) 2006-2007 University of Thessaly
 * Copyright (c) 2006-2007 Princeton University
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
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <clicknet/ether.h>
#include "satable.hh"
#include "sadatatuple.hh"

CLICK_DECLS

SATable::SATable()
{
}

SATable::~SATable()
{
}

/*Get a reference to SA Data*/
SADataTuple *
SATable::lookup(SPI this_spi)
{
  if (!this_spi) {
    click_chatter("%s: lookup called with NULL spi!\n", name().c_str());
    return NULL;
  }
  //retrieve security association
  SADataTuple  *dat = _table.findp(this_spi);
  return dat;
}

/*Eventually this will be called from userspace Internet Key Exchange transactions*/
int
SATable::insert(SPI spi , SADataTuple SA_data)
{
  if ((!spi) || (!SA_data)) {
    click_chatter("SATable %s: Attempt to insert data failed. Invalid arguments\n",name().c_str());
    return -1;
  }
  SADataTuple * dat = _table.findp(spi);
  if (!dat) {
    _table.insert(spi, SA_data);
  }
  return 0;
}

/*Function to Remove Data*/
int
SATable::remove(unsigned int spi)
{
  if(!spi){
	click_chatter("Invalid SPI parameter");
	return -1;
  }
  SADataTuple  *dat = _table.findp(spi);
  if(!dat) {
	click_chatter("No such entry");
	return -1;
  }
  delete dat;
  _table.remove(SPI(spi));

  return 0;
}

/*Return data to user space file*/
String
SATable::print_sa_data()
{
  StringAccum sa;
  int k;
  for (SIter iter = _table.begin(); iter.live(); iter++) {
    SADataTuple n = iter.value();
    sa << "\nNew Entry\n";
    for(k=0; k< 16;k++)
	{sa << n.Encryption_key[k];}
    sa <<" ";
    for(k=0; k< 16;k++)
        {sa << n.Authentication_key[k];}
    sa << " ";
  }
  return sa.take_string();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SATable)

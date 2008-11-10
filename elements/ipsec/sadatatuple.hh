#ifndef CLICK_SADATATUPLE_HH
#define CLICK_SADATATUPLE_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
CLICK_DECLS

/*
 * sadatatuple.hh -- IPSec ESP Security Parameter Classes
 * Dimitris Syrivelis <jsyr@inf.uth.gr>, Ioannis Avramopoulos <iavramop@cs.princeton.edu>
 *
 * Copyright (c) 2006 University of Thessaly
 * Copyright (c) 2006 Princeton University
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, subject to the conditions listed in the Click LICENSE
 * file. These conditions include: you must preserve this copyright
 * notice, and you cannot mention the copyright holders in advertising
 * related to the Software without their permission.  The Software is
 * provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This notice is a
 * summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#define KEY_SIZE 16

/* Security Parameter Index (SPI) Class*/

class SPI {

   public:
	inline SPI(unsigned int a)
	: _spi(a)
	{
	}

	inline SPI()
	{
		memset(this, 0, sizeof(*this));
	}

	inline operator bool() const
	{
		return (_spi != 0);
	}
	inline bool
	operator==(SPI b)
	{
		return (this->_spi == b._spi);
	}

	inline bool
	operator!=(SPI b)
	{
		return (this->_spi != b._spi);
	}
	uint32_t getValue() const
	{
		return (_spi);
	}
	inline hashcode_t hashcode() const;
   private:
	uint32_t _spi;
 };

// Security Association Data Tuple
class SADataTuple {
  public:

    //SA Data must be added here...
    uint8_t Encryption_key[KEY_SIZE]; // The Data key
    uint8_t Authentication_key[KEY_SIZE];//The Authentication key
    /*These fields below deal with replay protection*/
    uint32_t replay_start_counter;
    uint32_t cur_rpl;
    uint8_t  ooowin;	/* out-of-order window size */
    uint32_t bitmap;	/* Support out-of-order receive support */
    uint32_t lastseq;	/* in host order */

    SADataTuple() {
	memset(this, 0, sizeof(*this));
    }

    SADataTuple(const void * enc_key , const void * Auth_key, uint32_t counter, uint8_t o_oowin)
     {
		memset(this, 0, sizeof(*this));
		memcpy(Encryption_key, enc_key, KEY_SIZE);
		memcpy(Authentication_key, Auth_key, KEY_SIZE);
		replay_start_counter = counter;
		ooowin = o_oowin;
	        bitmap=0;
		lastseq=cur_rpl=counter;
     }

     operator bool() const
     {
         return ((cur_rpl != 0));
     }

String unparse_entries() const
     {
         char buf[71];
	 int i,j;
	 sprintf(buf," |");
	 for(i=0,j=0;i<16;i++,j+=2) {
		sprintf(&buf[2+j],"%02x",Encryption_key[i]);
	 }
	 sprintf(&buf[34],"| |");
	 for(i=0,j=0;i<16;i++,j+=2) {
		sprintf(&buf[37+j],"%02x",Authentication_key[i]);
	 }
	 sprintf(&buf[69],"|");
         return String(buf, 70);
    }
};


inline hashcode_t SPI::hashcode() const
{
    return _spi;
}

CLICK_ENDDECLS
#endif

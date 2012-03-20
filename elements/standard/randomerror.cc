/*
 * randomerror.{cc,hh} -- element introduces errors into packet data
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2006 Regents of the University of California
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
#include "randomerror.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

static int bit_flip_array_idx[] = {
  0, 1, 9, 37, 93, 163, 219, 247, 255, 256
};

static unsigned char bit_flip_array[] = {
  0x00,						  // 00000000

  0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, // 00000001

  0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0x81, // 00000011
  0x05, 0x0A, 0x14, 0x28, 0x50, 0xA0, 0x41, 0x82, // 00000101
  0x09, 0x12, 0x24, 0x48, 0x90, 0x21, 0x42, 0x84, // 00001001
  0x11, 0x22, 0x44, 0x88,			  // 00010001

  0x07, 0x0E, 0x1C, 0x38, 0x70, 0xE0, 0xC1, 0x83, // 00000111
  0x0B, 0x16, 0x2C, 0x58, 0xB0, 0x61, 0xC2, 0x85, // 00001011
  0x13, 0x26, 0x4C, 0x98, 0x31, 0x62, 0xC4, 0x89, // 00010011
  0x0D, 0x1A, 0x34, 0x68, 0xD0, 0xA1, 0x43, 0x86, // 00001101
  0x19, 0x32, 0x64, 0xC8, 0x91, 0x23, 0x46, 0x8C, // 00011001
  0x15, 0x2A, 0x54, 0xA8, 0x51, 0xA2, 0x45, 0x8A, // 00010101
  0x25, 0x4A, 0x94, 0x29, 0x52, 0xA4, 0x49, 0x92, // 00100101

  0x0F, 0x1E, 0x3C, 0x78, 0xF0, 0xE1, 0xC3, 0x87, // 00001111
  0x17, 0x2E, 0x5C, 0xB8, 0x71, 0xE2, 0xC5, 0x8B, // 00010111
  0x27, 0x4E, 0x9C, 0x39, 0x72, 0xE4, 0xC9, 0x93, // 00100111
  0x1B, 0x35, 0x6C, 0xD8, 0xB1, 0x53, 0xC6, 0x8D, // 00011011
  0x2B, 0x56, 0xAC, 0x59, 0xB2, 0x65, 0xCA, 0x95, // 00101011
  0x33, 0x66, 0xCC, 0x99,			  // 00110011
  0x1D, 0x3A, 0x74, 0xE8, 0xD1, 0xA3, 0x47, 0x8E, // 00011101
  0x2D, 0x5A, 0xB4, 0x69, 0xD2, 0xA5, 0x4B, 0x96, // 00101101
  0x35, 0x6A, 0xD4, 0xA9, 0x53, 0xA6, 0x4D, 0x9A, // 00110101
  0x55, 0xAA,					  // 01010101

  0xF8, 0xF1, 0xE3, 0xC7, 0x8F, 0x1F, 0x3E, 0x7C, // 11111000
  0xF4, 0xE9, 0xD3, 0xA7, 0x4F, 0x9E, 0x3D, 0x7A, // 11110100
  0xEC, 0xD9, 0xB3, 0x67, 0xCE, 0x9D, 0x3B, 0x76, // 11101100
  0xF2, 0xE5, 0xCB, 0x97, 0x2F, 0x5E, 0xBC, 0x79, // 11110010
  0xE6, 0xCD, 0x9B, 0x37, 0x6E, 0xDC, 0xB9, 0x73, // 11100110
  0xE9, 0xD5, 0xAB, 0x57, 0x9E, 0x5D, 0xBA, 0x75, // 11101010
  0xDA, 0xB5, 0x6B, 0xD6, 0xAD, 0x5B, 0xB6, 0x6D, // 11011010

  0xFC, 0xF9, 0xF3, 0xE7, 0xCF, 0x9F, 0x3F, 0x7E, // 11111100
  0xFA, 0xF5, 0xEB, 0xD7, 0xAF, 0x5F, 0xBE, 0x7D, // 11111010
  0xF6, 0xED, 0xDB, 0xB7, 0x6F, 0xDE, 0xBD, 0x7B, // 11110110
  0xEE, 0xDD, 0xBB, 0x77,			  // 11101110

  0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F, // 11111110

  0xFF						  // 11111111
};

RandomBitErrors::RandomBitErrors()
{
}

void
RandomBitErrors::set_bit_error(unsigned bit_error)
{
    assert(bit_error <= 0x10000000);
    _p_bit_error = bit_error;
    unsigned non_bit_error = (0xFFFFFFF - bit_error) + 1;
    // _p_error[i] is the probability that <= i bits are flipped

    uint64_t accum = 0;
    for (int i = 0; i < 8; i++) {
	// Calculate the probability of exactly i bit errors,
	// 'bit_error^i * (1-bit_error)^(8-i) * n_combinations',
	// with 35 bits of fraction
	uint64_t p = 0x800000000LL;
	for (int j = 0; j < i; j++)
	    p = (p * bit_error) >> 28;
	for (int j = i; j < 8; j++)
	    p = (p * non_bit_error) >> 28;
	p *= bit_flip_array_idx[i+1] - bit_flip_array_idx[i];
	// accumulated probability of <= i bit errors
	accum += p;
	// shift down to 28 bits of fraction, with rounding
	_p_error[i] = (accum >> 7) & 0x1FFFFFFF;
	if ((accum & 0x7F) >= 0x40)
	    _p_error[i]++;
    }

    // even with that careful rounding, make sure that the last entry is
    // larger than any possible 28-bit random number
    _p_error[8] = 0x10000000;
}

int
RandomBitErrors::configure(Vector<String> &conf, ErrorHandler *errh)
{
    unsigned bit_error;
    String kind_str = "flip";
    bool on = true;
    if (Args(conf, this, errh)
	.read_mp("P", FixedPointArg(28), bit_error)
	.read_p("KIND", kind_str)
	.read_p("ACTIVE", on)
	.complete() < 0)
	return -1;

    unsigned kind;
    if (kind_str == "flip" || kind_str == "")
	kind = 2;
    else if (kind_str == "set")
	kind = 1;
    else if (kind_str == "clear")
	kind = 0;
    else
	return errh->error("bad action %<%s%> (must be %<set%>, %<clear%>, or %<flip%>)",
			   kind_str.c_str());

    if (bit_error > 0x10000000)
	return errh->error("drop probability must be between 0 and 1");
    if (bit_error == 0)
	errh->warning("zero bit error probability (underflow?)");

    // configuration OK; set variables
    set_bit_error(bit_error);
    _kind = kind;
    _on = on;

    return 0;
}

Packet *
RandomBitErrors::simple_action(Packet *p_in)
{
  // if no chance we'll flip a bit, return now
  if (!_on || _p_error[0] >= 0x10000000)
    return p_in;

  WritablePacket *p = p_in->uniqueify();
  unsigned char *data = p->data();
  unsigned len = p->length();
  int *p_error = _p_error;
  int kind = _kind;

  for (unsigned i = 0; i < len; i++) {
    int v = (click_random() >> 3) & 0xFFFFFFF;
    if (v <= p_error[0])
	continue;

    int nb = 1;
    while (v > p_error[nb])
	nb++;

    int idx = bit_flip_array_idx[nb];
    int n = bit_flip_array_idx[nb+1] - idx;
    unsigned char errors = bit_flip_array[ ((click_random() >> 5) % n) + idx ];

    if (kind == 0)
      data[i] &= ~errors;
    else if (kind == 1)
      data[i] |= errors;
    else
      data[i] ^= errors;
  }

  return p;
}

static String
random_bit_errors_read(Element *f, void *vwhich)
{
  int which = (intptr_t)vwhich;
  RandomBitErrors *lossage = (RandomBitErrors *)f;
  if (which == 0)
    return cp_unparse_real2(lossage->p_bit_error(), 28);
  else {
    switch (lossage->kind()) {
     case 0: return "clear";
     case 1: return "set";
     case 2: return "flip";
     default: return "??";
    }
  }
}

void
RandomBitErrors::add_handlers()
{
  add_read_handler("p_bit_error", random_bit_errors_read, 0);
  add_write_handler("p_bit_error", reconfigure_keyword_handler, "0 P");
  add_read_handler("error_kind", random_bit_errors_read, 1);
  add_write_handler("error_kind", reconfigure_keyword_handler, "1 KIND");
  add_data_handlers("active", Handler::OP_READ | Handler::OP_WRITE, &_on);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(RandomBitErrors)
ELEMENT_MT_SAFE(RandomBitErrors)
ELEMENT_REQUIRES(int64)

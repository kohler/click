// -*- c-basic-offset: 4; related-file-name: "../include/click/integers.hh" -*-
/*
 * integers.{cc,hh} -- unaligned/network-order "integers"
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
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
#include <click/config.h>
#include <click/glue.hh>
#include <click/integers.hh>
CLICK_DECLS

// ffs_msb(uint32_t) borrowed from tcpdpriv

#if NEED_FFS_MSB_UNSIGNED
int
ffs_msb(unsigned x)
{
    static_assert(sizeof(unsigned) == 4, "unsigned has unexpected size.");

    int add = 0;
    static uint8_t bvals[] = { 0, 4, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1 };

    if ((x & 0xFFFF0000) == 0) {
        if (x == 0)             /* zero input ==> zero output */
            return 0;
        add += 16;
    } else
        x >>= 16;

    if ((x & 0xFF00) == 0)
        add += 8;
    else
        x >>= 8;

    if ((x & 0xF0) == 0)
        add += 4;
    else
        x >>= 4;

    return add + bvals[x & 0xF];
}
#endif

#define ffs_msb_hilo(type)                      \
    size_t bits = sizeof(type) * 8;             \
    type hi = (type) (x >> bits);               \
    if (hi == 0) {                              \
        type lo = (type) x;                     \
        return lo ? bits + ffs_msb(lo) : 0;     \
    } else                                      \
        return ffs_msb(hi);

#if NEED_FFS_MSB_UNSIGNED_LONG
int
ffs_msb(unsigned long x)
{
    static_assert(sizeof(unsigned) == 4 && sizeof(unsigned long) == 8, "unsigned or unsigned long has unexpected size.");
    ffs_msb_hilo(unsigned);
}
#endif

#if NEED_FFS_MSB_UNSIGNED_LONG_LONG
int
ffs_msb(unsigned long long x)
{
# if SIZEOF_LONG_LONG == 8
    static_assert(sizeof(unsigned) == 4, "unsigned has unexpected size.");
    ffs_msb_hilo(unsigned);
# elif SIZEOF_LONG_LONG == 16
    static_assert(sizeof(unsigned long) == 8, "unsigned long has unexpected size.");
    ffs_msb_hilo(unsigned long);
# else
    static_assert(sizeof(unsigned long long) == 8 || sizeof(unsigned long long) == 16, "unsigned long long has unexpected size.");
# endif
}
#endif

#if NEED_FFS_MSB_UINT64_T
int
ffs_msb(uint64_t x)
{
    static_assert(sizeof(unsigned) == 4, "unsigned has unexpected size.");
    ffs_msb_hilo(unsigned);
}
#endif


#if NEED_FFS_LSB_UNSIGNED
int
ffs_lsb(uint32_t x)
{
    static_assert(sizeof(unsigned) == 4, "unsigned has unexpected size.");

    int add = 0;
    static uint8_t bvals[] = { 0, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1 };

    if ((x & 0x0000FFFF) == 0) {
        if (x == 0)             /* zero input ==> zero output */
            return 0;
        add += 16;
        x >>= 16;
    }

    if ((x & 0x00FF) == 0) {
        add += 8;
        x >>= 8;
    }

    if ((x & 0x0F) == 0) {
        add += 4;
        x >>= 4;
    }

    return add + bvals[x & 0xF];
}
#endif

#define ffs_lsb_hilo(type)                      \
    size_t bits = sizeof(type) * 8;             \
    type lo = (type) x;                         \
    if (lo == 0) {                              \
        type hi = (type) (x >> bits);           \
        return hi ? bits + ffs_lsb(hi) : 0;     \
    } else                                      \
        return ffs_lsb(lo);

#if NEED_FFS_LSB_UNSIGNED_LONG
int
ffs_lsb(unsigned long x)
{
    static_assert(sizeof(unsigned) == 4 && sizeof(unsigned long) == 8, "unsigned or unsigned long has unexpected size.");
    ffs_lsb_hilo(unsigned);
}
#endif

#if NEED_FFS_LSB_UNSIGNED_LONG_LONG
int
ffs_lsb(unsigned long long x)
{
# if SIZEOF_LONG_LONG == 8
    static_assert(sizeof(unsigned) == 4, "unsigned has unexpected size.");
    ffs_lsb_hilo(unsigned);
# elif SIZEOF_LONG_LONG == 16
    static_assert(sizeof(unsigned long) == 8, "unsigned long has unexpected size.");
    ffs_lsb_hilo(unsigned long);
# else
    static_assert(sizeof(unsigned long long) == 8 || sizeof(unsigned long long) == 16, "unsigned long long has unexpected size.");
# endif
}
#endif

#if NEED_FFS_LSB_UINT64_T
int
ffs_lsb(uint64_t x)
{
    static_assert(sizeof(unsigned) == 4, "unsigned has unexpected size.");
    ffs_lsb_hilo(unsigned);
}
#endif


uint32_t
int_sqrt(uint32_t x)
{
    if (x + 1 <= 1)
        return (x ? 0xFFFFU : 0);

    // Newton's algorithm.
    uint32_t y, z;
    // Initial overestimate.
#if NEED_FFS_MSB_UINT32_T
    y = (x / 2) + 1;
#else
    y = 1 << ((32 - ffs_msb(x)) / 2 + 1);
#endif
    do {
        z = y;
        y = (y + (x / y)) / 2;
    } while (y < z);
    // Executed in the integer domain Newton's algorithm may terminate with an
    // overestimate.  Correct that.
    while (y * y > x)
        --y;
    return y;
}

#if HAVE_INT64_TYPES && HAVE_INT64_DIVIDE

uint64_t
int_sqrt(uint64_t x)
{
    if (x + 1 <= 1)
        return (x ? 0xFFFFFFFFU : 0);

    // Newton's algorithm.
    uint64_t y, z;
    // Initial overestimate.
#if NEED_FFS_MSB_INT32_T
    y = (x / 2) + 1;
#else
    y = 1 << ((64 - ffs_msb(x)) / 2 + 1);
#endif
    do {
        z = y;
        y = (y + (x / y)) / 2;
    } while (y < z);
    // Executed in the integer domain Newton's algorithm may terminate with an
    // overestimate.  Correct that.
    while (y * y > x)
        --y;
    return y;
}

#endif

CLICK_ENDDECLS

// -*- c-basic-offset: 4 -*-
#ifndef CLICK_HASHTABLETEST_HH
#define CLICK_HASHTABLETEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

HashTableTest()

=s test

runs regression tests for HashTable<K, V>

=d

HashTableTest runs HashTable regression tests at initialization time. It
does not route packets.

*/

class HashTableTest : public Element { public:

    HashTableTest() CLICK_COLD;

    const char *class_name() const		{ return "HashTableTest"; }

    int initialize(ErrorHandler *) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

// -*- c-basic-offset: 4 -*-
#ifndef CLICK_RUNPARSE_HH
#define CLICK_RUNPARSE_HH
#include "routert.hh"
#include <click/pair.hh>

class RouterUnparserT { public:

    RouterUnparserT(ErrorHandler *);

    struct Pair {
	ElementClassT *first;
	ElementClassT *second;
	Pair(ElementClassT *a, ElementClassT *b) : first(a), second(b) { }
    };

  private:

    HashTable<int, int> _tuid_map;
    Vector<ElementClassT *> _types;

    enum { X_BAD = 0, X_UNK = 1, X_LT = 2, X_LEQ = 3, X_EQ = 4, X_GEQ = 5, X_GT = 6, X_NUM = 7 };
    static int relation_negater[X_NUM];
    static uint8_t relation_combiner[X_NUM][X_NUM];
    HashTable<Pair<ElementClassT *, ElementClassT *>, int> _relation;

    ErrorHandler *_errh;

};

#endif

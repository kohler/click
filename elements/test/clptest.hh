// -*- c-basic-offset: 4 -*-
#ifndef CLICK_CLPTEST_HH
#define CLICK_CLPTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

CLPTest([keywords])

=s test

runs regression tests for CLP command line parser

=d

SortTest runs click_qsort regression tests at initialization time. It
does not route packets.

If additional arguments are provided, SortTest will not perform its normal
tests.  Instead, it will sort those arguments and optionally print out the
results.  At userlevel a file can be sorted as well.

Keyword arguments are:

=over 8

=item FILE

Filename.  Sorts the lines of FILE.

=item NUMERIC

Boolean.  Sort values as numeric.  Default is false.

=item REVERSE

Boolean.  Reverse sort values.  Default is false.

=item PERMUTE

Boolean.  Stable sort values.  Default is false.

=item STDC

Boolean.  Use standard C qsort, not Click sort.  Default is false.

=item OUTPUT

Boolean.  If true, results of the extra sort are printed to standard output.
Default is false.

=back

*/

class CLPTest : public Element { public:

    CLPTest();
    ~CLPTest();

    const char *class_name() const		{ return "CLPTest"; }

    int initialize(ErrorHandler *);
    
};

CLICK_ENDDECLS
#endif

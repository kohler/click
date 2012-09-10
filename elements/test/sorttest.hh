// -*- c-basic-offset: 4 -*-
#ifndef CLICK_SORTTEST_HH
#define CLICK_SORTTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

SortTest([keywords])

=s test

runs regression tests for click_qsort

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

class SortTest : public Element { public:

    SortTest() CLICK_COLD;

    const char *class_name() const		{ return "SortTest"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;

  private:

    Vector<String> _strvec;
    Vector<size_t> _sizevec;
    Vector<int> _permute;
    bool _reverse;
#if CLICK_USERLEVEL
    bool _output;
    bool _stdc;
#endif

    int initialize_vec(ErrorHandler *);

};

CLICK_ENDDECLS
#endif

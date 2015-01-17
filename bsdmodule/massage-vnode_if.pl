#! /usr/bin/perl -w

# massage-vnode_if.pl -- massages vnode_if.pl to make it C++ friendly
# Eddie Kohler
#
# Copyright (c) 2002 International Computer Science Institute
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, subject to the conditions
# listed in the Click LICENSE file. These conditions include: you must
# preserve this copyright notice, and you cannot mention the copyright
# holders in advertising related to the Software without their permission.
# The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
# notice is a summary of the Click LICENSE file; the license in that file is
# legally binding.

@ARGV == 1 && -r $ARGV[0] || die;
rename($ARGV[0], "$ARGV[0]~") || die;
open IN, "$ARGV[0]~" || die;
undef $/;
$_ = <IN>;
close IN;

# Let's see you do this shit in Python.
while (/\A ([\000-\377]*^static.*?[\(,]\s*)
           (\w+)
           ((,.*\)|\))[ \t]*\n)
           \s*
           (\w.*\2\s*);[ \t]*\n
           ([\000-\377]*) \Z/mx) {
    $_ = $1 . $5 . $3 . $6;
}

open OUT, ">$ARGV[0]" || die;
print OUT $_;
close OUT;

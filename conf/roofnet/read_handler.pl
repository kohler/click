#!/usr/bin/perl -w

use strict;


if (scalar(@ARGV) < 1) {
    die "usage: read_handler.pl handler args....\n";
}

my $handler = $ARGV[0];
if (-f "/click/version") {
    my $proc = "/click/$handler";
    $proc =~ s/\./\//g;
    exec "cat $proc";
}
exec "printf \"read $handler\\nquit\\n\" | nc 127.0.0.1 7777";



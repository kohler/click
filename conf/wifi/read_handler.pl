#!/usr/bin/perl -w

use strict;

my $res = 0;

if (scalar(@ARGV) < 1) {
    die "usage: read_handler.pl handler args....\n";
}

my $handler = $ARGV[0];
if (-f "/click/version") {
    my $proc = "/click/$handler";
    $proc =~ s/\./\//g;
    exec "cat $proc";
}


my $results = `printf \"read @ARGV\\nquit\\n\" | nc 127.0.0.1 7777`;
if ($results eq "") {
    print "ERROR\n";
    $res = 1;
}
foreach my $line(split /\n/, $results) {
    next if ($line =~ /200 Goodbye!/);
    next if ($line =~ /^Click::ControlSocket/);
    next if ($line =~ /^200 Read handler/);
    next if ($line =~ /^DATA/);
    print "$line\n";
}


exit $res;

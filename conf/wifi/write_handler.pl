#!/usr/bin/perl -w

use strict;


if (scalar(@ARGV) < 1) {
    die "usage: read_handler.pl handler args....\n";
}

my $handler = shift @ARGV;
if (-f "/click/version") {
    my $proc = "/click/$handler";
    $proc =~ s/\./\//g;
    exec "echo -n \"@ARGV\" > $proc";
}

my $value = join " ", @ARGV;
if ($value =~ /\\/ && $value =~ /\s+/) {
    $value = "\\\"$value\\\"";
}
#print "$handler $value\n";
my $results = `printf \"write $handler $value\\nquit\\n\" | nc 127.0.0.1 7777`;
foreach my $line(split /\n/, $results) {
    next if ($line =~ /^Click::ControlSocket/);
    next if ($line =~ /^200 Write handler/);
    next if ($line =~ /^200 Goodbye!/);
    print "ERROR $line\n";
}

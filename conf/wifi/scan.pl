#!/usr/bin/perl -w

use strict;

system "write_handler.pl bs.reset";
print "scanning on channel: ";
foreach (my $x = 1; $x < 12; $x++) {
    printf "%d", $x%10;
    system "write_handler.pl winfo.channel -1";
    system "/sbin/iwconfig ath0 channel $x";
    system "write_handler.pl winfo.channel $x";
    system "usleep 100000";
}
print "\n";
system "read_handler.pl bs.scan";

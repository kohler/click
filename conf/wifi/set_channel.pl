#!/usr/bin/perl -w

use strict;

my ($channel) = @ARGV;

if ($channel eq "12") {
    exit -1;
}
system "write_handler.pl winfo.channel -1";


if ($channel > 20) {
    system "write_handler.pl set_rate.rate 12";
    system "write_handler.pl rates.insert DEFAULT 12 24 48 18 36 72 96 108";
    system "write_handler.pl rate.reset 1";
} else {
    system "write_handler.pl set_rate.rate 2";
    system "write_handler.pl rates.insert DEFAULT 2 4 11 12 18 22 24 48 36 72 96 108";
    system "write_handler.pl rate.reset 1";
}

system "/sbin/iwconfig ath0 channel $channel";
system "write_handler.pl winfo.channel $channel";


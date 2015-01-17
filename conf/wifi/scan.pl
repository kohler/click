#!/usr/bin/perl -w

use strict;

my $who = `whoami`;
chomp($who);
if (!($who =~ /^root$/)) {
    die "$0:  must be run as root!\n";
}

my $original_channel = `read_handler.pl winfo.channel`;
chomp($original_channel);
system "write_handler.pl bs.reset";

my @b_channels = (1 ..11);
my @a_channels = (34, 36, 38, 40, 42, 44, 46, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165);

my @channels = @b_channels;
if (0) {
    push @channels, @a_channels;
}
my $x = 0;
my $first_a = 0;
my $first_b = 0;
system "write_handler.pl winfo.reset 1";
foreach my $channel (@channels) {
    system "set_channel.pl $channel";
    if ($channel < 15) {
	if (!$first_b) {
	    print "scanning b: ";
	} else{
	    print "\b\b\b\b\b\b\b";
	}
	$first_b = 1;
	printf "%3d/%3d", $channel, $b_channels[scalar(@b_channels)-1];
    } else {
	if (!$first_a) {
	    print "\nscanning a: ";
	} else{
	    print "\b\b\b\b\b\b\b";
	}
	$first_a = 1;
	printf "%3d/%3d", $channel, $a_channels[scalar(@a_channels)-1];
    }
    system "write_handler.pl station_probe.send_probe 1";
    system "usleep 100000";
    $x++;
}

print "\n";
if ($original_channel > 0) {
    system "/sbin/iwconfig ath0 channel $original_channel";
}

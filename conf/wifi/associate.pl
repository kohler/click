#!/usr/bin/perl -w

use strict;
my $desired_ssid;
my $best_ssid = "";
my $best_rssi = 0;
my $best_bssid = "";
my $best_channel = 0;

if (scalar(@ARGV)) {
    $desired_ssid =  shift @ARGV;
}

my @lines = split /\n/, `read_handler.pl bs.scan`;

foreach my $line (@lines) {
    my ($bssid, $rest) = split /\s+/, $line;

    $line =~ / channel (\d+) /;
    my $channel = $1;

    $line =~ / rssi (\d+) /;
    my $rssi = $1;

    $line =~ / ssid (\S+) /;
    my $ssid = $1;

    if ($best_rssi < $rssi &&
	(! defined $desired_ssid ||
	 $desired_ssid eq $ssid)) {
	$best_rssi = $rssi;
	$best_ssid = $ssid;
	$best_channel = $channel;
	$best_bssid = $bssid;
    }
}

print "found: ssid $best_ssid bssid $best_bssid channel $best_channel +$best_rssi\n";
system "/sbin/iwconfig ath0 channel $best_channel";
system "write_handler.pl winfo.bssid $best_bssid";
system "write_handler.pl winfo.channel $best_channel";
system "write_handler.pl winfo.ssid $best_ssid";

system "write_handler.pl station_auth.send_auth_req 1";
system "write_handler.pl station_assoc.send_assoc_req 1";


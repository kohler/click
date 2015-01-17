#!/usr/bin/perl -w

use strict;
my $desired_ssid;
my $best_ssid;
my $best_rssi = 0;
my $best_bssid = "";
my $best_channel = 0;


my %ssid_blacklist;

if (scalar(@ARGV)) {
    $desired_ssid =  shift @ARGV;
}



my $blacklist_file = "~/.ap_blacklist";
my @lines = grep !/^\s*$/, grep !/^\s*#/, split /\n/, `cat $blacklist_file 2>/dev/null`;
foreach my $ssid (@lines) {
    $ssid_blacklist{$ssid} = $ssid;
}

@lines = split /\n/, `read_handler.pl bs.scan`;

foreach my $line (@lines) {
    my ($bssid, $rest) = split /\s+/, $line;

    $line =~ / channel (\d+) /;
    my $channel = $1;

    $line =~ / rssi (\d+) /;
    my $rssi = $1;

    $line =~ / ssid (\S+) /;
    my $ssid = $1;

    if (defined $ssid_blacklist{$ssid}) {
	if (! defined $desired_ssid ||
	    $desired_ssid ne $ssid) {
	    print "# ignoring $ssid\n";
	    next;
	}
    }

    if ($best_rssi < $rssi &&
	(! defined $desired_ssid ||
	 $desired_ssid eq $ssid)) {
	$best_rssi = $rssi;
	$best_ssid = $ssid;
	$best_channel = $channel;
	$best_bssid = $bssid;
    }
}

if (! defined $best_ssid) {
    print STDERR "no ssid found!\n";
    exit -1;
}
system "set_channel.pl $best_channel";

system "write_handler.pl winfo.bssid $best_bssid";
system "write_handler.pl winfo.ssid $best_ssid";

system "write_handler.pl station_auth.send_auth_req 1";
system "write_handler.pl station_assoc.send_assoc_req 1";

system "usleep 100000";
my $associated = `read_handler.pl station_assoc.associated`;

if ($associated =~ /true/) {
    print "association SUCCESS: ssid $best_ssid bssid $best_bssid channel $best_channel +$best_rssi\n";
    exit 0;
}
    print "association FAILURE: ssid $best_ssid bssid $best_bssid channel $best_channel +$best_rssi\n";
exit -1;


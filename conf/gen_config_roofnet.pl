#!/usr/bin/perl -w

use strict;

use Getopt::Long;



sub usage() {
    print STDERR "usage:
    --dev {e. g. ath0}
    --ssid
    --channel
    --gateway
    --mode {a/b/g}
";
    exit 1;
}

sub mac_addr_from_dev($) {
    my $device = $_[0];
    my $output = `/sbin/ifconfig $device 2>&1`;
    if ($output =~ /^$device: error/) {
        return "";
    }
    my @tmp = split(/\s+/, $output);
    my $mac = $tmp[4];
    $mac =~ s/-/:/g;
    my @hex = split(/:/, $mac);
    
    return uc (join ":", @hex[0 .. 5]);
}




sub mac_addr_to_ip($) {
    my $mac = $_[0];
    my @hex = split(/:/, $mac);
    if (scalar(@hex) != 6) {
        return "0";
    }
    # convert the hex digits to decimals                                            
    my $x = hex($hex[3]);
    my $y = hex($hex[4]);
    my $z = hex($hex[5]);

    return "$x.$y.$z";

}

my $dev;
my $channel = 3;
my $ssid = "roofnet4";
my $mode = "g";
my $gateway = 0;
GetOptions('device=s' => \$dev,
	   'channel=i' => \$channel,
	   'ssid=s' => \$ssid,
	   'mode=s' => \$mode,
	   'gateway' => \$gateway,
	   ) or usage();

if (! defined $dev) {
    usage();
}


if ($gateway) {
    $gateway = "true";
} else{
    $gateway = "false";
}
if ($dev =~ /wlan/) {
    $mode= "b";
}

my $hostname = `hostname`;
my $wireless_mac = mac_addr_from_dev($dev);
my $suffix;
if ($hostname =~ /rn-pdos(\S+)-wired/) {
    $suffix = "0.0.$1";
    $channel = "11";
} else {
    $suffix = mac_addr_to_ip($wireless_mac);
}



my $srcr_ip = "5." . $suffix;
my $safe_ip = "6." . $suffix;
my $ap_ip = "12." . $suffix;

my $srcr_nm = "255.0.0.0";
my $srcr_net = "5.0.0.0";
my $srcr_bcast = "5.255.255.255";


if ($wireless_mac eq "" or 
    $wireless_mac eq "00:00:00:00:00:00") {
    print STDERR "got invalid mac address!";
    exit -1;
}
system "ifconfig $dev up";
my $iwconfig = "/home/roofnet/bin/iwconfig";

if (-f "/sbin/iwconfig") {
    $iwconfig = "/sbin/iwconfig";
}
system "$iwconfig $dev mode Ad-Hoc";
system "$iwconfig $dev channel $channel";

if (!($dev =~ /ath/)) {
    system "/sbin/ifconfig $dev $safe_ip";
    system "/sbin/ifconfig $dev mtu 1650";
}

if ($dev =~ /wlan/) {
    system "/home/roofnet/bin/prism2_param $dev pseudo_ibss 1";
    system "$iwconfig $dev essid $ssid";
    system "$iwconfig $dev rts off";
    system "$iwconfig $dev retries 8";
    system "$iwconfig $dev txpower 23";
}

if ($dev =~ /ath/) {
    if ($mode =~ /a/) {
	system "iwpriv ath0 mode 1 1>&2";
    } elsif ($mode=~ /g/) {
	system "iwpriv ath0 mode 3 1>&2";
    } else {
	# default b mode
	print STDERR "in b mode\n";
	system "iwpriv ath0 mode 2 1>&2";
    }
}


my $probes = "2 60 2 1500 4 1500 11 1500 22 1500";

if ($mode =~ /g/) {
    $probes = "2 60 12 60 2 1500 4 1500 11 1500 22 1500 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500";
} elsif ($mode =~ /a/) {
    $probes = "12 60 2 60 12 1500 24 1500 48 1500 72 1500 96 1500 108 1500";
}


my $srcr_es_ethtype = "0942";
my $srcr_forwarder_ethtype = "0943";
my $srcr_ethtype = "0944";
my $srcr_gw_ethtype = "092c";

if ($mode =~ /g/) {
    print "rates :: AvailableRates(DEFAULT 2 4 11 12 18 22 24 36 48 72 96 108,
$wireless_mac 2 4 11 12 18 22 24 36 48 72 96 108);\n\n";
} elsif ($mode =~ /a/) {
    print "rates :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108,
$wireless_mac 12 18 24 36 48 72 96 108);\n\n";
} else {
    print "rates :: AvailableRates(DEFAULT 2 4 11 22);\n\n";
}

print <<EOF;
// has one input and one output
// takes and spits out ip packets
elementclass LinuxIPHost {
    \$dev, \$ip, \$nm |

  input -> CheckIPHeader()
    -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) 
    -> SetPacketType(HOST) 
    -> to_host :: ToHost(\$dev);


  from_host :: FromHost(\$dev, \$ip/\$nm) 
    -> fromhost_cl :: Classifier(12/0806, 12/0800);


  // arp packets
  fromhost_cl[0] 
    -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) 
    -> SetPacketType(HOST) 
    -> ToHost();
  
  // IP packets
  fromhost_cl[1]
    -> Strip(14)
    -> CheckIPHeader 
    -> GetIPAddress(16) 
    -> MarkIPHeader(0)
    -> output;

}




elementclass SniffDevice {
    \$device, \$promisc|
  from_dev :: FromDevice(\$device, PROMISC \$promisc)
  -> t1 :: Tee
  -> output;

    t1 [1] -> ToHostSniffers(\$device);

  input
  -> t2 :: PullTee
  -> to_dev :: ToDevice(\$device);

    t2 [1] -> ToHostSniffers(\$device);
}

sniff_dev :: SniffDevice($dev, false);

sched :: PrioSched()
-> set_power :: SetTXPower(POWER 63)
//-> Print ("to_dev", TIMESTAMP true)
-> sniff_dev;

route_q :: NotifierQueue(50) 
EOF

    if ($dev =~ /ath/) {
    print "-> WifiEncap(0x00, 00:00:00:00:00:00)\n";
}

print <<EOF;
-> [0] sched;

data_q :: NotifierQueue(50)
//-> Print("after_q", TIMESTAMP true)
-> auto_rate :: MadwifiRate(OFFSET 0,
			    RT rates)
//-> auto_rate :: AutoRateFallback(OFFSET 0,
//				 STEPUP 25,
//				 RT rates)
EOF

if ($dev =~ /ath/) {
    print "-> WifiEncap(0x00, 00:00:00:00:00:00)\n";
}
print <<EOF;
//-> Print ("after_rate", TIMESTAMP true)
-> [1] sched;


Idle -> ap_control_q :: NotifierQueue(50) 
-> [2] sched;
Idle -> ap_data_q :: NotifierQueue(50)

EOF

if ($dev =~ /ath/) {
    print "-> WifiEncap(0x02, $wireless_mac)\n";
}

print <<EOF;

-> SetTXRate(RATE 2)
-> [3] sched;

// make sure this is listed first so it gets tap0
srcr_host :: LinuxIPHost(srcr, $srcr_ip, $srcr_nm);

srcr_arp :: ARPTable();
srcr_lt :: LinkTable(IP $srcr_ip);


srcr_gw :: GatewaySelector(ETHTYPE 0x$srcr_gw_ethtype,
			   IP $srcr_ip,
			   ETH $wireless_mac,
			   LT srcr_lt,
			   ARP srcr_arp,
			   PERIOD 15,
			   GW $gateway,
			   LM srcr_ett);


srcr_gw -> SetSRChecksum -> route_q;

srcr_set_gw :: SetGateway(SEL srcr_gw);


srcr_es :: ETTStat(ETHTYPE 0x$srcr_es_ethtype, 
		   ETH $wireless_mac, 
		   IP $srcr_ip, 
		   PERIOD 30000, 
		   TAU 300000, 
		   ARP srcr_arp,
		   PROBES \"$probes\",
		   ETT srcr_ett,
		   RT rates);


srcr_ett :: ETTMetric(ETT srcr_es,
		      IP $srcr_ip, 
		      LT srcr_lt);


srcr_forwarder :: SRForwarder(ETHTYPE 0x$srcr_forwarder_ethtype, 
			      IP $srcr_ip, 
			      ETH $wireless_mac, 
			      ARP srcr_arp, 
			      LM srcr_ett,
			      LT srcr_lt);


srcr_querier :: SRQuerier(ETHTYPE 0x$srcr_ethtype, 
			  IP $srcr_ip, 
			  ETH $wireless_mac, 
			  SR srcr_forwarder,
			  LT srcr_lt, 
			  ROUTE_DAMPENING true,
			  TIME_BEFORE_SWITCH 5,
			  DEBUG true);

srcr_query_forwarder :: SRQueryForwarder(ETHTYPE 0x$srcr_ethtype, 
					 IP $srcr_ip, 
					 ETH $wireless_mac, 
					 SR srcr_forwarder,
					 LT srcr_lt, 
					 ARP srcr_arp,
					 LM srcr_ett,
					 DEBUG true);

srcr_query_responder :: SRQueryResponder(ETHTYPE 0x$srcr_ethtype, 
					 IP $srcr_ip, 
					 ETH $wireless_mac, 
					 SR srcr_forwarder,
					 LT srcr_lt, 
					 ARP srcr_arp,
					 LM srcr_ett,
					 DEBUG true);


srcr_query_responder -> SetSRChecksum -> route_q;
srcr_query_forwarder -> SetSRChecksum -> route_q;

srcr_data_ck :: SetSRChecksum() 

srcr_host 
-> SetTimestamp()
-> counter_incoming :: IPAddressCounter(USE_DST true)
-> srcr_host_cl :: IPClassifier(dst net $srcr_ip mask $srcr_nm,
				-)
-> srcr_querier
-> srcr_data_ck;


srcr_host_cl [1] -> [0] srcr_set_gw [0] -> srcr_querier;

srcr_forwarder[0] 
  -> srcr_dt ::DecIPTTL
  -> srcr_data_ck
  -> data_q;
srcr_dt[1] -> ICMPError($srcr_ip, timeexceeded, 0) -> srcr_querier;


// queries
srcr_querier [1] -> SetSRChecksum -> route_q;

srcr_es 
-> SetTimestamp()
-> route_q;

srcr_forwarder[1] //ip packets to me
  -> StripSRHeader()
  -> CheckIPHeader()
  -> from_gw_cl :: IPClassifier(src net $srcr_net mask $srcr_nm,
				-)
  -> counter_outgoing :: IPAddressCounter(USE_SRC true)
  -> srcr_host;

from_gw_cl [1] -> [1] srcr_set_gw [1] -> srcr_host;


EOF


print <<EOF;
txf :: WifiTXFeedback() -> WifiDecap() -> [1] auto_rate;
Idle -> WifiDecap() -> [1] auto_rate;


EOF


if ($dev =~ /ath/) {
    print "sniff_dev -> SetTimestamp() -> dupe :: WifiDupeFilter(WINDOW 5) -> WifiDecap()\n";
} else {
    print "sniff_dev\n";
}
print <<EOF;
-> HostEtherFilter($wireless_mac, DROP_OTHER true, DROP_OWN true) 
//-> rxstats :: RXStats()
-> ncl :: Classifier(
		     12/$srcr_forwarder_ethtype, //srcr_forwarder
		     12/$srcr_ethtype, //srcr
		     12/$srcr_es_ethtype, //srcr_es
		     12/$srcr_gw_ethtype, //srcr_gw
		     -);


// ethernet packets
ncl[0] -> CheckSRHeader() -> [0] srcr_forwarder;
ncl[1] -> CheckSRHeader() -> srcr_query_t :: Tee(2);

srcr_query_t [0] -> srcr_query_forwarder;
srcr_query_t [1] -> srcr_query_responder;

ncl[2] -> srcr_es;
ncl[3] -> CheckSRHeader() -> srcr_gw;
EOF


if ($dev =~ /ath/) {
print <<EOF;
ncl[4] 
-> ToHost(safe);

FromHost(safe, $safe_ip/8, ETHER $wireless_mac) 
-> WifiEncap(0x0, 00:00:00:00:00:00)
-> ap_control_q;

EOF
} else {
    print "ncl[4] -> ToHost($dev);\n";
}

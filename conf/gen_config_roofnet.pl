#!/usr/bin/perl -w

use strict;

use Getopt::Long;



sub usage() {
    print STDERR "usage:
    --dev {e. g. ath0}
    --gateway
    --ssid
    --channel
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
    return uc($mac);
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
my $gateway = 0;
my $channel = 3;
my $ap_ssid = "\"madwifi\"";
my $ssid = "roofnet4";

GetOptions('device=s' => \$dev,
	   'channel=i' => \$channel,
	   'gateway' => \$gateway,
	   'ssid=s' => \$ssid,
	   ) or usage();

if (! defined $dev) {
    usage();
}




my $wireless_mac = mac_addr_from_dev($dev);
my $srcr_ip = "5." . mac_addr_to_ip($wireless_mac);
my $safe_ip = "6." . mac_addr_to_ip($wireless_mac);
my $ap_ip = "12." . mac_addr_to_ip($wireless_mac);

my $srcr_nm = "255.0.0.0";
my $srcr_net = "5.0.0.0";
my $srcr_bcast = "5.255.255.255";


if ($wireless_mac eq "" or 
    $wireless_mac eq "00:00:00:00:00:00") {
    print STDERR "got invalid mac address!";
    exit -1;
}
system "ifconfig $dev up";
system "iwconfig $dev mode Ad-Hoc";
system "iwconfig $dev channel $channel";

if (!($dev =~ /ath/)) {
    system "/sbin/ifconfig $dev $safe_ip";
    system "/sbin/ifconfig $dev mtu 1650";
}

if ($dev =~ /wlan/) {
    system "prism2_param $dev pseudo_ibss 1";
    system "iwconfig $dev essid $ssid";
    system "iwconfig $dev rts off";
    system "iwconfig $dev retries 8";
    system "iwconfig $dev txpower 23";
}

print <<EOF;


rates :: AvailableRates(DEFAULT 2 4 11 22);

elementclass APDevice {
    \$device, \$mac, \$promsic|

  beacon_source :: BeaconSource(INTERVAL 1000, 
				CHANNEL $channel,
				SSID $ap_ssid,
				BSSID \$mac,
				RT rates,
				)
    -> output;
  
  ar :: AssociationResponder(INTERVAL 1000,
			     SSID $ap_ssid,
			     BSSID \$mac,
			     RT rates,
			     )
    -> output;
  
  pr :: ProbeResponder(INTERVAL 1000,
		       SSID $ap_ssid,
		       BSSID \$mac,
		       CHANNEL $channel,
		       RT rates,
		       )
    -> output;
  
  auth :: OpenAuthResponder(BSSID \$mac) ->output;
  
  
  wifi_cl :: Classifier(0/00%0c, //mgt
			0/08%0c); //data

  // management
  wifi_cl [0] -> management_cl :: Classifier(0/00%f0, //assoc req
					     0/10%f0, //assoc resp
					     0/40%f0, //probe req
					     0/50%f0, //probe resp
					     0/80%f0, //beacon
					     0/a0%f0, //disassoc
					     0/b0%f0, //disassoc
					     );
  
  
  
  management_cl [0] -> Print ("assoc_req") -> ar;
  management_cl [1] -> Print ("assoc_resp") -> Discard;
  management_cl [2] -> pr;
  management_cl [3] -> Print ("probe_req",200) ->bs :: BeaconScanner(RT rates) ->  Discard; 
  management_cl [4] -> bs;
  management_cl [5] -> Print ("disassoc") -> Discard;
  management_cl [6] -> Print ("auth") -> auth;
  
  //data
  wifi_cl [1] 
    -> bssid_cl :: Classifier(16/000000000000,
			      -);

  bssid_cl [0]
    -> WifiDecap() 
    -> sniff_safe_t :: Tee()
    -> SetPacketType(HOST) 
    -> [1] output;
  
  bssid_cl [1]
    -> WifiDecap()
    -> sniff_ap_t :: Tee()
    -> SetPacketType(HOST) 
    -> [2] output;
    
  sniff_safe_t [1] -> ToHostSniffers(safe);
  sniff_ap_t [1] -> ToHostSniffers(ap);

  from_dev :: FromDevice(\$device);
  from_dev -> wifi_cl;
  
  input [0] -> ToDevice(\$device);


}

// has one input and one output
// takes and spits out ip packets
elementclass LinuxIPHost {
    \$dev, \$ip, \$nm |

  input -> CheckIPHeader()
    -> EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) 
    -> SetPacketType(HOST) 
    -> CycleCountAccum
    -> to_host :: ToHost(\$dev);


  from_host :: FromHost(\$dev, \$ip/\$nm) 
    -> SetCycleCount
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
  -> output;

  input
  -> to_dev :: ToDevice(\$device);
}

EOF

if ($dev =~ /ath/) {
    print "sniff_dev :: APDevice($dev, $wireless_mac, false);\n";
} else {
    print "sniff_dev :: SniffDevice($dev, false);\n";
}


print <<EOF;
sched :: PrioSched()
-> sniff_dev;

route_q :: NotifierQueue(50) 
EOF

    if ($dev =~ /ath/) {
    print "-> WifiEncap(0x00, 00:00:00:00:00:00)\n";
}

print <<EOF;
-> SetTXRate(RATE 2)
-> [0] sched;

ecn_q :: ECNQueue(LENGTH 100, DEBUG false) 
 -> srcr_rate :: SetTXRate(ETHTYPE 0x092a,
			   RATE 11, 
			   ETT srcr_ett)

EOF

if ($dev =~ /ath/) {
    print "-> WifiEncap(0x00, 00:00:00:00:00:00)\n";
}
print <<EOF;

-> [1] sched;


Idle -> ap_control_q :: NotifierQueue(50) 
-> [2] sched;
Idle -> ap_data_q :: NotifierQueue(50)

EOF

if ($dev =~ /ath/) {
    print "-> WifiEncap(0x02, $wireless_mac)\n";
}

my $gw_string = "false";

if ($gateway) {
    $gw_string = "true";
}

print <<EOF;

-> SetTXRate(RATE 2)
-> [3] sched;

// make sure this is listed first so it gets tap0
srcr_host :: LinuxIPHost(srcr, $srcr_ip, $srcr_nm);

srcr_arp :: ARPTable();
srcr_lt :: LinkTable(IP $srcr_ip);


srcr_es :: ETTStat(ETHTYPE 0x0932, 
		   ETH $wireless_mac, 
		   IP $srcr_ip, 
		   PERIOD 10000, 
		   TAU 180000, 
		   SIZE 1500,
		   ARP srcr_arp,
		   ETT srcr_ett);

srcr_ett :: ETTMetric(ETT srcr_es,
		      IP $srcr_ip, 
		      LT srcr_lt,
		      2WAY_METRICS true,
		      2_WEIGHT 14,
		      4_WEIGHT 28,
		      11_WEIGHT 63,
		      22_WEIGHT 100);


srcr_forwarder :: SRForwarder(ETHTYPE 0x092a, 
			      IP $srcr_ip, 
			      ETH $wireless_mac, 
			      ARP srcr_arp, 
			      SRCR srcr,
			      LM srcr_ett,
			      LT srcr_lt);

srcr :: SRCR(ETHTYPE 0x092b, 
	   IP $srcr_ip, 
	   ETH $wireless_mac, 
	   SR srcr_forwarder,
	   LT srcr_lt, 
	   ARP srcr_arp, 
           LM srcr_ett,
	   ROUTE_DAMPENING true,
	   TIME_BEFORE_SWITCH 5,
	   DEBUG false);

srcr_gw :: GatewaySelector(ETHTYPE 0x092c, 
		      IP $srcr_ip, 
		      ETH $wireless_mac, 
		      LT srcr_lt, 
		      ARP srcr_arp, 
		      PERIOD 15,
		      GW $gw_string,
		      LM srcr_ett);

srcr_set_gw :: SetGateway(SEL srcr_gw);

srcr_flood :: CounterFlood(ETHTYPE 0x0941,
			   IP $srcr_ip,
			   ETH $wireless_mac,
			   BCAST_IP $srcr_bcast,
			   COUNT 0,
			   MAX_DELAY_MS 750,
			   DEBUG false,
			   HISTORY 1000);

srcr_ratemon :: IPRateMonitor(BYTES, 1, 0);
srcr_host -> srcr_host_cl :: IPClassifier(dst host $srcr_bcast,
					  dst net $srcr_ip mask $srcr_nm,
					  -);


srcr_data_ck :: SetSRChecksum() 
srcr_forwarder[1] -> srcr_data_ck;
srcr_forwarder[0] 
  -> srcr_dt ::DecIPTTL
  -> srcr_data_ck
  -> srcr_data_t:: Tee(2) 
  -> ecn_q;
srcr_dt[1] -> ICMPError($srcr_ip, timeexceeded, 0) -> srcr_host_cl;

// SRCR handles data source-routing packets
srcr_data_t[1] -> [1] srcr; //data packets (for link error replies)
srcr_host_cl[0] -> [1] srcr_flood; //local broadcast packets
srcr_host_cl[1] -> [1] srcr_ratemon; //ip packets for the wireless network
srcr_host_cl[2] 
  -> [0] srcr_set_gw [0] 
  -> [1] srcr_ratemon; //ip packets for the gatewa

srcr_flood [0] -> ecn_q;
srcr_flood [1] 
  -> StripSRHeader() 
  -> CheckIPHeader()
  -> srcr_host;

srcr_ratemon [1] ->  
gw_incoming_cl :: IPClassifier(src host $srcr_ip,
			       -);
gw_incoming_cl [0] -> [2] srcr;
gw_incoming_cl [1] 
  -> gw_counter_incoming :: IPAddressCounter(USE_DST true)
  -> [2] srcr; //ip packets for the wireless network

srcr[0] -> SetSRChecksum -> route_q;
srcr_es -> route_q;

srcr_gw -> SetSRChecksum -> route_q;

srcr_io_q :: InOrderQueue(LENGTH 100, PACKET_TIMEOUT 500, DEBUG false) 

srcr_forwarder[2] //ip packets to me
  -> srcr_io_q
  -> StripSRHeader()
  -> CheckIPHeader()
EOF

    if ($gateway) {

print <<EOF;
  -> gw_outgoing_cl :: IPClassifier(dst host $srcr_ip,
			   -);
gw_outgoing_cl[0] -> srcr_host;

gw_outgoing_cl[1] 
  -> gw_counter_outgoing :: IPAddressCounter(USE_SRC true)
  -> srcr_ratemon
  -> srcr_host; 
  Idle -> [1] srcr_set_gw [1]-> Discard;
EOF
} else {
print <<EOF;
  -> from_gw_cl :: IPClassifier(src net $srcr_net mask $srcr_nm,
				-);
from_gw_cl[0]
  -> srcr_ratemon
  -> srcr_host; 

from_gw_cl[1] 
-> [1] srcr_set_gw [1] 
-> srcr_ratemon;
EOF
}

print <<EOF;
txf :: WifiTXFeedback() 

txf [0] -> Discard;
txf [1] -> Print ("failure") -> Discard;
// both successes and failures go to the LinkFailureDetection

Idle
-> failure_cl :: Classifier(12/092a) //srcr_forwarder
-> srcr_fail_filter :: FilterFailures(MAX_FAILURES 15, ALLOW_SUCCESS false)
-> ecn_q;


EOF


    if ($dev =~ /ath/) {
print <<EOF;

sniff_dev [0] -> SetTXRate(RATE 2) -> ap_control_q;

sniff_dev [2] -> ToHost(ap);/* ap */
FromHost(ap, $ap_ip/8, ETHER $wireless_mac) -> ap_data_q;


sniff_dev [1] /* safe */
EOF
} else {
    print "sniff_dev\n";
}
print <<EOF;
-> HostEtherFilter($wireless_mac, DROP_OTHER true, DROP_OWN true) 
-> rxstats :: RXStats()


-> ncl :: Classifier(
		     12/092a, //srcr_forwarder
		     12/092b, //srcr
		     12/092c, //srcr_gw
		     12/0941, //srcr_flood
		     12/0932, //srcr_es
		     -);


// ethernet packets
ncl[0] -> CheckSRHeader() -> srcr_dupe :: DupeFilter(WINDOW 100) -> [0] srcr_forwarder;
ncl[1] -> CheckSRHeader() -> [0] srcr;
ncl[2] -> CheckSRHeader() -> srcr_gw;
ncl[3] -> srcr_flood;
ncl[4] -> srcr_es;
EOF


if ($dev =~ /ath/) {
print <<EOF;
ncl[5] 
-> ToHost(safe);

FromHost(safe, $safe_ip/8, ETHER $wireless_mac) 
-> WifiEncap(0x0, 00:00:00:00:00:00)
-> ap_control_q;

EOF
} else {
    print "ncl[5] -> ToHost($dev);\n";
}

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
    --ap ( act as an access point. off by default)
    --txf (enable/disable tx-feedback. on by default)
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
my $ssid;
my $mode = "g";
my $gateway = 0;
my $ap = 0;
my $txf = 1;
my $interval = 10000;
my $rate_control = "static-2";
my $kernel = 0;
GetOptions('device=s' => \$dev,
	   'channel=i' => \$channel,
	   'ssid=s' => \$ssid,
	   'mode=s' => \$mode,
	   'gateway' => \$gateway,
	   'ap!' => \$ap,
	   'txf!' => \$txf,
	   'rate-control=s' => \$rate_control,
	   'kernel!' => \$kernel,
	   ) or usage();


if (! defined $dev) {
    if (`/sbin/ifconfig ath0 2>&1` =~ /Device not found/) {
	if (`/sbin/ifconfig wlan0 2>&1` =~ /Device not found/) {
	} else {
	    $dev = "wlan0";
	}
    } else {
	$dev = "ath0";
    }
}


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
my $etx_ip = "4." . $suffix;
my $safe_ip = "6." . $suffix;
my $rate_ip = "7." . $suffix;


my $srcr_nm = "255.0.0.0";
my $srcr_net = "5.0.0.0";
my $srcr_bcast = "5.255.255.255";

if (! defined $ssid) {
    $ssid = "roofnet.$srcr_ip";
}
if ($wireless_mac eq "" or 
    $wireless_mac eq "00:00:00:00:00:00") {
    print STDERR "got invalid mac address!";
    exit -1;
}
system "/sbin/ifconfig $dev up";
my $iwconfig = "/home/roofnet/bin/iwconfig";

if (-f "/sbin/iwconfig") {
    $iwconfig = "/sbin/iwconfig";
}

system "$iwconfig $dev mode Ad-Hoc";


#system "/sbin/ifconfig $dev $safe_ip";
system "/sbin/ifconfig $dev mtu 1800";


if ($dev =~ /wlan/) {
    system "/home/roofnet/bin/prism2_param $dev ptype 6";
    system "/home/roofnet/bin/prism2_param $dev pseudo_ibss 1";
    system "/home/roofnet/bin/prism2_param $dev monitor_type 1";
    system "$iwconfig $dev essid $ssid";
    system "$iwconfig $dev rts off";
    system "$iwconfig $dev retries 16";
    # make sure we broadcast at a fixed power
    system "/home/roofnet/bin/prism2_param $dev alc 0";
    system "$iwconfig $dev txpower 23";

}

if ($dev =~ /ath/) {
    system "/sbin/ifconfig ath0 txqueuelen 5";
}

system "$iwconfig $dev channel $channel";

system "/sbin/modprobe tun > /dev/null 2>&1";
my $probes = "2 60 2 1500 4 1500 11 1500 22 1500";

if ($mode =~ /g/) {
    $probes = "2 60 12 60 2 1500 4 1500 11 1500 22 1500 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500";
} elsif ($mode =~ /a/) {
    $probes = "12 60 2 60 12 1500 24 1500 48 1500 72 1500 96 1500 108 1500";
}



my $srcr_es_ethtype = "0941";  # broadcast probes
my $srcr_forwarder_ethtype = "0943"; # data
my $srcr_ethtype = "0944";  # queries and replies
my $srcr_gw_ethtype = "092c"; # gateway ads

if ($mode =~ /g/) {
    print "rates :: AvailableRates(DEFAULT 2 4 11 12 18 22 24 36 48 72 96 108,
$wireless_mac 2 4 11 12 18 22 24 36 48 72 96 108);\n\n";
} elsif ($mode =~ /a/) {
    print "rates :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108,
$wireless_mac 12 18 24 36 48 72 96 108);\n\n";
} else {
    print "rates :: AvailableRates(DEFAULT 2 4 11 22);\n\n";
}



my $data = "";
for (my $x = 0; $x < 1500; $x++) {
    $data .= "ff";
}

$data = "\\<$data>";


system "cat /home/roofnet/scripts/srcr.click";
system "cat /home/roofnet/scripts/etx.click";

print <<EOF;
// has one input and one output
// takes and spits out ip packets

elementclass LinuxHost {
    \$dev, \$ip, \$nm, \$mac |
    input -> ToHost(\$dev);
    FromHost(\$dev, \$ip/\$nm, ETHER \$mac) -> output;
}


EOF

if ($kernel) {
print <<EOF;

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

EOF
} else {
    print <<EOF;
// has one input and one output
// takes and spits out ip packets
elementclass LinuxIPHost {
    \$dev, \$ip, \$nm |

  input -> KernelTun(\$ip/\$nm, MTU 1500, DEV_NAME \$dev) 
  -> MarkIPHeader(0)
  -> CheckIPHeader()
  -> output;

}

elementclass SniffDevice {
    \$device, \$promisc|
  from_dev :: FromDevice(\$device, PROMISC \$promisc) -> output;
  input -> to_dev :: ToDevice(\$device);
}

EOF
    }

if (!$kernel) {
    print "control :: ControlSocket(\"TCP\", 7777);\n";
    print "chatter :: ChatterSocket(\"TCP\", 7778);\n";
}

print <<EOF;
sniff_dev :: SniffDevice($dev, false);

sched :: PrioSched()
//-> prism2_encap :: Prism2Encap()
-> set_power :: SetTXPower(POWER 63)
-> extra_encap :: ExtraEncap()
//-> Print ("to_dev", TIMESTAMP true)
-> sniff_dev;

route_q :: FullNoteQueue(10) 
-> [0] sched;

data_q :: FullNoteQueue(10)
-> data_static_rate :: SetTXRate(RATE 2)
-> data_madwifi_rate :: MadwifiRate(OFFSET 4,
			       ALT_RATE true,
			       RT rates,
			       ACTIVE true)
-> data_arf_rate :: AutoRateFallback(OFFSET 4,
				STEPUP 25,
				RT rates,
				ACTIVE false)
-> data_probe_rate :: ProbeTXRate(OFFSET 4,
			     WINDOW 5000,
			     ALT_RATE true,
			     RT rates,
			     ACTIVE false)

-> [1] sched;

Idle -> [1] data_probe_rate;
Idle -> [1] data_madwifi_rate;
Idle -> [1] data_arf_rate;



srcr :: srcr_ett($srcr_ip, $srcr_nm, $wireless_mac, $gateway, 
		 "$probes");

etx :: srcr_etx($etx_ip, $srcr_nm, $wireless_mac, $gateway, 
		 "2 132");



// make sure this is listed first so it gets tap0
srcr_host :: LinuxIPHost(srcr, $srcr_ip, $srcr_nm)
-> [1] srcr;

// make sure this is listed first so it gets tap0
etx_host :: LinuxIPHost(etx, $etx_ip, $srcr_nm)
-> [1] etx;


route_encap :: WifiEncap(0x0, 00:00:00:00:00:00)
->  route_q;
data_encap :: WifiEncap(0x0, 00:00:00:00:00:00)
-> data_q;

srcr [0] -> route_encap;   // queries, replies
srcr [1] -> route_encap;   // bcast_stats
srcr [2] -> data_encap;    // data
srcr [3] -> srcr_host; // data to me


etx [0] -> route_encap;   // queries, replies
etx [1] -> route_encap;   // bcast_stats
etx [2] -> data_encap;    // data
etx [3] -> etx_host; // data to me

EOF


print <<EOF;

sniff_dev 
-> prism2_decap :: Prism2Decap()
-> phyerr_filter :: FilterPhyErr()
-> extra_decap :: ExtraDecap()
//-> PrintWifi(fromdev)
-> beacon_cl :: Classifier(0/00%0c 0/80%f0, //beacons
			    -)
-> bs :: BeaconScanner(RT rates)
-> Discard;

beacon_cl [1]
-> Classifier(0/08%0c) //data
-> rate_cl :: Classifier(30/9000,-); //rate packets

rate_cl [1] 
-> tx_filter :: FilterTX()
-> dupe :: WifiDupeFilter() 
-> WifiDecap()
-> HostEtherFilter($wireless_mac, DROP_OTHER true, DROP_OWN true) 
-> rxstats :: RXStats()
-> ncl :: Classifier(
		     12/09??,
		     12/0a??,
		     -);


ncl [0] -> srcr;
ncl [1] -> etx;



ncl[2] 
EOF

if ($kernel) {
    print "-> ToHost(ath0);\n";
} else {
    print "-> Discard;\n";
}

    if ($txf) {
print <<EOF;
tx_filter [1] 
//-> PrintWifi(txf)
-> txf_t2 :: Tee(3);

txf_t2 [0] -> [1] data_arf_rate;
txf_t2 [1] -> [1] data_madwifi_rate;
txf_t2 [2] -> [1] data_probe_rate;

EOF
}



print <<EOF;



rate_arf_rate :: AutoRateFallback(OFFSET 4,
                             ACTIVE false,
                             STEPUP 10,
                             THRESHOLD 0,
                             ADAPTIVE_STEPUP false,
                             RT rates);
rate_aarf_rate :: AutoRateFallback(OFFSET 4,
                             ACTIVE false,
                             STEPUP 10,
                             THRESHOLD 0,
                             ADAPTIVE_STEPUP true,
			      RT rates);

rate_madwifi_rate :: MadwifiRate(OFFSET 4,
                            ACTIVE false,
                            THRESHOLD 0,
                            RT rates);
rate_probe_rate :: ProbeTXRate(OFFSET 4,
                          ACTIVE false,
                          WINDOW 10000,
                          MIN_SAMPLE 35,
                          THRESHOLD 0,
                          FILTER_LOW_RATES 1,
                          FILTER_NEVER_SUCCESS 1,
                          AGGRESSIVE_ALT_RATE 1,
                          RT rates);
rate_static_rate :: SetTXRate(RATE 2);



rate_cl [0] 
-> rate_tx_filter :: FilterTX()
-> rate_dupe :: WifiDupeFilter() 
//-> Print(fromdev, 60)
-> WifiDecap()
-> HostEtherFilter($wireless_mac, DROP_OTHER true, DROP_OWN true) 
-> rate_rxstats :: RXStats()
-> rate_count :: Counter()
-> Discard;

rate_inf_src :: InfiniteSource(DATA $data, LIMIT -1, ACTIVE false)
//-> seq :: IncrementSeqNo(FIRST 0)
-> rate_ee :: EtherEncap(0x9000, $wireless_mac, ff:ff:ff:ff:ff:ff)
-> rate_wifi_encap :: WifiEncap(0x00, 0:0:0:0:0:0)
-> rate_static_rate
-> rate_arf_rate
-> rate_aarf_rate
-> rate_madwifi_rate
-> rate_probe_rate
-> [2] sched;

rate_poke :: PokeHandlers(pause,
                     write rate_inf_src.active true,
                     wait 30,
                     write rate_inf_src.active false,
                     loop);


rate_static_poke :: PokeHandlers(pause,
                     write rate_inf_src.active true,
                     wait 1,
                     write rate_inf_src.active false,
		     loop);


rate_tx_filter[1]
//-> Print(txf, 60)
-> [1] rate_arf_rate [1]
-> [1] rate_aarf_rate [1]
-> [1] rate_madwifi_rate [1]
-> [1] rate_probe_rate [1]
-> FilterFailures()
-> rate_send_count :: Counter()
-> Discard;
EOF

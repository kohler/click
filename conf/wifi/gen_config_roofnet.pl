#!/usr/bin/perl -w

use strict;

use Getopt::Long;



sub usage() {
    print STDERR "usage:
    --dev {e. g. ath0}
    --ssid
    --gateway
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
my $rawdev;
my $ssid;
my $gateway = 0;
my $ap = 0;
my $txf = 1;
my $interval = 10000;
my $rate_control = "static-2";
my $kernel = 0;
GetOptions('device=s' => \$dev,
	   'ssid=s' => \$ssid,
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

my $hostname = `hostname`;
my $wireless_mac = mac_addr_from_dev($dev);
my $suffix;
if ($hostname =~ /rn-pdos(\S+)-wired/) {
    $suffix = "0.0.$1";
} else {
    $suffix = mac_addr_to_ip($wireless_mac);
}



my $srcr_ip = "5." . $suffix;
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

$rawdev = $dev;

if ($dev =~ /ath/) {
    $rawdev = "${dev}raw";
    system "/sbin/sysctl -w dev.$dev.rxfilter=0xff > /dev/null 2>&1";
    system "/sbin/sysctl -w dev.$dev.rawdev_type=2 > /dev/null 2>&1";
    system "/sbin/sysctl -w dev.$dev.rawdev=1 > /dev/null 2>&1";
    system "/sbin/ifconfig $rawdev up";
}

my $iwconfig = "/home/roofnet/bin/iwconfig";

if (-f "/sbin/iwconfig") {
    $iwconfig = "/sbin/iwconfig";
}

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
    system "/sbin/ifconfig $dev txqueuelen 5";
}

my $mode = "b";
my $iwconfig_result = `$iwconfig $dev 2>/dev/null`;
if ($iwconfig_result =~ /Frequency:(\d+)\.(\d+)GHz/) {
    my $channel = "$1.$2";
    if ($channel > 5.0) {
	$mode = "a";
    } elsif ($dev =~ /ath/) {
	$mode = "g";
    }
}

print STDERR "using mode $mode\n";

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


# we need to find srcr.click
# first, try the directory where the script was run from
my $srcr_file = $0;
$srcr_file =~ s/gen_config_roofnet\.pl/srcr\.click/g;

if (! -f $srcr_file) {
    # ok, try srcr.click
    $srcr_file = "/home/roofnet/scripts/srcr.click";
}

if (! -f $srcr_file) {
    die "couldn't find srcr.click: tried $srcr_file\n";
}

print STDERR "using $srcr_file\n";
system "cat $srcr_file";

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
	// we only want txf for NODS packets
	// ether[2:2] == 0x1200 means it has an ath_rx_radiotap header (it is 18 bytes long)
	// ether[2:2] == 0x1000 means it has an ath_tx_radiotap header (it is 16 bytes long)
	// ether[18] == 0x08 means NODS
  from_dev :: FromDevice(\$device, 
			 BPF_FILTER "(ether[2:2] == 0x1200 and ether[18] == 0x08) or (ether[2:2] == 0x1000 and ether[16] == 0x08)",
			 PROMISC \$promisc) 
  -> output;
  input -> to_dev :: ToDevice(\$device);
}

EOF
    }

if (!$kernel) {
    print "control :: ControlSocket(\"TCP\", 7777);\n";
    print "chatter :: ChatterSocket(\"TCP\", 7778);\n";
}

print <<EOF;
sniff_dev :: SniffDevice($rawdev, false);

sched :: PrioSched()
-> set_power :: SetTXPower(POWER 60)
-> radiotap_encap :: RadiotapEncap()
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
			     RT rates,
			     ACTIVE false)

-> [1] sched;

Idle -> [1] data_probe_rate;
Idle -> [1] data_madwifi_rate;
Idle -> [1] data_arf_rate;



srcr :: srcr_ett($srcr_ip, $srcr_nm, $wireless_mac, $gateway, 
		 "$probes");


// make sure this is listed first so it gets tap0
srcr_host :: LinuxIPHost(srcr, $srcr_ip, $srcr_nm)
-> [1] srcr;


route_encap :: WifiEncap(0x0, 00:00:00:00:00:00)
->  route_q;
data_encap :: WifiEncap(0x0, 00:00:00:00:00:00)
-> data_q;

srcr [0] -> route_encap;   // queries, replies
srcr [1] -> route_encap;   // bcast_stats
srcr [2] -> data_encap;    // data
srcr [3] -> srcr_host; // data to me


EOF


print <<EOF;

sniff_dev 
-> prism2_decap :: Prism2Decap()
-> phyerr_filter :: FilterPhyErr()
-> extra_decap :: ExtraDecap()
-> radiotap_decap :: RadiotapDecap()
//-> PrintWifi(fromdev)
-> beacon_cl :: Classifier(0/00%0c 0/80%f0, //beacons
			    -)
-> bs :: BeaconScanner(RT rates)
-> Discard;

beacon_cl [1]
-> Classifier(0/08%0c) //data
-> tx_filter :: FilterTX()
-> dupe :: WifiDupeFilter() 
-> WifiDecap()
-> HostEtherFilter($wireless_mac, DROP_OTHER true, DROP_OWN true) 
-> rxstats :: RXStats()
-> ncl :: Classifier(
		     12/09??,
		     -);


ncl [0] -> srcr;




ncl[1] 
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

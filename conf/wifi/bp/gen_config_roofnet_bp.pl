#!/usr/bin/perl -w
#
# Kostas Choumas
# 
#

use strict;
use Getopt::Long;

### Definition of functions

sub usage() {
    print STDERR "usage:
    --bp {for running Backpressure over WiFi instead of Roofnet}
    --enhanced {for running Enhanced Backpressure}
    --ns {for running with NS-3}
    --id=\%s {REQUIRED by NS-3}
    --src_device=\%s {e.g. eth1}
    --device=\%s {e.g. mon0}
    --monitor
    --base=\%s {e.g. wlan0}
    --ethernet=\%s {e.g. eth0}
    --channel=\%d REQUIRED
    --modulation=\%s
    --rate=\%d
    --power=\%d
    --neighborhood=\%s {1,2 means nodes 1 and 2}
    --timerecords
    --bwshaper=\%d {the maximum rate of the interface, expressed in bytes/sec}
    --slot=\%d REQUIRED {measured in msec}
    --capacity=\%d REQUIRED
    --dmax=\%d
    --V=\%d
    --BLThr=\%d
    --debug
";
    exit 1;
}

sub mac_addr_from_dev($) {
  my $device = $_[0];
  my $output = `ifconfig $device 2>&1`;
  if ($output =~ /^$device: error/) {
    return "";
  }
  my @tmp = split (/\s+/, $output);
  my $mac = $tmp[4];
  $mac =~ s/-/:/g;
  my @hex = split (/:/, $mac);
    
  return uc (join (":", @hex[0 .. 5]));
}

sub mac_addr_from_id($) {    
  my @hex = ("00", "00", "00", "00", "00", "00");
  $hex[5] = sprintf ("%02x", $_[0]);
    
  return uc (join (":", @hex[0 .. 5]));
}

sub get_node_id_from_ip($){
  my $nodeip = $_[0];
  my $output = `ifconfig $nodeip 2>&1`;
  if ($output =~ /^$nodeip: error/) {
    return "";
  }
  my @tmp = split (/\s+/, $output);
  my $myip = $tmp[6];
  my @ip_str = split (/\./, $myip);
  return $ip_str[3];
}

### Definition of variables

my $bp;
my $enhanced; 
my $ns;

my $node_id;
my $src_dev;
my $dev = "mon0";
my $mon;
my $dev_eth = "eth0";
my $dev_base = "wlan0";
my $channel;
my $modulation;
my $rate;
my $power = "0";
my $neigh;
my @neigh_array;
my $neigh_filter = "";
my $timerecords;
my $bwshaper = 0;
my $slot;
my $capacity;
my $dmax = 0;
my $V;
my $BLThr = 0;
my $src_mac;
my $mac;
my $srcr_nprefix = "5.0.0.";
my $srcr_ip;
my $srcr_nmask = "255.255.255.0";
my $debug;

GetOptions('bp' => \$bp,
           'enhanced' => \$enhanced,
           'ns' => \$ns,
           'id=s' => \$node_id,
           'src_device=s' => \$src_dev,
           'device=s' => \$dev,
           'monitor' => \$mon,
	   'ethernet=s' => \$dev_eth,
	   'base=s' => \$dev_base,
	   'channel=i' => \$channel,
	   'modulation=s' => \$modulation,
           'rate=i' => \$rate,
           'power=i' => \$power,
           'neighborhood=s' => \$neigh,
           'timerecords' => \$timerecords,
           'bwshaper=i' => \$bwshaper,
           'slot=i' => \$slot,
           'capacity=i' => \$capacity,
           'dmax=i' => \$dmax,
           'V=i' => \$V,
           'BLThr=i' => \$BLThr,
           'debug' => \$debug,
	   ) or usage();

if (! defined $channel) {
  print STDERR "You should specify the channel!\n";
  usage();
}
elsif ($ns && ! defined $node_id) {
  print STDERR "You should specify the node id in NS-3 simulation!\n";
  usage();
}

if (! defined $slot || ! defined $capacity) {
  print STDERR "You should specify the slot duration and the queue capacity!\n";
  usage();
}

if (! defined $V) {
  $V = $capacity;
}

if ($bp && $enhanced) {
  $enhanced = "true";
  print STDERR ":: The routing protocol is Enhanced Backpressure\n";
}
elsif ($bp) {
  $enhanced = "false";
  print STDERR ":: The routing protocol is Backpressure\n";
}
else {
  print STDERR ":: The routing protocol is Roofnet\n";
}

my @accepted_channels = ( 1 .. 13 , (map { 4 * $_ } 9 .. 35) , (map { 1 + 4 * $_ } 37 .. 41) );
my $channel_num = -1;
foreach (@accepted_channels) {
  if ($_ == $channel) {
    $channel_num = $channel;
  }
}
if ($channel_num == -1) {
  print STDERR "The channel indicator is not valid\n";
  exit 1;
}

if (! defined $modulation) {
  if ($channel_num > 20) {
    $modulation = "a";
  } else {
    $modulation = "b";
  }
} elsif (($modulation =~ /a/ && $channel_num < 20) || (($modulation =~ /b/ || $modulation =~ /g/) && $channel_num > 20)){
  print STDERR ":: Channel $channel_num does not fit with 802.11$modulation\n";
  exit 1;
}

if (! defined $rate) {
  if ($modulation =~ /a/) {
    $rate = 12;
  } else {
    $rate = 2;
  }
}

if (! defined $node_id) {
  $node_id = get_node_id_from_ip ($dev_eth);
}
print STDERR ":: Local node id is $node_id\n";

if (! $mon) {
  $dev_base = $dev;
}
if (! $ns) {
  $mac = mac_addr_from_dev ($dev_base);
  if ($src_dev) {
    $src_mac = mac_addr_from_dev ($src_dev);
  }
} else {
  $mac = mac_addr_from_id ($node_id);
}
if ($mac eq "" or 
  $mac eq "00:00:00:00:00:00") {
  print STDERR "The MAC address is not valid!\n";
  exit -1;
}
print STDERR ":: MAC of node is $mac\n";

$srcr_ip = $srcr_nprefix.$node_id;
print STDERR ":: IP of node is  $srcr_ip\n";
 
print STDERR ":: Channel is $channel and modulation is 802.11$modulation\n";
print STDERR ":: Rate is (${rate}/2)Mbps\n";
print STDERR ":: Power is ${power}mW\n";

my $probes = "2 60 12 60 2 1500 4 1500 11 1500 22 1500 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500";  # modulation g
if ($modulation =~ /a/) {
  #$probes = "12 60 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500 108 1500"; # modulation a
  $probes = "12 60 12 1500 $rate 1500"; # modulation a
} elsif ($modulation =~ /b/) {
  #$probes = "2 60 2 1500 4 1500 11 1500 22 1500"; # modulation b
  $probes = "2 60 2 1500 $rate 1500"; # modulation b
}

my @path = split (/\//, $0);
if ($bp) {
  $path[-1] = "srcr_bp_minimal.click";
}
else {
  $path[-1] = "srcr_minimal.click";
}
my $srcr_file = join ("/", @path);
if (! -f $srcr_file) {
  die "couldn't find $srcr_file\n";
}

if (defined $neigh) {
  @neigh_array = split (/,/, $neigh);
  my @neigh_macs;
  if (! $ns) {
    @neigh_macs = map {"6/".join ("", split (/:/, $_))} @neigh_array;
  }
  else {
    @neigh_macs = map {sprintf ("6/0000000000%02x", $_)} @neigh_array;
  }
  $neigh_filter = join (",", @neigh_macs);
}
else {
  $neigh_array[0] = "all";
}
print STDERR ":: Accepted neighbors are: ",join (", ", @neigh_array),"\n";

if ($debug) {
  $debug = "true";
}
else {
  $debug = "false";
}

### Configuration of interfaces

if (! $ns and $mon) {
  system "modprobe tun > /dev/null 2>&1";
  if (1) { # ath5k or ath9k
    #system "modprobe ath5k > /dev/null 2>&1";
    #system "modprobe ath9k > /dev/null 2>&1";

    system "iw dev $dev del";
    system "iw dev $dev_base interface add $dev type monitor";
    system "ifconfig $dev_base down";
    system "ifconfig $dev mtu 1800";
    #system "ifconfig $dev txqueuelen 1000"; #default is 1000
    system "ifconfig $dev up";
    system "iw dev $dev set channel $channel";
    system "iwconfig $dev txpower $power";
    system "iw reg set XX  > /dev/null 2>&1" ;
  }
  elsif (0) { # madwifi
    #system "modprobe ath_pci > /dev/null 2>&1";

    # Configs for creating athXraw
    $dev = "${dev_base}raw";
    system "sysctl -w dev.$dev_base.rxfilter=0xff > /dev/null 2>&1"; #receive all packets
    system "sysctl -w dev.$dev_base.rawdev_type=2 > /dev/null 2>&1"; #radiotap headers on 802.11 frames
    system "sysctl -w dev.$dev_base.rawdev=1 > /dev/null 2>&1";      #create interface athXraw

    # Configs for athX & athXraw
    system "iwconfig $dev_base channel $channel";
    system "iwconfig $dev txpower $power";
    # The follow 3 commands are useless because don't apply to athXraw...
    #system "sysctl -w dev.$dev.diversity=0 > /dev/null 2>&1";   #disables diversity (uses only the one existing antenna 1)
    #system "sysctl -w dev.$dev.rxantenna=1 > /dev/null 2>&1";   #uses antenna 1 for rx (the connector for antenna 2 is unconnected)
    #system "sysctl -w dev.$dev.txantenna=1 > /dev/null 2>&1";   #uses antenna 1 for tx (the connector for antenna 2 is unconnected)

    system "ifconfig $dev mtu 1800";
    #system "ifconfig $dev txqueuelen 1000"; #default is 1000
    system "ifconfig $dev up";
    system "ifconfig $dev_base down";
  }
}

### Click

print "////Element classes\n\n";
system "cat $srcr_file";
print "\n";

if (! $ns) {
  print <<EOF;
elementclass SniffDevice {
    \$device, \$promisc|

  FromDevice(\$device, PROMISC \$promisc) -> output;
  input -> ToDevice(\$device);
}

elementclass LinuxIPHost {
    \$dev, \$ip, \$nm |

  input -> KernelTun(\$ip/\$nm, MTU 1500, DEVNAME \$dev) //interface to /dev/tun or ethertap (user-level) 
  -> MarkIPHeader() //sets IP header annotation 
  -> CheckIPHeader(CHECKSUM false) //checks IP header
  -> output;
}
EOF
}
else {
  print <<EOF;
elementclass SniffDevice {
    \$device, \$snaplen |

  FromSimDevice(\$device, SNAPLEN \$snaplen) -> output;
  input -> ToSimDevice(\$device);
}

elementclass LinuxIPHost {
    \$device, \$snaplen |

  FromSimDevice(\$device, SNAPLEN \$snaplen)
  -> MarkIPHeader() //sets IP header annotation 
  -> CheckIPHeader(CHECKSUM false) //checks IP header
  -> output;
  
  input
  -> ToSimDevice(\$device, IP)
}
EOF
}
print "\n\n";

if (! $ns) {
  print "////Sockets\n\n";
  print "control :: ControlSocket(\"TCP\", 7780);\n";
  print "chatter :: ChatterSocket(\"TCP\", 7779);\n\n\n";
}

print "////Elements\n\n";

#if ($modulation =~ /a/) {
#  print "rates :: AvailableRates(DEFAULT 12 18 24 36 48 72 96 108, $mac 12 18 24 36 48 72 96 108);\n\n";
#} elsif ($modulation =~ /b/) {
#  print "rates :: AvailableRates(DEFAULT 2 4 11 22, $mac 2 4 11 22);\n\n";
#} else {
#  print "rates :: AvailableRates(DEFAULT 2 4 11 12 18 22 24 36 48 72 96 108, $mac 2 4 11 12 18 22 24 36 48 72 96 108);\n\n";
#}
print "rates :: AvailableRates($mac $rate);\n\n";

if ($bp) {
  print "bpdata :: BPData();\n";
  print "data_q :: DataQueues(IP $srcr_ip, ETH $mac, PERIOD $slot, BPDATA bpdata, LT srcr/lt, ARP srcr/arp, CAPACITY $capacity, BWSHAPER $bwshaper, ENHANCED $enhanced, dmax $dmax, V $V, BLThr $BLThr);\n\n";
}
else {
  print "data_q :: FullNoteQueue($capacity); //FullNoteQueue(10)\n\n";
}

if ($ns) {
  print "dev :: SniffDevice($dev_eth, 4096);\n";
  print "srcr_host :: LinuxIPHost(tap0, 4096);\n";
}
else {
  print "dev :: SniffDevice($dev, ", ($mon?"true":"false"), ");\n";
  print "srcr_host :: LinuxIPHost(srcr, $srcr_ip, $srcr_nmask);\n";
}

if ($bp) {
  print "srcr :: srcr_ett($srcr_ip, $srcr_nmask, $mac, $debug, \"$probes\", $enhanced);\n\n";
}
else {
  print "srcr :: srcr_ett($srcr_ip, $srcr_nmask, $mac, $debug, \"$probes\");\n\n";
}

print "sched :: PrioSched();\n";
print "route_q :: FullNoteQueue($capacity);\n";

if ($src_dev) {
  print "route_tee :: Tee(2);\n";
} else {
  print "route_tee :: Tee(1);\n";
}

print <<EOF;


////Transmission

srcr_host 
EOF
if ($timerecords) {
  print "-> StoreUDPTimeSeqRecord(OFFSET 0, DELTA false)\n";
}
print <<EOF;
-> [1] srcr;

srcr [0] -> route_tee -> route_q; // queries, replies
srcr [1] -> route_tee; // ett_stats
srcr [2] -> data_q; // data packets to go out
EOF
if ($timerecords) {
  print "srcr [3] -> StoreUDPTimeSeqRecord(OFFSET 0, DELTA true) -> [1]data_q[1] -> srcr_host; // data to me\n";
}
else {
  print "srcr [3] -> [1]data_q[1] -> srcr_host; // data to me\n";
}
if ($bp) {
  print "srcr [4] -> route_tee; // bp_stats\n";
}

print <<EOF;

route_q
-> [0] sched;

data_q
-> SetTXRate(RATE $rate, TRIES 8) //802.11 recommends 7 retries
-> [1] sched;

sched
EOF

if ($mon) {
  print <<EOF;
-> WifiEncap(0x0, 00:00:00:00:00:00)
-> RadiotapEncap()
//-> SetTXPower(POWER $power) //Sets the transmit power for a packet
EOF
}

print <<EOF;
-> dev;


////Reception

dev
EOF

if ($mon) {
  print <<EOF;
-> RadiotapDecap() //Pulls the click_wifi_radiotap header from a packet and stores it in Packet::anno()
-> FilterPhyErr() //Filters packets that failed the 802.11 CRC check
-> Classifier(0/08%0c) //Data packets (0/08%0c means "Type" = 10 = Data)
-> FilterTX()          //Filter out wireless transmission feedback packets
-> WifiDupeFilter()    //Filters out duplicate 802.11 packets based on their sequence number
-> WifiDecap()         //Turns 802.11 packets into ethernet packets
-> HostEtherFilter($mac, DROP_OTHER true, DROP_OWN true) //drops Ethernet packets if mac == src or mac != dst
EOF
}
else {
  print "-> setrxrate :: SetTXRate(RATE $rate, TRIES 8) //puts RX rate that is read by ETTStat\n";
}

if (defined $neigh) {
  print "-> MACfilter :: Classifier($neigh_filter) // drops packets that come from other nodes except of accepted neighbors\n";
}

print <<EOF;
-> rxstats :: RXStats()
-> ncl :: Classifier(12/064?) //SRCR packets (12/064? means "Ether_type" = 0x064? = SRCR packet)
-> [0] srcr;
EOF

if (defined $neigh) {
  print "\n";
  print map {sprintf("MACfilter [%s] -> ncl;\n", $_)} 1 .. (@neigh_array - 1);
}

print "\n";

if ($src_dev) {
  print <<EOF;
route_tee[1] 
-> StoreEtherAddress($src_mac, src)
-> src_route_q :: FullNoteQueue($capacity)
EOF
  print "-> src_dev :: SniffDevice($src_dev", ($mon?"true":"false"), ")";
  print "-> setrxrate;";
  print "\n";
}

if ($ns) {
  print <<EOF;


////Routing table

// It is mandatory to use an IPRouteTable element with ns-3-click
// (but we do not use it in this example)
rt :: LinearIPLookup (${srcr_nprefix}0/24 0.0.0.0 1);
// We are actually not using the routing table
Idle () -> rt;
rt[0] -> Discard;
rt[1] -> Discard;
EOF
}

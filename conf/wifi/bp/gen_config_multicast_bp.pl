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
    --id=\%s
    --src_device=\%s {e.g. eth1}
    --dst_device=\%s {e.g. wlan0}
    --src_monitor
    --dst_monitor
    --eth_device=\%s {e.g. eth0}
    --children=\%s {e.g. 5.0.0.240,00:00:00:00:00:01,1,00:00:00:00:00:02,2,,5.0.0.250,00:00:00:00:00:03,3 (flow,mac,weight,...,mac,weight,,new_flow,...)}
    --timerecords
    --bwshaper=\%d {the maximum rate of the interface, expressed in bytes/sec}
    --rates=\%s {in bytes/sec e.g. 5.0.0.240,00:00:00:00:00:01,1000000,,5.0.0.250,00:00:00:00:00:03,2000000 (flow,mac,rate,...,mac,rate,,new_flow,...)}
    --slot=\%d REQUIRED {measured in msec}
    --capacity=\%d REQUIRED
    --dmax=\%d REQUIRED
    --V=\%d REQUIRED
    --policy=\%s
    --epsilon=\%s
    --vmax=\%s
    --zeta=\%s
    --theta=\%s
    --a=\%s
";
    exit 1;
}

sub mac_from_dev($) {
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

sub get_node_id($){
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

my $node_id;
my $src_dev;
my $dst_dev = "wlan0";
my $src_mon;
my $dst_mon;
my $eth_dev = "eth0";
my $children = "no_children";
my $flows_ips = "";
my @children_macs = ();
my $timerecords;
my $bwshaper = 0;
my $rates = "no_rates";
my $slot;
my $capacity;
my $dmax = 0;
my $V;
my $policy = "MMT";
my $epsilon = "0";
my $vmax = "0";
my $zeta = "0";
my $theta = "0";
my $a = "0";
my $stats_period = "1000"; # msecs
my $src_mac;
my $mac;
my $nprefix = "5.0.0.";
my $ip;
my $nmask = "255.255.255.0";

GetOptions('id=s' => \$node_id,
           'src_device=s' => \$src_dev,
           'dst_device=s' => \$dst_dev,
           'src_monitor' => \$src_mon,
           'dst_monitor' => \$dst_mon,
           'eth_device=s' => \$eth_dev,
           'children=s' => \$children,
           'timerecords' => \$timerecords,
           'bwshaper=i' => \$bwshaper,
           'rates=s' => \$rates,
           'slot=i' => \$slot,
           'capacity=i' => \$capacity,
           'dmax=i' => \$dmax,
           'V=i' => \$V,
           'policy=s' => \$policy,
           'epsilon=s' => \$epsilon,
           'vmax=s' => \$vmax,
           'zeta=s' => \$zeta,
           'theta=s' => \$theta,
           'a=s' => \$a,
	   ) or usage();

if (! defined $slot || ! defined $capacity) {
  print STDERR "You should specify the slot duration and the queue capacity!\n";
  usage();
}

if (! defined $V) {
  $V = $capacity;
}

if (! defined $node_id) {
  $node_id = get_node_id ($eth_dev);
}
print STDERR ":: Local node id is $node_id\n";

$mac = mac_from_dev ($dst_dev);
if ($mac eq "" || $mac eq "00:00:00:00:00:00") {
  print STDERR "The MAC address is not valid!\n";
  exit -1;
}
print STDERR ":: MAC of node is $mac\n";
if ($src_dev) {
  $src_mac = mac_from_dev ($src_dev);
}

$ip = $nprefix.$node_id;
print STDERR ":: IP of node is  $ip\n";

if ( !($children =~ /no_children/) ) {
  # At the end, @flows_ips_array contains all flows' ips and 
  # @children_macs is an array of array, while each of the last arrays contains the macs of a specific flow ip
  my @flows = split (/,,/, $children); # flows are separated with double commas
  my @flows_ips_array = ();
  foreach my $flow (@flows) {
    my @flow_macs = split (/,/, $flow); # ip, mac, weight, mac, weight, ...
    my $flow_ip = shift(@flow_macs); # the first element is the flow ip
    push @flows_ips_array, "dst host ".$flow_ip;
    push @children_macs, [(map { $flow_macs[2 * $_] } 0 .. (@flow_macs/2 - 1))]; # keep only the elements with even index, that correspond to macs
    #push @children_macs, [@flow_macs];
  }
  $flows_ips = join ", ", @flows_ips_array;
  print STDERR ":: Children MACs are: ",$children,"\n";
} else {
  $children = "no_weights";
  $flows_ips = "dst host 0.0.0.0";
  @children_macs = (["00:00:00:00:00:00"]);
}

### Click

print <<EOF;
////Sockets

control :: ControlSocket(\"TCP\", 7780);
chatter :: ChatterSocket(\"TCP\", 7779);


////Elements

bpdata :: BPData();
rates :: AvailableRates($mac 2);
bps :: BPStat(ETHTYPE 0x0642, ETH $mac, IP $ip, PERIOD $stats_period, BPDATA bpdata, DQ data_q, RT rates, LAYER eth, ENHANCED false);

sched :: PrioSched();
data_q :: DataQueuesMulticast(IP $ip, ETH $mac, PERIOD $slot, BPDATA bpdata, CAPACITY $capacity, MACLAYER ether, BWSHAPER $bwshaper,
  RATES \"$rates\", WEIGHTS \"$children\", POLICY $policy, dmax $dmax, V $V, epsilon $epsilon, vmax $vmax, zeta $zeta, theta $theta, a $a);
route_q :: FullNoteQueue($capacity);
host :: KernelTun($ip/$nmask, MTU 1500, DEVNAME tun0)


////Transmission

host
-> MarkIPHeader 
-> CheckIPHeader
EOF
if ($timerecords) {
  print "-> StoreUDPTimeSeqRecord(OFFSET 0, DELTA false)\n";
}
print <<EOF;
-> flow :: IPClassifier($flows_ips)
EOF
print "-> tee :: Tee(", scalar @{$children_macs[0]}, ")\n";
print <<EOF;
-> EtherEncap(0x0800, $mac, ${children_macs[0][0]}) //Encaps with an Ethernet header
-> txrate0_0 :: SetTXRate(RATE 2, TRIES 8)
-> data_q
EOF
if ($src_dev) {
  print "-> [0]sched\n";
} else {
  print "-> [1]sched\n";
}
if ($dst_mon) {
  print <<EOF;
-> WifiEncap(0x0, 00:00:00:00:00:00)
-> RadiotapEncap()
EOF
}
print "-> ToDevice($dst_dev);\n\n";

print map {sprintf("tee[%s] -> EtherEncap(0x0800, $mac, %s) -> txrate0_%s :: SetTXRate(RATE 2, TRIES 8) -> data_q;\n", $_, ${children_macs[0][$_]}, $_)} 1 .. (@{$children_macs[0]} - 1);
print "\n";

foreach my $i (1 .. (@children_macs - 1)) {
  my @flow_macs = @{$children_macs[$i]};
  print "flow[", $i, "] -> tee", $i, " :: Tee(", scalar @flow_macs, ")\n";
  print map {sprintf("tee%s[%s] -> EtherEncap(0x0800, $mac, %s) -> txrate%s\_%s :: SetTXRate(RATE 2, TRIES 8) -> data_q;\n", $i, $_, ${children_macs[$i][$_]}, $i, $_)} 0 .. (@{$children_macs[$i]} - 1);
  print "\n";
}

print <<EOF;
bps
-> route_q
EOF
if ($src_dev) {
  print "-> StoreEtherAddress($src_mac, src)\n";
  if ($src_mon) {
    print <<EOF;
-> WifiEncap(0x0, 00:00:00:00:00:00)
-> RadiotapEncap()
EOF
  }
  print "-> ToDevice($src_dev)\n";
} else {
  print "-> [0]sched\n";
}

print "\n\n";
print "////Reception\n\n";
print "FromDevice($dst_dev, PROMISC ", ($dst_mon?"true":"false"), ")\n";

if ($dst_mon) {
  print <<EOF;
-> RadiotapDecap() //Pulls the click_wifi_radiotap header from a packet and stores it in Packet::anno()
-> FilterPhyErr() //Filters packets that failed the 802.11 CRC check. 
-> Classifier(0/08%0c) //Data packets (0/08%0c means "Type" = 10 = Data)
-> filter :: FilterTX()   //Filter out wireless transmission feedback packets 
-> WifiDupeFilter()    //Filters out duplicate 802.11 packets based on their sequence number. 
-> WifiDecap()         //Turns 802.11 packets into Ethernet packets.
-> HostEtherFilter($mac, DROP_OTHER true, DROP_OWN true) //Drops Ethernet packets if mac == src or mac != dst
EOF
}

print <<EOF;
-> type :: Classifier(12/0800, 12/0642) //Specific packets of our type
-> Strip(14)
-> MarkIPHeader
-> CheckIPHeader
-> duplicate :: Tee(2)
-> IPClassifier(dst host $ip)
EOF
if ($timerecords) {
  print "-> StoreUDPTimeSeqRecord(OFFSET 0, DELTA true)\n";
}
print <<EOF;
-> [1]data_q[1]
-> host;

type[1] -> bps;

duplicate[1] -> flow;

EOF

if ($dst_mon) {
  print <<EOF;
filter[1] -> filter2 :: FilterFailures -> Discard;
filter2[1] -> [2]data_q[2] -> Discard;
EOF
}

if ($src_dev) {
  print "FromDevice($src_dev, PROMISC ", ($src_mon?"true":"false"), ")\n";
  if ($src_mon) {
    print <<EOF;
-> RadiotapDecap() //Pulls the click_wifi_radiotap header from a packet and stores it in Packet::anno()
-> FilterPhyErr() //Filters packets that failed the 802.11 CRC check. 
-> Classifier(0/08%0c) //Data packets (0/08%0c means "Type" = 10 = Data)
-> FilterTX()          //Filter out wireless transmission feedback packets 
-> WifiDupeFilter()    //Filters out duplicate 802.11 packets based on their sequence number. 
-> WifiDecap()         //Turns 802.11 packets into Ethernet packets.
-> HostEtherFilter($src_mac, DROP_OTHER true, DROP_OWN true) //Drops Ethernet packets if mac == src or mac != dst
EOF
  }
  print "-> type;\n";
}


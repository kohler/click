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
    --id=\%d
    --mac=\%s
    --device=\%s {e.g. mon0}
    --base=\%s {e.g. wlan0}
    --ethernet=\%s {e.g. eth0}
    --channel=\%d REQUIRED
    --modulation=\%s
    --rate=\%d
    --power=\%d
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

my $node_id;
my $dev = "mon0";
my $dev_eth = "eth0";
my $dev_base = "wlan0";
my $channel;
my $modulation;
my $rate;
my $power = "0";
my $capacity = "100";
my $mac;
my $srcr_nprefix = "5.0.0.";
my $srcr_ip;
my $srcr_nmask = "255.255.255.0";

GetOptions('id=s' => \$node_id,
           'device=s' => \$dev,
	   'ethernet=s' => \$dev_eth,
	   'base=s' => \$dev_base,
	   'channel=s' => \$channel,
	   'modulation=s' => \$modulation,
           'rate=s' => \$rate,
           'power=s' => \$power,
           'mac=s' => \$mac,
	   ) or usage();

if (! defined $channel) {
  print STDERR "You should specify the channel!\n";
  usage();
}

my @accepted_channels = ( 1 .. 13 , (map { 4 * $_ } 9 .. 35) , (map { 1 + 4 * $_ } 37 .. 41) );
my $channel_num = -1;
foreach (@accepted_channels) {
  if ($_ == int($channel)) {
    $channel_num = int($channel);
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
    $rate = "12";
  } else {
    $rate = "2";
  }
}

if (! defined $node_id) {
  $node_id = get_node_id_from_ip ($dev_eth);
}
print STDERR ":: Local node id is $node_id\n";

if (! defined $mac) {
  $mac = mac_addr_from_dev ($dev_base);
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
print STDERR ":: Rate is ($rate/2)Mbps\n";
print STDERR ":: Power is ${power}mW\n";

### Configuration of interfaces

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

### Click

print <<EOF;
////Element classes

elementclass SniffDevice {
    \$device, \$promisc|

  FromDevice(\$device, PROMISC \$promisc) -> output;
  //input -> ToDevice(\$device);
}

elementclass LinuxIPHost {
    \$dev, \$ip, \$nm |

  input -> KernelTun(\$ip/\$nm, MTU 1500, DEVNAME \$dev) //interface to /dev/tun or ethertap (user-level) 
  -> Discard;
  //-> MarkIPHeader() //sets IP header annotation 
  //-> CheckIPHeader(CHECKSUM false) //checks IP header
  //-> output;
}


////Sockets

control :: ControlSocket(\"TCP\", 7780);
chatter :: ChatterSocket(\"TCP\", 7779);


////Elements

sniff_dev :: SniffDevice($dev, true);
srcr_host :: LinuxIPHost(srcr, $srcr_ip, $srcr_nmask);


////Reception

sniff_dev
-> RadiotapDecap() //Pulls the click_wifi_radiotap header from a packet and stores it in Packet::anno()
-> FilterPhyErr() //Filters packets that failed the 802.11 CRC check. 
-> Classifier(0/08%0c) //Data packets (0/08%0c means "Type" = 10 = Data)
-> FilterTX()          //Filter out wireless transmission feedback packets 
-> WifiDupeFilter()    //Filters out duplicate 802.11 packets based on their sequence number. 
-> WifiDecap()         //Turns 802.11 packets into ethernet packets.
-> rxstats :: RXStats()
-> HostEtherFilter($mac, DROP_OTHER true, DROP_OWN true) //drops Ethernet packets if mac == src or mac != dst
-> ncl :: Classifier(12/0643) //SRCR Data packets (12/0643 means "Ether_type" = 0x0643 = SRCR Data packet)
-> CheckSRHeader()
-> StripSRHeader()
-> CheckIPHeader(CHECKSUM false)
-> IPClassifier(dst host $srcr_ip)
-> srcr_host;
EOF


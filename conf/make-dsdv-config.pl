#!/usr/bin/perl -w

# script to generate DSDV configurations for Click.
# Douglas S. J. De Couto
# 8 August 2003

use strict;

my $prog = "make-dsdv-config.pl";

# routing parameters, times in milliseconds
my $rt_timeout = 60000;
my $rt_period  = 15000;
my $rt_jitter  =  7500;
my $rt_min_period = 1000;
my $rt_hops = 100; 

my $dsdv_config = "";  # extra DSDVRouteTable element configuration arguments

my $probe_size = 148; # total bytes, including ethernet header
my $probe_size2 = 34; # optional second probe size: min size is 34 bytes

my @allowable_metrics = ("etx", "etx2", "hopcount", "lir", "thresh", "e2eloss", "yarvis");
my $metric = "hopcount";
my $metric_config = "";  # extra metric element configuration arguments
my $ls_config = "";      # extra linkstat element configuration arguments

sub usage() {
    print "usage: $prog -i IF -a ADDR [OPTIONS]
  Generate a Click DSDV configuration to run on interface IF with IP address ADDR.

Options:
  Run in kernel or at userlevel?  Defaults to kernel.
   -k, --kernel        Run in kernel.  Only works on Linux.
   -u, --userlevel     Run in userlevel

   -i, --interface IF  Use interface IF.  Required option.
   -a, --address A     Use IP address A.  Required option.  

   --ether ETH         Use Ethernet address ETH for interface.  If not specified, the address
                        will be dynamically discovered. 

  Optional DSDV parameters.
   --timeout T         Expire stale route entries after T milliseconds.  Defaults to $rt_timeout.
   --period P          Send `full dump' every P milliseconds.  Defaults to $rt_period.
   --max-jitter J      Jitter route ad timing up to J milliseconds.  Defaults to $rt_jitter.
   --min-period M      Don't send route ads more than once every M milliseconds.  Defaults to $rt_min_period.
   --max-hops H        Only propagate routes for H hops.  Defaults to $rt_hops.
   --dsdv-config C     Pass extra configuration string C to DSDVRouteTable element.
   --metric M          Use metric M.  
      Possible metrics are ";
    print join ", ", @allowable_metrics;
print ".  Defaults to $metric.
   --metric-config C   Pass extra configuration string C to metric element.
   --probe-size N      Use N-byte link probes.  Ignored for hopcount metric.  Defaults to $probe_size.
   --force-probes      Send link probes even if the metric doesn't require them.
   --linkstat-config C  Pass extra configuration string C to LinkStat elements.

   --click CLICK       Location of userlevel click executable.  If specified, will be used to 
                        dynamically determine protocol offsets and sizes.

   -h, --help          Print this message and exit.
";
}

my @orig_args = @ARGV;

foreach my $arg (@ARGV) { if ($arg eq "-h" || $arg eq "--help") { usage(); exit(0); } }

sub bail($) {
    print STDERR "$prog: ", shift, "\n";
    exit 1;
}

sub get_arg() {
    my $arg = shift @ARGV;
    if (!defined($arg)) {
	print STDERR "$prog: Missing argument parameter\n";
	usage();
	exit 1;
    }
    return $arg;
}

my $ifname = "";
my $click = "";
my $in_kernel = 1;
my $addr = "";
my $eth = "";
my $force_probes = 0;

while (scalar(@ARGV) > 0) {
    my $arg = shift @ARGV;
    if ($arg eq "--help" || $arg eq "-h") {
	usage();
	exit 0;
    }
    elsif ($arg eq "--click") {
	$click = get_arg();
    }

    elsif ($arg eq "--interface" || $arg eq "-i") {
	$ifname = get_arg();
    }
    elsif ($arg eq "--address" || $arg eq "-a") {
	$addr = get_arg();
    }
    elsif ($arg eq "--ether") {
	$eth = get_arg();
    }
    elsif ($arg eq "--kernel" || $arg eq "-k") {
	$in_kernel = 1;
    }
    elsif ($arg eq "--userlevel" || $arg eq "-u") {
	$in_kernel = 0;
    }
    elsif ($arg eq "--force-probes") {
	$force_probes = 1;
    }
    # DSDV params
    elsif ($arg eq "--timeout") {
	$rt_timeout = get_arg();
    }
    elsif ($arg eq "--period") {
	$rt_period = get_arg();
    }
    elsif ($arg eq "--max-jitter") {
	$rt_jitter = get_arg();
    }
    elsif ($arg eq "--min-period") {
	$rt_min_period = get_arg();
    }
    elsif ($arg eq "--max-hops") {
	$rt_hops = get_arg();
    }
    elsif ($arg eq "--dsdv-config") {
	$dsdv_config = get_arg();
    }
    elsif ($arg eq "--probe-size") {
	$probe_size = get_arg();
    }
    elsif ($arg eq "--metric") {
	$metric = get_arg();
    }
    elsif ($arg eq "--metric-config") {
	$metric_config = get_arg();
    }
    elsif ($arg eq "--linkstat-config") {
	$ls_config = get_arg();
    }
    else {
	print STDERR "$prog: Unknown argument `$arg'\n";
	usage();
	exit 1;
    }
}

if ($ifname eq "") { bail("No interface specified, try --help for more info."); }
if ($addr eq "") { bail("No IP address specified, try --help for more info"); }

if ($addr !~ /\d+\.\d+\.\d+\.\d+/) { 
    bail("IP address `$addr' has a bad format (should be like `a.b.c.d')"); 
}

if ($eth ne "" && $eth !~ /([a-f0-9][a-f0-9]?:){5}[a-f0-9][a-f0-9]?/i) {
    bail("Ethernet address `$eth' has a bad format"); 
}

sub check_param($$$) {
    my $n = shift;
    my $v = shift;
    my $min = shift;
    if ($v =~ /(\-?\d+)/) { 
	$v = $1; 
	if ($v < $min) {
	    bail("Argument to --$n must be >= $min");
	}
    }
    else {
	bail("Argument to --$n must be numeric");
    }
}

check_param("timeout", $rt_timeout, 0);
check_param("period", $rt_period, 0);
check_param("max-jitter", $rt_jitter, 0);
check_param("min-period", $rt_min_period, 0);
check_param("max-hops", $rt_hops, 0);
check_param("probe-size", $probe_size, 20 + 14); 

my $metric_type_ok = 0;
foreach my $m (@allowable_metrics) {
    if ($metric eq $m) { $metric_type_ok = 1; }
}
if (!$metric_type_ok) {
    bail("Unknown argument `$metric' to --metric; should be one of " . join ", ", @allowable_metrics);
}


my %default_proto_info = ("offsetof_grid_hdr_type" => 5,
			  "sizeof_grid_hdr"        => 60,
			  "sizeof_grid_nbr_encap"  => 8);

sub get_proto_info($) {
    my $n = shift;
    my $ret = "";
    if ($click ne "") {
	$ret = `$click -q -e 'g::GridHeaderInfo' -h g.${n}`;
	if ($ret =~ /(\d+)/) { return $1; }
	else { bail("Can't get value for $n from Click"); }
    }
    else {
	$ret = $default_proto_info{$n};
	if (!defined($ret)) { bail("Don't have protocol info for `$n'"); }
	return $ret;
    }
}

# dynamically determine some of the configuration parameters, since
# they depend on protocol header layouts, which have been, ahem, known
# to change with time...

my $offset_grid_proto = 14 + get_proto_info("offsetof_grid_hdr_type");
my $sizeof_grid_hdr = get_proto_info("sizeof_grid_hdr");
my $sizeof_grid_nbr_encap = get_proto_info("sizeof_grid_nbr_encap");
my $offset_encap_ip = 14 + $sizeof_grid_hdr + $sizeof_grid_nbr_encap;
my $tun_input_headroom = $sizeof_grid_hdr + $sizeof_grid_nbr_encap;
my $tun_mtu = 1500 - $sizeof_grid_hdr - $sizeof_grid_nbr_encap;

my $now = `date`;
chomp $now;

# setup parts of configuration that differ between userlevel & kernel
my $addrinfo = "";
my $tosniffers = "";
if ($in_kernel) {
    my $ethspec = ($eth ne "") ? $eth : "$ifname:eth";
    $addrinfo = "AddressInfo(me $addr $ethspec)";
    $tosniffers = "ToHostSniffers(\$dev)";
}
else {
    if ($eth eq "") {
	my $ifconfig_out = `ifconfig $ifname`;
	if ($ifconfig_out =~ /(\S\S:\S\S:\S\S:\S\S:\S\S:\S\S)/) {
	    $eth = $1;
	    print STDERR "$prog: Using ethernet address $eth\n";
	}
	else {
	    bail("Unable to discover ethernet address for interface `$ifname'; try using the --ether option");
	}
    }
    $addrinfo = "AddressInfo(me $addr $eth)";
    $tosniffers = "Discard";
}


# construct metric element part of configuration
my $metric_el = "";
if ($metric_config !~ /\S+/) { $metric_config = ""; } # convert all-white-space config to empty string
my $comma_metric_config = ($metric_config eq "") ? "" : ", $metric_config"; # don't prepend empty string with comma

if ($ls_config !~ /\S+/) { $ls_config = ""; } 
my $comma_ls_config = ($ls_config eq "") ? "" : ", $ls_config"; 

my $two_probe_sizes = 0; # some metrics want two LinkStats with different size packets

my $probeswitcharg = -1; # disable probes by default

if ($metric eq "etx") {
    $metric_el = "ETXMetric(ls $comma_metric_config)";
    $probeswitcharg = 0;
}
elsif ($metric eq "etx2") {
    $metric_el = "ETX2Metric(ls, ls2  $comma_metric_config)";
    $probeswitcharg = 0;
    $two_probe_sizes = 1;
}
elsif ($metric eq "hopcount") {
    $metric_el = "HopcountMetric($metric_config)";
}
elsif ($metric eq "thresh") {
    $metric_el = "ThresholdMetric(ls $comma_metric_config)";
    $probeswitcharg = 0;
}
elsif ($metric eq "e2eloss") {
    $metric_el = "E2ELossMetric(ls $comma_metric_config)";
    $probeswitcharg = 0;
}
elsif ($metric eq "yarvis") {
    $metric_el = "YarvisMetric(ls $comma_metric_config)";
    $probeswitcharg = 0;
}
elsif ($metric eq "lir") {
    $metric_el = "LIRMetric(nb $comma_metric_config)";
}
else {
    die;
}

if ($force_probes) {
    $probeswitcharg = 0;
}

my $ls2_element = "Idle";
if ($two_probe_sizes) {
    $ls2_element = "LinkStat(ETH me:eth, SIZE $probe_size2, USE_SECOND_PROTO true $comma_ls_config)";
}


if ($dsdv_config !~ /\S+/) { $dsdv_config = ""; } 
my $comma_dsdv_config = ($dsdv_config eq "") ? "" : ", $dsdv_config"; 

print "
// This file automatically generated at $now with the following command:
// $prog @orig_args

// this configuration performs routing lookup *after* the interface
// queue, and only works with one interface.

$addrinfo;

elementclass TTLChecker {
  // expects grid packets with MAC headers --- place on output path to
  // decrement the IP TTL for next hop and provide traceroute support.  
  // 
  // push -> push 
  // 
  // output [0] passes through the Grid MAC packets 
  // 
  // output [1] produces ICMP error packets to be passed back to IP
  // routing layer 
 
  input -> cl :: Classifier($offset_grid_proto/03, -);
  cl [1] -> output; // don't try to dec ttl for non-IP packets...

  cl [0] 
    -> MarkIPHeader($offset_encap_ip) 
    -> cl2 :: IPClassifier(src host != me, -);

  cl2 [0]-> dec :: DecIPTTL; // only decrement ttl for packets we don't originate
  cl2 [1] -> output; 

  dec [0] -> output;
  dec [1] -> ICMPError(me, 11, 0) -> [1] output;
};

li :: GridLocationInfo2(0, 0, LOC_GOOD false);

elementclass FixupGridHeaders {
  \$li | // LocationInfo element
  input  
    -> FixSrcLoc(\$li)
    -> SetGridChecksum
    -> output;
};

elementclass ToGridDev {
  // push, no output
  \$dev |
  input -> cl :: Classifier(12/7ffe, // LinkStat 1
                            12/7ffd, // LinkStat 2
			    $offset_grid_proto/02,
			    $offset_grid_proto/03);
  prio :: PrioSched;
  cl [0] -> probe_counter :: Counter -> probe_q :: Queue(5) -> [0] prio;
  cl [1] -> probe_counter;
  cl [2] -> route_counter :: Counter -> route_q :: Queue(5) -> FixupGridHeaders(li) -> [1] prio;
  cl [3] ->  data_counter :: Counter ->  data_q :: Queue(5)  
    -> data_counter_out :: Counter
    -> tr :: TimeRange
    -> lr :: LookupLocalGridRoute2(me:eth, me:ip, nb) 
    -> FixupGridHeaders(li)
    -> data_counter_out2 :: Counter
    -> tr2 :: TimeRange
    -> [2] prio;
  prio
    -> dev_counter :: Counter
    -> t :: PullTee 
    -> ToDevice(\$dev);
  t [1] -> SetTimestamp -> $tosniffers;
};

elementclass FromGridDev {
  // push, no input
  // `Grid' packets on first output
  // `LinkStat' packets on second output
  \$dev, \$mac |
  FromDevice(\$dev, PROMISC false) 
    -> t :: Tee 
    -> HostEtherFilter(\$mac, DROP_OWN true)
    -> cl :: Classifier(12/7fff, 12/7ffe, 12/7ffd, -);
  cl [0]  // `Grid' packets
    -> ck :: CheckGridHeader
    -> [0] output;
  cl [1]  // `LinkStat 1' packets
    -> [1] output;
  cl [2]  // `LinkStat 2' packets
    -> [1] output;
  cl [3] // everything else
    -> [2] output;
  t [1] -> $tosniffers;
  ck [1] -> Print('Bad Grid header received', TIMESTAMP true, NBYTES 166) -> Discard;
};

elementclass GridLoad {
  // push, no input 

  // DATASIZE should be the size of the desired UDP packet (including
  // ethernet, Grid, and IP headers), plus 2 for alignment.  It must
  // be at least 120.  Most of this is stripped off to be re-used
  // later, avoiding expensive pushes in the UDP/IP and Grid
  // encapsulation.
  src :: InfiniteSource(ACTIVE false, DATASIZE 120)
    -> Strip(112) // 14 + 60 + 8 + 20 + 8 + 2 = 112 
                  // (eth + grid + grid_encap + ip + udp + 2 for alignment)
    -> seq :: IncrementSeqNo(FIRST 0, OFFSET 0)
    -> SetIPAddress(me)
    -> StoreIPAddress(4)
    -> udp :: UDPIPEncap(me, 1111, 0.0.0.0, 8021)
    -> count :: Counter
    -> tr :: TimeRange
    -> output;
}

ls2 :: $ls2_element;
ls :: LinkStat(ETH me:eth, SIZE $probe_size $comma_ls_config);
metric :: $metric_el;

nb :: DSDVRouteTable($rt_timeout, $rt_period, $rt_jitter, $rt_min_period,
		     me:eth, me:ip, 
		     MAX_HOPS $rt_hops,
                     METRIC metric,
		     VERBOSE false
                     $comma_dsdv_config   
                     );

grid_demux :: Classifier($offset_grid_proto/03,    // encapsulated (data) packets
			 $offset_grid_proto/02);   // route advertisement packets

arp_demux :: Classifier(12/0806 20/0001, // arp queries
			12/0800);        // IP packets

// handles IP packets with no extra encapsulation
ip_demux :: IPClassifier(dst host me,    // ip for us
			 dst net me/24); // ip for Grid network

// handles IP packets with Grid data encapsulation
grid_data_demux :: IPClassifier(dst host me,    // ip for us
				dst net me/24); // ip for Grid network

// dev0
dev0 :: ToGridDev($ifname);
from_dev0 :: FromGridDev($ifname, me:eth) 
from_dev0 [0] -> Paint(0) -> grid_demux
from_dev0 [1] -> Paint(0) -> probe_cl :: Classifier(12/7ffe, 12/7ffd);

probe_cl [0] -> ls ->  probe_switch :: Switch($probeswitcharg) -> dev0;
probe_cl [1] -> ls2 -> probe_switch;

// support for traceroute
dec_ip_ttl :: TTLChecker -> dev0;
dec_ip_ttl [1] -> ip_demux;

grid_demux [0] -> CheckIPHeader( , $offset_encap_ip) -> grid_data_demux;
grid_demux [1] -> nb -> dev0;

ip_input :: CheckIPHeader -> GetIPAddress(16) -> ip_demux;
";

# setup physical wireless interfaces and Linux interfaces
if ($in_kernel) {
    print "
to_host :: ToHost(grid0);
to_host_encap :: EtherEncap(0x0800, 1:1:1:1:1:1, 2:2:2:2:2:2) -> to_host; 
from_host :: FromHost(grid0, me/24) -> arp_demux -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> to_host;
arp_demux [1] -> Strip(14) -> ip_input;
from_dev0 [2] -> ToHost(me);
";
}
else {
    print ";
to_host_encap :: KernelTun(me/24, HEADROOM $tun_input_headroom, MTU $tun_mtu) -> ip_input;

// not needed in userlevel
Idle -> arp_demux [0] -> Idle;
arp_demux [1] -> Idle;

from_dev0 [2] -> Discard;

ControlSocket(tcp, 7777);
";
}

print "
ip_demux [0] -> to_host_encap;  // loopback packet sent by us, required on BSD userlevel
ip_demux [1] -> GridEncap(me:eth, me:ip) -> dec_ip_ttl;   // forward packet sent by us

grid_data_demux [0] -> Strip($offset_encap_ip) -> to_host_encap;  // receive packet from net for us  
grid_data_demux [1] -> dec_ip_ttl;                                // forward packet from net for someone else


// UDP packet generator
load :: GridLoad -> ip_input;
";

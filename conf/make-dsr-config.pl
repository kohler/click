#!/usr/bin/perl -w

# script to generate DSR configurations for Click.
# Daniel Aguayo
# 2003

use strict;

my $prog = "make-dsr-config.pl";

my $probe_size = 148; # total bytes, including ethernet header

my @allowable_metrics = ("ETX", "hopcount");
my $metric = "hopcount";

sub usage() {
    print "usage: $prog -i IF -a ADDR [OPTIONS]
  Generate a Click DSR configuration to run on interface IF with IP address ADDR.

Options:
  Run in kernel or at userlevel?  Defaults to kernel.
   -k, --kernel        Run in kernel.  Only works on Linux.
   -u, --userlevel     Run in userlevel

   -i, --interface IF  Use interface IF.  Required option.
   -a, --address A     Use IP address A.  Required option.  

   --ether ETH         Use Ethernet address ETH for interface.  If not specified, 
                        the address will be dynamically discovered. 

  Optional DSR parameters.
   --blacklist         Use DSR link blacklist.  Enabled by default.
   --no-blacklist      Don't use DSR link blacklist.
   --feedback          Use TX failure feedback to trigger route queries.  
                        Requires running in kernel.
                        Enabled by default in kernel configurations.
                        Disabled by default in userlevel configurations.
   --airo              Use AironetTXFeedback instead of WifiTXFeedback
   --no-feedback       Don't use TX failure feedback.  Implied by --userlevel.
   --metric M          Use metric M.  Allowable values are ";
    print join ", ", @allowable_metrics;
print ".  Defaults to $metric.
   --probe-size N      Use N-byte link probes.  Ignored for hopcount metric.  
                        Defaults to $probe_size.
   --force-probes      Send link probes even if the metric doesn't require them.

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
my $in_kernel = 1;
my $addr = "";
my $eth = "";
my $force_probes = 0;
my $use_blacklist = 1;
my $use_feedback = 1;
my $request_use_feedback = 0;
my $request_no_feedback = 0;
my $airo = 0;

while (scalar(@ARGV) > 0) {
    my $arg = shift @ARGV;
    if ($arg eq "--help" || $arg eq "-h") {
	usage();
	exit 0;
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
    elsif ($arg eq "--airo") {
	$airo = 1;
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
    # DSR params
    elsif ($arg eq "--blacklist") {
	$use_blacklist = 1;
    }
    elsif ($arg eq "--no-blacklist") {
	$use_blacklist = 0;
    }
    elsif ($arg eq "--feedback") {
	$request_use_feedback = 1;
	$use_feedback = 1;
    }
    elsif ($arg eq "--no-feedback") {
	$request_no_feedback = 1;
	$use_feedback = 0;
    }
    elsif ($arg eq "--probe-size") {
	$probe_size = get_arg();
    }
    elsif ($arg eq "--metric") {
	$metric = get_arg();
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

if ($request_use_feedback && !$in_kernel) {
    bail("Can't use TX feedback at userlevel");
}

if (!$in_kernel) { 
    $use_feedback = 0;
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

check_param("probe-size", $probe_size, 20 + 14); 

my $metric_type_ok = 0;
foreach my $m (@allowable_metrics) {
    if ($metric eq $m) { $metric_type_ok = 1; }
}
if (!$metric_type_ok) {
    bail("Unknown argument `$metric' to --metric; should be one of " . join ", ", @allowable_metrics);
}


my $netmask = 24;

my $datasize = 134;
my $data = sprintf "'%0${datasize}d'", 0;

print "AddressInfo(me0 $addr);\n";
if ($in_kernel) {
    my $ethspec = ($eth ne "") ? $eth : "$ifname:eth";
    print "AddressInfo(my_ether $ethspec);\n";
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
    print "AddressInfo(my_ether $eth);\n";
}

print "
rt_q2 :: SimpleQueue(10); // just ahead of todevice

dsr_ls :: LinkStat(ETH my_ether, SIZE $probe_size) -> rt_q0 :: Queue(5);

dsr_lt :: LinkTable(IP me0);
";

if ($metric eq "ETX") {
    print "dsr_rt :: DSRRouteTable(me0, dsr_lt, OUTQUEUE rt_q2, USE_BLACKLIST $use_blacklist, LINKSTAT dsr_ls);\n";
} 
elsif ($metric eq "hopcount") {
    print "dsr_rt :: DSRRouteTable(me0, dsr_lt, OUTQUEUE rt_q2, USE_BLACKLIST $use_blacklist);\n";
}
else { 
    die; 
}

print "
dsr_arp :: DSRArpTable(me0, my_ether);

in1 :: FromDevice(${ifname}, PROMISC 0);

dsr_filter :: HostEtherFilter(my_ether,1);


in_cl :: Classifier(12/7FFF, -);
in_cl[0] -> dsr_ls;
";

if ($in_kernel) {
    print "
// [2]dsr_arp takes incoming packets, and passes them through
// unchanged to output 2, adding entries to an ARP table

// in the kernel we need to explicitly copy packets going to and
// from the device to hostsniffers.

in1 -> // Print(_in, NBYTES 192) -> 
          in_t :: Tee(2);

in_t[0] -> in_cl;
in_t[1] -> ToHostSniffers(${ifname});

in_cl[1] -> dsr_filter; // non-probes -- XXX does this need to come after the hostetherfilter?

// respond to ARP requests from the kernel with a bogus MAC
from_host :: FromHost(dsr0, me0/$netmask) -> bs_t :: Tee(2);

// packets destined for this host
dsr_rt[0] -> // Print(_th) -> 
             CheckIPHeader -> 
             setup_cl :: IPClassifier(udp port 8022, -);

setup_cl[0] -> Print(setup) -> Discard;
setup_cl[1] -> EtherEncap(0x0800, my_ether, my_ether) ->
               ToHost(dsr0);

bs_t[0] -> arp_cl :: Classifier(12/0806, 12/0800);
bs_t[1] -> Discard; // dodge wmem_alloc bug

arp_cl[0] -> ARPResponder(0.0.0.0/0 1:1:1:1:1:1) -> 
             ToHost;

// IP packets from the kernel
arp_cl[1] -> // Print(_fh) ->
             Strip(14) ->
             CheckIPHeader ->
             MarkIPHeader ->
             GetIPAddress(16) ->
             [0]dsr_rt;

// (again) in the kernel we need to explicitly copy packets going to
// and from the device to hostsniffers.

ls_prio :: PrioSched -> out_pt :: PullTee(2);

out_pt[0] -> ToDevice(${ifname});
out_pt[1] -> ToHostSniffers(${ifname});

";
} 
else { 
    print "
cs::ControlSocket(TCP, 7777);

// [1]dsr_arp takes incoming packets, and passes them through
// unchanged to output 1, adding entries to an ARP table

in1 -> in_cl;
in_cl[1] -> dsr_filter; // non-probes

// packets destined for this host

kt :: KernelTun(me0/$netmask);

kt -> icmp_cl :: Classifier(20/0302, -);

icmp_cl[0] -> Discard; // icmp 'protocol not supported'
icmp_cl[1] -> IPPrint(0rt, CONTENTS true, NBYTES 128) -> 
[0]dsr_rt[0] -> CheckIPHeader -> 
                IPPrint(rt0, CONTENTS true, NBYTES 128) -> 
                setup_cl :: IPClassifier(udp port 8022, -);

setup_cl[0] -> Print(setup) -> Discard;
setup_cl[1] -> kt;

ls_prio :: PrioSched -> ToDevice(${ifname});

";
}


print "
    
dsr_filter[0] -> CheckIPHeader(14) -> [2]dsr_arp;

// drop packets with my ethernet source address
dsr_filter[1] -> // Print(Mine) ->
                 Discard;

dsr_arp[2] -> Print(_in, NBYTES 192) ->
              Strip(14) ->
              IPPrint ->
              CheckIPHeader() ->
              MarkIPHeader() ->
              GetIPAddress(16) ->
              DSR_class :: Classifier(09/C8, -);  // DSR packets

DSR_class[0] -> // Print(DSR) ->
                [1]dsr_rt;
DSR_class[1] -> // Print(Other) ->
                Discard;


// packets to send out on the wireless; dsr_arp puts on the ethernet
// header based on its ARP table
td_prio :: PrioSched;
dsr_rt[1] -> rt_q1 :: Queue(20) -> [0]td_prio;
dsr_rt[2] -> rt_q2 -> [1]td_prio;
td_prio -> [0]dsr_arp;

Idle -> [1] dsr_arp [1] -> Idle;

";

if ($metric eq "ETX" || $force_probes) {
  print "rt_q0 -> [0]ls_prio;\n";
} 
else {
  print "Idle -> [0]ls_prio;\n";
  print "rt_q0 -> Discard;\n";
}

print "
dsr_arp[0] -> Print(out, NBYTES 192) ->
              [1]ls_prio;

// packet spewer for throughput tests.
spew :: RatedSource(ACTIVE false, RATE 700, DATA $data)
        -> Strip(42) // 14 + 20 + 8 = 42
                     // (eth + ip + udp)
        -> seq :: IncrementSeqNo(FIRST 0, OFFSET 0)
        -> SetIPAddress(me0)
        -> StoreIPAddress(4)
        -> udp :: UDPIPEncap(me0, 1111, 0.0.0.0, 8021)
        -> CheckIPHeader 
        -> GetIPAddress(16) 
        -> [0]dsr_rt;

// setup the DSR source route
setup :: RatedSource(ACTIVE false, RATE 1, DATA 'xxx')
        -> SetIPAddress(me0)
        -> StoreIPAddress(4)
        -> udp2 :: UDPIPEncap(me0, 1111, 0.0.0.0, 8022)
        -> CheckIPHeader 
        -> GetIPAddress(16) 
        -> [0]dsr_rt;

poke :: Script(pause,
		     write setup.active true,
		     wait 5,
		     write setup.active false,
		     wait 5,
                     write spew.active true,
		     wait 30,
                     write spew.active false,
                     loop);

setup_poke :: Script(pause, write setup.active true, wait 5, write setup.active false, loop);

";

if ($in_kernel && $use_feedback) { 

if ($airo) {
  print "
// tx feedback
airo_fb :: AironetTXFeedback -> 
           MSQueue -> 
           Unqueue -> 
           Classifier(! 0/FFFFFFFFFFFF) -> // throw out broadcasts
           fbh :: FeedbackHandler;

// packets which had a transmission failure
fbh[0] -> Print(txf) -> Strip(14) ->
          CheckIPHeader ->
          [2]dsr_rt;
  ";
} else {
  print "
// tx feedback
wifi_fb :: WifiTXFeedback;

wifi_fb[0] -> Discard; // successful transmissions
wifi_fb[1] -> MSQueue -> // failures
           Unqueue -> 
           Classifier(! 0/FFFFFFFFFFFF) -> // throw out broadcasts
          Print(txf) -> Strip(14) ->
          CheckIPHeader ->
          [2]dsr_rt;

  ";
}
} else { 
  print  "
Idle -> [2]dsr_rt;
";
}

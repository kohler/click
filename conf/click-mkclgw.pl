#!/usr/bin/perl

#
#
# $Id: click-mkclgw.pl,v 1.13 2005/03/06 23:32:29 max Exp $
#
# click-mkclgw
#
#   Click Make Cluster Gateway
#
#    Imagine a cluster of servers that have a small block of IPs 
#    externally and a whole boatload of IPs internally.  This script 
#    accepts input configuration files, and outputs Click configurations
#    for managing packet rewriting for the cluster.  Incudes 
#    NATing and load balancing functionality.
#
#   Max Krohn
#
#
use strict;

#
# CLGW::Base
#
#  Base class for all CLGW data compontents (not parser components)
#
package CLGW::Base;

sub new {
    my $class = shift;
    my $self = { @_ };
    bless $self, $class;
    return $self;
}

sub iscluster { return 0; }
sub clust_full_size { return -1; }
sub active_nodes { return (); }
sub ith { return undef; }

#
# CLGW::Base::range_to_arr
#
#   Parse a range from the string of the form "[1-4,5-7,13]" and outputs
#   an array of the form (1,2,3,4,5,6,7,13).  Also returns a inclusion
#   map for the array, so that you can quickly lookup which elements
#   are in the set.
#
sub range_to_arr {
    my ($self, $input) = @_;
    my @terms = split /,/ , $input;
    my @lst;
    my $err = 0;
    foreach my $term (@terms) {
	if ($term =~ /^\d+$/) {
	    push @lst, $term;
	} elsif ($term =~ /^(\d+)-(\d+)$/ ) {
	    my $lim = $2;
	    for (my $i = $1; $i <= $lim; $i++) {
		push @lst, $i;
	    }
	} else {
	    warn "Bad range specification: $input\n";
	    $err = 1;
	}
    }
    return (undef, undef) if $err;

    my @slst = sort { $a <=> $b } @lst;
    my $ret_map = {};
    my $rarr = [];
    foreach my $i (@slst) {
	unless ($ret_map->{$i}) {
	    push @$rarr, $i;
	    $ret_map->{$i} = 1;
	}
    }
    return ($rarr, $ret_map);
}

#
# CLGW::Cluster - base class for Cluster data components
#
package CLGW::Cluster;

sub iscluster { return 1; }

package CLGW::Const;

#
# Woefully inadequate list of ports to specify in a config file
# Special ports "napt", "info" and "param" have been hacked on --
# they specify config file groups more than they specify ports.
# 
$CLGW::Const::PORT_ALL = 0;

$CLGW::Const::PORT_NAPT = 1;
$CLGW::Const::INFO = 2;
$CLGW::Const::PARAM = 3;

$CLGW::Const::PORT_HTTP = 4;
$CLGW::Const::PORT_SSH = 5;
$CLGW::Const::PORT_SFS = 6;
$CLGW::Const::PORT_HTTPS = 7;
$CLGW::Const::PORT_TELNET = 8;

#
# Different types of definable addressing schemes
#
$CLGW::Const::ADDR_ENET = 0;
$CLGW::Const::ADDR_IPNET = 1;
$CLGW::Const::ADDR_IP = 2;
$CLGW::Const::ADDR_IP_CLUSTER = 3;

$CLGW::Const::NAPT_PORT_RANGE = "50000-65535";
$CLGW::Const::TAB_IN = 45;  # formatting constant

%CLGW::Const::PORTMAP = ( "*" => $CLGW::Const::PORT_ALL,

			  # not ports per-se, but can refer
			  # to parts of the configuration file,
			  # in much the same we're using ports
			  "napt" => $CLGW::Const::PORT_NAPT,
			  "info" => $CLGW::Const::INFO,
			  "default" => $CLGW::Const::INFO,
			  "param" => $CLGW::Const::PARAM,
			  
			  # TCP ports/services open 
			  "www" => $CLGW::Const::PORT_HTTP,
			  "http" => $CLGW::Const::PORT_HTTP,
			  "ssh" => $CLGW::Const::PORT_SSH,
			  "sfs" => $CLGW::Const::PORT_SFS,
			  "https" => $CLGW::Const::PORT_HTTPS,
			  "telnet" => $CLGW::Const::PORT_TELNET );

%CLGW::Const::R_PORTMAP = ( $CLGW::Const::PORT_SSH => "ssh",
			    $CLGW::Const::PORT_HTTP => "www",
			    $CLGW::Const::PORT_HTTPS => "https",
			    $CLGW::Const::PORT_TELNET => "telnet",
			    $CLGW::Const::PORT_SFS => 4  );

# 
# strings of how these parameters will be output to the 
# Click configuration
# 
%CLGW::Const::LABEL_MAP = qw [ EXTERNAL     extern
			       INTERNAL     intern
			       NEXTHOP      extern_next_hop
			       INTDNS       dns_int ];

#
# CLGW::Parser
#
package CLGW::Parser;
@CLGW::Parser::ISA = qw [ CLGW::Base ] ;

sub new {
    my $self = CLGW::Base::new (@_);
    $self->{_eof} = 0;
    return $self;
}

#
# CLGW::Parser::nxtln
#
#    Pulls a line from the input file (at this point, can only
#    be standard input).
#
sub nxtln {
    my ($self) = @_;
    my $line;
    while (($line = <>)) {
	$line =~ s/#.*//;
	if ($line =~ /\S/) {
	    chomp ($line);
	    return $line;
	}
    }
    $self->{_eof} = 1;
    return undef;
}

sub parse_error {
    my ($self, $e, $ln) = @_;
    $ln = $self->{_eof} ? "EOF" : $self->{_line_now} unless $ln;
    die $ln . ": " . $e . "\n";
}

#
# CLGW::Parser::parse_rule_group
#
#   <rule-group> := {<port-specifier>;
#                    <rule1>;
#                    <rule2>;
#                       .
#                       .
#                    <rulen>}
#
#  <port-specifier> := (<port1>|<port2>| .... )  |
#                      (info) |
#                      (napt) |
#                      (param)
#
# Returns: 0 on success / -1 on EOF
#
sub parse_rule_group {
    my ($self, $arr_out) = @_;
    my $line = $self->nxtln ();
    $self->{_line_now} = $.;
    return -1 unless $line;
    $self->parse_error ("Expected rule start") unless $line =~ s/^\s*\{// ;
    my $n;
    my $done = 0;
    $done = 1 if $line =~ s/\}.*$// ;
    while ($done == 0 and ($n = $self->nxtln ())) {
	$line .= $n;
	$done = 1 if $line =~ s/\}.*$// ;
    }
    $self->parse_error ("Rule starting at line $self->{_line_now} unclosed!")
	unless $done;
    my @terms = split /\s*;\s*/, $line;

    my $srv_raw = shift @terms;
    $self->parse_error ("Expected port/service type!")
	unless $srv_raw =~ /^\((.*)\)$/ ;
    
    my $ports = $1;
    my @port_str_arr = split /\|/, $ports;
    my @port_arr;
    foreach my $port_str (@port_str_arr) {
	my $port = $CLGW::Const::PORTMAP{$port_str};
	$self->parse_error ("Unrecognized port/service: $port_str") 
	    unless defined $port;
	push @port_arr, $port;
    }

    $self->parse_error ("empty rule encountered!") unless $#terms >= 0;

    foreach my $term (@terms) {
	my $rule = $self->parse_rule ($term, \@port_arr);
	if (ref ($rule) eq "ARRAY") {
	    push @$arr_out, @$rule;
	} else {
	    push @$arr_out, $rule;
	}
    }
    return 0;
}

#
# CLGW::Parser::parse_rule
#
#  <rule> :=  <label> <addr1> <addr2> ...
#
sub parse_rule {
    my ($self, $term, $port_arr) = @_;
    my @factors = split /\s+/, $term;

    $self->parse_error ("unlabeled entry encountered") unless $#factors >= 0;
    my $label = $self->parse_cluster_label (shift @factors);

    $self->parse_error ("useless entry encountered") unless $#factors >= 0;

    my @addrs;
    foreach my $addr (@factors) {
	push @addrs, $self->parse_addr ($addr);
    }

    my $ret;

    if ($#addrs == 2 and
	$self->port_typ ($port_arr, $CLGW::Const::PORT_NAPT) and
	$addrs[0]->typ () eq "ip" and
	$addrs[1]->typ () eq "ip" and
	$addrs[2]->typ () eq "net" and
	!$label->iscluster () ) {
	
	#
	# NAPT cluster, not accessible from the outside:
	#
	#  <label> <ip-addr> <ip-addr> <net-addr>
	#
	$ret = CLGW::Rule::NAPT->new ( "_ports" => [ @$port_arr ],
					"_label" => $label,
					"_ext_addr" => $addrs[0],
					"_int_addr" => $addrs[1],
					"_nat_net" => $addrs[2] );

    } elsif ($#addrs == 0 and $addrs[0]->typ eq "ip" and
	     $self->port_typ ($port_arr, $CLGW::Const::INFO) and
	     !$label->iscluster () ) {
	
	#
	# Assign a label to an IP address
	#
	#  <label> <ip-addr>
	#
	$ret = CLGW::Rule::IPLabel->new ( "_label" => $label,
					  "_ip" => $addrs[0] );

    } elsif ($#addrs == 0 and $addrs[0]->typ eq "param" and
	     $self->port_typ ($port_arr, $CLGW::Const::PARAM) and
	     !$label->iscluster () ) {

	#
	# Set a parameter
	#
	#  <name> <value>
	#
	$ret = CLGW::Rule::Param->new ( "_label" => $label,
					 "_param" => $addrs[0] );

    } elsif ($#addrs == 2 and
	!$label->iscluster () and
	$addrs[0]->typ () eq "ip" and
	$addrs[1]->typ () eq "net" and
	$addrs[2]->typ () eq "eth") {

	#
	# Specify a Click-style AddrInfo
	#
	#   <label> <ip-addr> <net-addr> <eth>
	#
	$ret = CLGW::Rule::IFace->new ( "_label" => $label,
					 "_ip" => $addrs[0],
					 "_net" => $addrs[1],
					 "_eth" => $addrs[2] );
    } elsif ($#addrs == 1 and
	     !$label->iscluster () and
	     $addrs[0]->typ () eq "ip" and
	     $addrs[1]->typ () eq "eth" and
	     $self->port_typ ($port_arr, $CLGW::Const::INFO) ) {

	#
	# Specify a Click-style AddrInfo
	#
	#  <label> <ip-addr> <eth-addr>
	#
	$ret = CLGW::Rule::IFace->new ("_label" => $label,
					"_ip" => $addrs[0],
					"_eth" => $addrs[1] );

    } elsif ($#addrs == 0 and 
	     !$label->iscluster () and
	     $addrs[0]->typ () eq "eth" and
	     $self->port_typ ($port_arr, $CLGW::Const::INFO)) {

	#
	# Specify a Click-style AddrInfo
	#
	#  <label> <eth-addr>
	#
	$ret = CLGW::Rule::IFace->new ("_label" => $label,
					"_eth" => $addrs[0] );

    } elsif (($#addrs == 1 or $#addrs == 3) and
	     $addrs[0]->is_ip () and
	     $addrs[1]->is_ip () ) {

	my $ext_addr = $addrs[0];
	my $int_addr = $addrs[1];

	#
	# load-balanced napt cluster
	#
	#   <clust-label> <ip-addr> <ip-addrs> [ <ip-addr> <ip-net> ]
	#   Cluster Label  ExtIP      IntIPS       IntGW    NAPTNet
	#
	if ($label->iscluster () and !$ext_addr->iscluster ()) {

	    $self->parse_error ("Malformed load-balanced cluster; " .
				"internal IP range is not a cluster")
		unless $int_addr->iscluster ();

	    $self->parse_error ("Cluster size mismatch") unless 
		$label->clust_full_size () == 
		$int_addr->clust_full_size ();

	    
	    $ret = CLGW::Rule::LBNAPT->new ("_ports" => [ @$port_arr ],
					     "_label" => $label,
					     "_int_clust" => $int_addr,
					     "_ext_addr" => $ext_addr );

	    if ($#addrs == 3) {

		my $int_gw = $addrs[2];
		my $nat_net = $addrs[3];
		my $typ;

		$self->parse_error ("4th argument is Internal Cluster Addr".
				    "and should be a simple IP Address")
		    unless $int_gw->typ () eq "ip" ;
		$self->parse_error ("5th argument is a $typ but" .
				    "should be a network range/ipnet")
		    unless ($typ = $nat_net->typ ()) eq "net";

		$ret->{_nat_net} = $nat_net;
		$ret->{_int_addr} = $int_gw;
	    }

	    $self->parse_error ("Bad port specification")
		unless $ret->fix_ports ();

	} elsif (!$label->iscluster () and !$ext_addr->iscluster () and 
		 !$int_addr->iscluster () and $#addrs == 1) {

	    #
	    # simple NAT resolution
	    #
	    $ret = CLGW::Rule::Nat->new ("_ports" => [ @$port_arr ],
					  "_label" => $label,
					  "_int_addr" => $int_addr,
					  "_ext_addr" => $ext_addr );
	    $self->parse_error ("Bad port specification")
		unless $ret->fix_ports ();

	} elsif ($label->iscluster () and $ext_addr->iscluster () and
		 $int_addr->iscluster () and $#addrs == 1) {


	    my $sz = $label->clust_full_size ();

	    $self->parse_error ("NAT group size mismatch") unless 
		(($sz == $ext_addr->clust_full_size ()) and
		 ($sz == $int_addr->clust_full_size ()));

	    # 
	    # array of simple NAT resolutions
	    #
	    $ret = [];
	    my @active = $label->active_nodes ();

	    foreach my $i (@active) {
		my $tlab = $label->ith ($i);
		my $tint = $int_addr->ith ($i);
		my $text = $ext_addr->ith ($i);
		my $n = CLGW::Rule::Nat->new ("_ports" => [ @$port_arr ],
					       "_label" => $tlab,
					       "_int_addr" => $tint,
					       "_ext_addr" => $text );

		$self->parse_error ("Bad port specification")
		    unless $n->fix_ports ();
		push @$ret, $n;
	    }
	} else {
	    $self->parse_error ("Unrecognized term construction: $term");
	}
    } else {
	$self->parse_error ("Unrecognized term construction: $term");
    }
    return $ret;
}

sub port_typ {
    my ($self, $port_arr, $typ) = @_;
    return ($#$port_arr == 0 and $port_arr->[0] == $typ);
}

#
# CLGW::Parser::parse_addr
#
# Classifies addresses as either Ethernet, IP, IPNet or IP
# Cluster/Range types.
#
sub parse_addr {
    my ($self, $input) = @_;

    if ($input =~ /^([a-fA-F0-9]{2}\:){5}[a-fA-F0-9]{2}$/) {
	return CLGW::Addr::Eth->new ("_addr" => $input );
    }

    return CLGW::Addr::Param->new ("_value" => $input) 
	unless $input =~ /^((\d{1,3}\.){3})(.*)$/;
    my $prfx = $1;
    my $sffx = $3;

    my $ret;

    if ($sffx =~ /^\[(.*)\]$/ ) {
	my ($arr, $map) = $self->range_to_arr ($1);
	$ret = CLGW::Addr::Cluster->new ("_prefix" => $prfx, 
					  "_range" => [ @$arr ]);
    } elsif ($sffx =~ m!^(\d{1,3})(/(\d{1,2}))?$! ) {
	my $part4 = $1;
	my $net = $3;
	my $addr = $prfx .  $part4;
	if ($net) {
	    $self->parse_error ("Bad subnet id: $net")
		unless $net >=8 and $net <= 32;
	    $ret = CLGW::Addr::Net->new ("_addr" => $addr,
					  "_class" => $net );
	} else {
	    $ret = CLGW::Addr::IP->new ("_addr" => $addr);
	}
    } else {
	$self->parse_error("Bad address: $input");
    }
    return $ret;
}

#
# CLGW::Parser::parse_cluster_label
#
# Cluster labels consist of a label, a group of included IDs,
# and a group of excluded IDs, which should be a subset of the
# included ids.  Examples include:
#
#      ws[0-14]/[4,9,10-13]
#      img[0-4]
#      cgi[1,2,3]
#
sub parse_cluster_label {
    my ($self, $input) = @_;

    #
    # note, no "-" characters allowed in labels, since Click
    # does not allow "-" in labels.
    #
    return undef unless 
	$input =~ m!^([a-zA-Z0-9_]+)(\[(.*?)\](/\[(.*?)\])?)?$!;
    my $lab = $1;
    my $incl = $3;
    my $excl = $5;

    my $ret ;
    if ($incl) {
	$ret = CLGW::Label::Cluster->new ("_label" => $lab,
					   "_include" => $incl,
					   "_exclude" => $excl );
    } else {
	$ret = CLGW::Label->new ("_label" => $lab);
    }
    $self->parse_error ("Bad cluster label specification") unless $ret;
    return $ret;
}

package CLGW::Addr;
@CLGW::Addr::ISA = qw [ CLGW::Base ];

sub typ { return "generic"; }
sub is_ip { return 0; }

sub dump {
    my ($self) = @_;
    warn ref ($self) , ": " , $self->{_addr} , "\n";
}

sub str { return undef };

package CLGW::Addr::Param;
@CLGW::Addr::Param::ISA = qw [ CLGW::Addr ];

sub typ { return "param"; }
sub str {
    my ($self) = @_;
    return $self->{_value};
}

package CLGW::Addr::Net;
@CLGW::Addr::Net::ISA = qw [ CLGW::Addr ];

sub typ { return "net" ; }
sub str {
    my ($self) = @_;
    return $self->{_addr} . "/" . $self->{_class};
}

package CLGW::Addr::IP;
@CLGW::Addr::IP::ISA = qw [ CLGW::Addr ];

sub typ { return "ip"; }
sub is_ip { return 1; }
sub str {
    my ($self) = @_;
    return $self->{_addr};
}

package CLGW::Addr::Eth;
@CLGW::Addr::Eth::ISA = qw [ CLGW::Addr ];
sub str {
    my ($self) = @_;
    return $self->{_addr};
}

sub typ { return "eth"; }

package CLGW::Addr::Cluster;
@CLGW::Addr::Cluster::ISA = qw [ CLGW::Cluster CLGW::Addr ];

sub typ { return "cluster"; }
sub is_ip { return 1; }

sub ith {
    my ($self, $i) = @_;
    my $id = $self->{_range}->[$i];
    return CLGW::Addr::IP->new ("_addr" => $self->{_prefix} . $id, 
				 "_clustid" => $i);
}

sub clust_full_size {
    my ($self) = @_;
    return $#{$self->{_range}} + 1;
}

sub to_machines {
    my ($self) = @_;
    my @arr;
    foreach my $r (@{$self->{_range}}) {
	my $addr = $self->{_prefix} . $r;
	push @arr, CLGW::Add::IP->new ("_addr" =>  $addr,
				       "_clustid" => $r );
    }
    return @arr;
}

sub dump {
    my ($self) = @_;
    warn ref ($self), ":\n";
    my @mach = $self->to_machines ();
    foreach my $m (@mach) { $m->dump (); }
}

package CLGW::Label;
@CLGW::Label::ISA = qw [ CLGW::Base ];

sub str { 
    my ($self) = @_;
    return $self->{_label};
}

package CLGW::Label::Cluster;
@CLGW::Label::Cluster::ISA = qw [ CLGW::Cluster CLGW::Label ] ;

sub clust_full_size {
    my ($self) = @_;
    return $#{$self->{_inc_arr}} + 1;
}

#
# CLGW::Label::Cluster::active_nodes
#
# Outputs a list of IDs that correspond to active nodes in the cluster.
#
sub active_nodes {
    my ($self) = @_;
    my $sz = $self->clust_full_size ();
    my @ret = ();
    for (my $i = 0; $i < $sz; $i++) {
	my $id = $self->{_inc_arr}->[$i];
	push @ret, $i unless $self->{_excl_map}->{$id};
    }
    return @ret;
}

sub ith {
    my ($self, $i) = @_;
    my $id = $self->{_inc_arr}->[$i];
    return CLGW::Label->new ("_label" => $self->{_label} . $id );
}

#
# CLGW::Label::Cluster::new
#
# Does error checking to make sure that the exlcuded list is indeed
# a subset of the included list.
#
sub new {
    my $self = CLGW::Base::new ( @_ );
    ($self->{_inc_arr}, $self->{_inc_map}) 
	= $self->range_to_arr ( $self->{_include} );
    ($self->{_excl_arr}, $self->{_excl_map}) 
	= $self->range_to_arr ( $self->{_exclude} );

    my $err = 0;
    foreach my $l (qw [ _inc_arr _inc_map _excl_arr _excl_map] ) {
	$err = 1 unless ($self->{$l});
    }
	    
    foreach my $a (@{$self->{_excl_arr}}) {
	unless ($self->{_inc_map}->{$a}) {
	    warn "Cannot exclude $self->{_label}$a; not included!\n";
	    $err = 1;
	}
    }

    return $err ? undef : $self;
}

#
# CLGW::Rule
#
# Generic rule; base class for more useful rules.
#
package CLGW::Rule;
@CLGW::Rule::ISA = qw [ CLGW::Base ];

sub is_napt { return 0; }
sub addr_info { return undef; }
sub lb_mapper { return undef; }
sub get_int_ips {}
sub get_ext_ips {}
sub ip_from_extern_classify { return undef; }
sub tcp_from_extern_classify { return undef; }
sub rewriter_pattern { return undef; }
sub ip_rewriter { return undef; }
sub rewriter_plumbing { return undef; }
sub get_intern_src { return undef; }
sub from_int_plumbing {}

#
# CLGW::Rule::connect
#
# A static method; takes element 1 (e1) port 1 (p1), and hooks
# it up to element 2, port 2, addind comment (com) to the end of the
# line.  Also tries to do some text alignment for purely asthetic
# reasons.
#
sub connect {
    my ($self, $e1, $p1, $e2, $p2, $com) = @_;
    my $p2_str = defined($p2) ? "[$p2]" : "";
    my $s = $e1 . "[$p1] -> " .  $p2_str . $e2 . ";";
    my $len = $CLGW::Const::TAB_IN - length ($s) ;
    foreach (1 .. $len) { $s .= " "; }
    $s .= " // $com" if $com;
    print $s, "\n";
}

#
# CLGW::Rule::discard
#
# As above, but throws out all packets coming out of the given
# port.
#
sub discard {
    my ($self, $e1, $p1, $com) = @_;
    my $s = $e1 . "[$p1] -> Discard;";
    my $len = $CLGW::Const::TAB_IN - length ($s) ;
    foreach (1 .. $len) { $s .= " "; }
    $s .= " // $com" if $com;
    print $s, "\n";
}

#
# CLGW::Rule::is_internal_dns
#
# Currently broken DNS feature.
#
sub is_internal_dns {
    my ($self) = @_;
    return $self->labelstr () eq "INTDNS";
}

sub labelstr { 
    my ($self) = @_;
    return $self->{_label}->str ();
}

#
# CLGW::Rule::int_lab
#
# Internal label for this rule..
#
sub int_lab {
    my ($self) = @_;
    return $self->labelstr () . "_int"; 
}

sub labelstr_gw_override {
    my ($self) = @_;
    return $self->labelstr ();
}

sub in_pat {
    my ($self) = @_;
    return $self->labelstr_gw_override () . "_in";
}

sub out_pat {
    my ($self) = @_;
    return $self->labelstr_gw_override () . "_out";
}

sub tcp_classifier_label {
    my ($self) = @_;
    my $prfx = $self->labelstr_gw_override ();
    return $prfx . "_ip_from_extern";
}

sub mapped_label {
    my ($self) = @_;
    my $lab = $self->labelstr ();
    my $lab2 = $CLGW::Const::LABEL_MAP{$lab};
    $lab = $lab2 if defined($lab2);
    return $lab;
}

#
# CLGW:Rule::fix_ports
#
# Converts ports from our input file format into Click style
# port combinations.
#
sub fix_ports {
    my ($self) = @_;
    my @ports;
    foreach my $p (@{$self->{_ports}}) {
	my $s = $CLGW::Const::R_PORTMAP{$p};
	if ($p == $CLGW::Const::PORT_ALL) {
	    $self->{_port_str} = "*";
	    return 1;
	}
	return 0 unless $s;
	push @ports, $s;
    }
    $self->{_port_str} = join " or ", @ports;
    return 1;
}

# 
# class for rules regarding this cluster's interfaces..
#
package CLGW::Rule::IFace;
@CLGW::Rule::IFace::ISA = qw [ CLGW::Rule ];

sub addr_info {
    my ($self) = @_;
    my $lab = $self->mapped_label ();
    my @elems = ( $lab );
    foreach my $prt (qw [ _ip _net _eth ] ) {
	my $x = $self->{$prt};
	push @elems, $x->str () if $x;
    }
    return join "\t", @elems;
}

sub is_external_iface {
    my ($self) = @_;
    return $self->labelstr () eq "EXTERNAL";
}

sub get_int_ips {
    my ($self, $outarr) = @_;
    return unless $self->labelstr () eq "INTERNAL";
    push @$outarr, $self->mapped_label () . ":ip";
}

sub get_ext_ips {
    my ($self, $outarr) = @_;
    return unless $self->is_external_iface ;
    push @$outarr, $self->mapped_label () . ":ip";
}

sub ip_from_extern_classify {
    my ($self, $port) = @_;
    return undef unless $self->is_external_iface ;
    $self->{_ip_from_extern_port} = $port;
    return $self->mapped_label ();
}

sub labelstr_gw_override { 
    my ($self) = @_;
    if ($self->{_label_override})  {
	return $self->{_label_override}
    } else {
	return "gw"; 
    }
}

sub tcp_from_extern_classify {
    my ($self) = @_;
    return undef unless $self->is_external_iface ;
    my $x = $self->tcp_classifier_label ();
    my $p = $self->{_ip_from_extern_port};
    my @inputs = ( "src tcp port ftp", 
		   "tcp or udp",
		   "-");

    $self->{_tcp_portmap}->{_ftp} = 0;
    $self->{_tcp_portmap}->{_tcp_or_udp} = 1;
    $self->{_tcp_portmap}->{_def} = 2;

    my $istr = join ",\n", map { "\t$_" } @inputs;
    print "$x :: IPClassifier(\n$istr\n);\nip_from_extern[$p] -> $x;\n\n";

}

sub rewriter_pattern {
    my ($self) = @_;
    return undef unless $self->is_external_iface ;
    my @els = ( $self->out_pat (),
		$self->mapped_label (), 
		$CLGW::Const::NAPT_PORT_RANGE,
		"-",
		"-" );
    return join (" ", @els);
}

sub ip_rewriter {
    my ($self, $port, $map) = @_;
    return undef unless $self->is_external_iface ;
    my @elems = ( "pattern", 
		  $self->out_pat (),
		  0,
		  1);
    $self->{_out_pat_port} = $port;
    return join (" ", @elems);
}

sub rewriter_plumbing {
    my ($self, $port_map, $map) = @_;
    return undef unless $self->is_external_iface ;
    my $cl = $self->tcp_classifier_label ();
    $self->connect ($cl, $self->{_tcp_portmap}->{_ftp}, 
		    "tcp_rw", 1, "FTP control traffic");
    $self->connect ($cl, $self->{_tcp_portmap}->{_tcp_or_udp}, 
		    "rw", $port_map->{_ext_to_gw}, 
		    "all TCP/UDP to GW");
    $self->discard ($cl, 2, "discard everything else");
    print "\n";
}

package CLGW::Rule::NatBase;
@CLGW::Rule::NatBase::ISA = qw [ CLGW::Rule ];

sub accepting { return 0; }

sub from_int_plumbing {
    my ($self, $in, $out) = @_;
    my ($p1, $p2);
    if (defined ($p1 = $self->{_intern_src_out_port}) and
	defined ($p2 = $self->{_out_pat_port})) {
	$self->connect ($in, $p1, $out, $p2);
    }
}

sub tcp_from_extern_classify {
    my ($self) = @_;
    my $x = $self->tcp_classifier_label ();
    my $p = $self->{_ip_from_extern_port};
    my $ports = $self->{_port_str};
    my $shft = 0;
    my @inputs = ( "src tcp port ftp",
		   "tcp or udp",
		   "-" );

    my $prt = 0;
    
    if (!($ports eq "*") and $self->accepting ()) {
	unshift @inputs, "dst tcp $ports";
	$self->{_fw} = 1;
	$self->{_tcp_portmap}->{_accepting} = $prt ++;
    }
    foreach my $pn (qw [ _ftp _tcp_or_udp _def ] ) {
	$self->{_tcp_portmap}->{$pn} = $prt ++;
    }

    my $port_str = join ",\n", map {"\t$_" } @inputs;

    print "$x :: IPClassifier (\n$port_str\n);\nip_from_extern[$p] -> $x;\n\n";
}

sub rewriter_plumbing {
    my ($self, $port_map, $map) = @_;
    my $cl = $self->tcp_classifier_label ();
    my ($p1, $p2);
    if (defined ($p1 = $self->{_tcp_portmap}->{_accepting}) and
	defined ($p2 = $self->{_in_pat_port})) {

	$self->connect ($cl, $p1, "rw", $p2, "accepting incoming connections");
    }
    $self->connect ($cl, $self->{_tcp_portmap}->{_ftp}, "tcp_rw", 1,
		    "FTP control traffic" );

    my $p1 = $self->{_tcp_portmap}->{_tcp_or_udp};

    if (defined ($p2 = $self->{_in_pat_port}) and
	$self->{_port_str} eq "*" and
	$self->accepting ()) {
	$self->connect ($cl, $p1, "rw", $p2, "BEWARE! No firewalling");
    } else {
	$self->connect ($cl, $p1, "rw", $port_map->{_drop},
			"rewrite or drop!");
    }
    $self->discard ($cl, $self->{_tcp_portmap}->{_def},
		    "non TCP/UDP");
    print "\n";
}

sub ip_from_extern_classify {
    my ($self, $port) = @_;
    $self->{_ip_from_extern_port} = $port;
    return $self->ext_lab ();
}

sub ext_lab {
    my ($self) = @_;
    return $self->labelstr () . "_ext";
}

sub get_ext_ips {
    my ($self, $outarr) = @_;
    push @$outarr, $self->ext_lab () . ":ip";
}

package CLGW::Rule::Nat;
@CLGW::Rule::Nat::ISA = qw [ CLGW::Rule::NatBase ];

sub accepting { return 1; }

sub rewriter_pattern {
    my ($self) = @_;
    my @els = ( $self->in_pat (),
		"-",
		"-",
		$self->int_lab (),
		"-");

    my $ret = [ join " ", @els ];

    @els = ( $self->out_pat (),
	     $self->ext_lab (),
	     "-",
	     "-",
	     "-");
    push @$ret, join (" ", @els);
    return $ret;
}

sub ip_rewriter {
    my ($self, $port, $map) = @_;
    my @els = ( "pattern",
		$self->out_pat (),
		0,
		1);
    $self->{_out_pat_port} = $port ++;
    my $ret = [ join " ", @els ];
    @els = ( "pattern",
	     $self->in_pat (),
	     1,
	     0);
    $self->{_in_pat_port} = $port;
    push @$ret, join (" ", @els);
    return $ret;
}

sub get_intern_src {
    my ($self, $port) = @_;
    my $ret = "src host ". $self->int_lab . ":ip";
    $self->{_intern_src_out_port} = $port;
    return $ret;
}

sub addr_info {
    my ($self) = @_;
    my $ret = [ $self->int_lab () . "\t" . $self->{_int_addr}->str () ,
		$self->ext_lab () . "\t" .  $self->{_ext_addr}->str () ];
    return $ret;
}


package CLGW::Rule::NAPT;
@CLGW::Rule::NAPT::ISA = qw [ CLGW::Rule::NatBase ];

sub net_lab {
    my ($self) = @_;
    return $self->labelstr () . "_net"; 
}

sub get_intern_src {
    my ($self, $port) = @_;
    my $ret = "src net " . $self->net_lab . ":ipnet";
    $self->{_intern_src_out_port} = $port;
    return $ret;
}

sub get_int_ips {
    my ($self, $outarr) = @_;
    push @$outarr, $self->net_lab () . ":ip";
}

sub addr_info {
    my ($self) = @_;
    my @net = ( $self->net_lab (),
		$self->{_int_addr}->str (),
		$self->{_nat_net}->str ()  );
    my @ext = ( $self->ext_lab (),
		$self->{_ext_addr}->str ());
    my $ret = [ join "\t", @net ];
    push @$ret, join ("\t", @ext);
    

    for (my $i = 0; $i < $self->{_label}->clust_full_size (); $i++) {
	push @$ret, join "\t", ($self->{_label}->ith ($i)->str (),
				$self->{_int_clust}->ith ($i)->str ()) ;
    }
    return $ret;
}

sub rewriter_pattern {
    my ($self) = @_;
    my @els = ( $self->out_pat (), 
		$self->ext_lab (),
		$CLGW::Const::NAPT_PORT_RANGE,
		"-",
		"-" );
    return join (" ", @els);
}

sub ip_rewriter {
    my ($self, $port, $map) = @_;
    my @els = ( "pattern",
		$self->out_pat (),
		0,
		1);
    $self->{_out_pat_port} = $port;
    return join (" ", @els );
}

#
# CLGW::Rule::LBNAPT
#
# Cluster of servers.  Input traffic is load balanced.  Output traffic
# is NAPT'ed.
#
package CLGW::Rule::LBNAPT;
@CLGW::Rule::LBNAPT::ISA = qw [ CLGW::Rule::NAPT ];

sub accepting { return 1; }

sub ip_rewriter {
    my ($self, $port, $map) = @_;
    my $ret =  [ $self->CLGW::Rule::NAPT::ip_rewriter ($port, $map) ];
    push @$ret, $self->{_mapper};
    $self->{_in_pat_port} = $port + 1;
    return $ret;
}

#
# CLGW::Rule::LBNAPT
#
# Outputs the core lb_mapper element for the input path.  Note that
# the lb_mapper is of type SourceIPHashMapper.  In future versions 
# of this script, we'd probably like to make this element configurable.
# For now, we've hardcoded this element in.  It does resolutions based
# on consistent hashes of the source IP address.
#
sub lb_mapper {
    my ($self, $map) = @_;
    my $seed = "0xbadbeef";
    my $c = $map->{LBSEED};
    $seed = $c->{_param}->str () if $c;
    my $nodes = 128;
    $c = $map->{LBNODES};
    $nodes = $c->{_param}->str () if $c;
    my $l = $self->mapped_label ();
    my $mapper = $l . "_lb_mapper";
    $self->{_mapper} = $mapper;

    my $ret = 
	"// Load Balancer for the '$l' Cluster\n" .
	"$mapper :: SourceIPHashMapper (\n\n" . 
	"\t// Params: Nodes per machine=$nodes; Seed=$seed\n" .
	"\t$nodes $seed,\n\n" .
	"\t// Cluster Machine Entries\n";

    my @nodes;
    for (my $i = 0; $i < $self->{_label}->clust_full_size (); $i++) {
	my @elems = ();
	my $id = $self->{_label}->{_inc_arr}->[$i];
	next if $self->{_label}->{_excl_map}->{$id};
	push @elems, qw [ - - ];
	push @elems, $self->{_label}->ith ($i)-> str();
	push @elems, qw [ - 1 0 ];
	push @elems, $id;
	push @nodes, "\t" . join " ", @elems,
    }
    my $node_str =  join ",\n", @nodes;
    $ret .= $node_str .= "\n);\n\n";

    return $ret;
}

#
# CLGW::Rule::IPLabel
#
# Right now, mainly hacked on for assinging an IP address to the
# internal DNS server that's taking redirects (although this doesn't
# currently work).
#
package CLGW::Rule::IPLabel;
@CLGW::Rule::IPLabel::ISA = qw [ CLGW::Rule ];

sub addr_info {
    my ($self) = @_;
    return $self->mapped_label () . "\t" . $self->{_ip}->str ();
}

sub pat_name { return "dns_pat" ; }

sub rewriter_pattern {
    my ($self, $map) = @_;
    return undef unless $self->is_internal_dns ;
    my @els = ( $self->pat_name (), 
		$map->{INTERNAL}->mapped_label (),
		"-",
		$self->mapped_label (),
		"-" );
    return join (" ", @els);
}

sub ip_rewriter {
    my ($self, $port, $map) = @_;
    $self->{_in_pat_port} = $port;
    my @els = ( "pattern",
		$self->pat_name (),
		1,
		1);
    return join (" ", @els);
}

package CLGW::Rule::Param;
@CLGW::Rule::Param::ISA = qw [ CLGW::Rule ];


#
#-----------------------------------------------------------------------
#-----------------------------------------------------------------------
#
# Output code
#  
#   The outputter class simply runs the output functions in sequence
#   to output the target Click configuration.  The functions are split
#   up into different functions mainly for readability.  But usually,
#   they're only called once in the output process.
#
package CLGW::Output;
@CLGW::Output::ISA = qw [ CLGW::Base ];

sub addr_info {
    my ($self) = @_;
    $self->start_fn ("addr_info");
    print "AddressInfo(\n";
    my $first = 1;
    my @infos;
    foreach my $rule (@{$self->{_rules}}) {
	my $info = $rule->addr_info ();
	next unless defined $info;
	if (ref($info) eq "ARRAY") {
	    push @infos, @$info;
	} else {
	    push @infos, $info;
	}
    }
    print $self->stdjoin ( \@infos );
    print "\n);\n\n";
}

sub lb_mappers ()
{
    my ($self) = @_;
    $self->start_fn ("lb_mappers");
    foreach my $rule (@{$self->{_rules}}) {
	my $lbm = $rule->lb_mapper ($self->{_map});
	next unless $lbm;
	print $lbm, "\n";
    }
}

sub dev_setup ()
{
    my ($self) = @_;
    $self->start_fn ("dev_setup");
    my $c = $self->{_map}->{SNIFF};
    my $sniff = 0;
    $sniff = 1 if ($c and $c->{_param}->{_value} == 1);

    if ($sniff) {
	print <<EOF;
elementclass SniffGatewayDevice {
  \$device |
  from :: PollDevice(\$device)
	-> t1 :: Tee
	-> output;
  input -> q :: Queue(1024)
	-> t2 :: PullTee
	-> to :: ToDevice(\$device);
  t1[1] -> ToHostSniffers;
  t2[1] -> ToHostSniffers(\$device);
  ScheduleInfo(from .1, to 1);
}
	
extern_dev :: SniffGatewayDevice(extern:eth);
intern_dev :: SniffGatewayDevice(intern:eth);
	
EOF
} else {
    print <<EOF;

elementclass GatewayDevice {
  \$device |
  from :: PollDevice(\$device)
	-> output;
  input -> q :: Queue(1024)
	-> to :: ToDevice(\$device);
  ScheduleInfo(from .1, to 1);
}

extern_dev :: GatewayDevice(extern:eth);
intern_dev :: GatewayDevice(intern:eth);

EOF
}

}

sub host_setup {
    my ($self) = @_;
    $self->start_fn ("host_setup");
    print <<EOF;

//
// ip_to_host: smacks a dummy ethernet header on this packet and
// sends it to the Host OS.
// 
ip_to_host_int :: EtherEncap(0x0800, 1:1:1:1:1:1, intern)
	-> ToHost;

ip_to_host_ext :: EtherEncap(0x0800, 2:2:2:2:2:2, extern)
	-> ToHost;

EOF
}

sub arp_machinery {
    my ($self) = @_;
    $self->start_fn ("arp_machinery");
    my (@iips, @eips);
    foreach my $r (@{$self->{_rules}}) {
	$r->get_int_ips (\@iips);
	$r->get_ext_ips (\@eips);
    }
    my $int_ips = join " ", @iips;
    my $int_eth = $self->{_map}->{INTERNAL}->mapped_label () . ":eth";

    my $ext_ips = join " ", @eips;
    my $ext_eth = $self->{_map}->{EXTERNAL}->mapped_label () . ":eth";

    print <<EOF;
// ARP MACHINERY
extern_arp_class, intern_arp_class
	:: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);
intern_arpq :: ARPQuerier(intern);

extern_dev -> extern_arp_class;

// other machines on the network are querying us for particular IPs
extern_arp_class[0] -> 
    ARPResponder($ext_ips $ext_eth) -> 
    extern_dev;
extern_arp_class[1] -> ToHost;			// ARP responses
extern_arp_class[3] -> Discard;

intern_dev -> intern_arp_class;
intern_arp_class[0] -> 
	ARPResponder($int_ips $int_eth) ->
	intern_dev;
intern_arp_class[1] -> intern_arpr_t :: Tee(2);
	intern_arpr_t[0] -> ToHost;
	intern_arpr_t[1] -> [1]intern_arpq;
intern_arp_class[3] -> Discard;
EOF
}

sub output_path {
    my ($self) = @_;
    $self->start_fn ("output_path");
    print <<EOF;

// OUTPUT PATH
ip_to_extern :: GetIPAddress(16) 
      -> CheckIPHeader
      -> EtherEncap(0x0800, extern:eth, extern_next_hop:eth)
      -> extern_dev;
ip_to_intern :: GetIPAddress(16) 
      -> CheckIPHeader
      -> [0]intern_arpq
      -> intern_dev;

EOF
}

sub ip_from_extern_classify {
    my ($self) = @_;
    $self->start_fn ("ip_from_extern_classify");
    my $port = 0;
    my @rules;
    foreach my $r (@{$self->{_rules}}) {
	my $lab = $r->ip_from_extern_classify ($port);
	if ($lab) {
	    push @rules, "dst host $lab";
	    $port++;
	}
    }
    push @rules, "-";
    my $rules_str = $self->stdjoin (\@rules );
    
    print <<EOF;
// IP-level classifications for packets coming in from outside world
ip_from_extern :: IPClassifier(
$rules_str);
ip_from_extern[$port] -> Discard;

EOF
}

sub tcp_from_extern_classify {
    my ($self) = @_;
    $self->start_fn ("tcp_from_extern_classify");
    foreach my $r (@{$self->{_rules}}) {
	$r->tcp_from_extern_classify ();
    }
}

sub from_ext_plumbing {
    my ($self) = @_;
    $self->start_fn ("from_ext_plumbing");
    print <<EOF;
extern_arp_class[2] -> Strip(14)
    -> CheckIPHeader
    -> ip_from_extern;

EOF
}

sub rewriter_patterns {
    my ($self) = @_;
    $self->start_fn ("rewriter_patterns");
    my @pats;
    foreach my $r (@{$self->{_rules}}) {
	my $pat = $r->rewriter_pattern ($self->{_map});
	next unless defined $pat;
	if (ref ($pat) eq "ARRAY") {
	    push @pats, @$pat;
	} else {
	    push @pats, $pat;
	}
    }

    my $patstr = $self->stdjoin (\@pats );
    print "IPRewriterPatterns(\n$patstr\n);\n\n";
}

sub ip_rewriter {
    my ($self) = @_;
    $self->start_fn ("ip_rewriter");

    my @lns;
    my $port = 0;
    foreach my $r (@{$self->{_rules}}) {
	my $ln = $r->ip_rewriter ($port, $self->{_map});
	next unless defined $ln;
	if (ref ($ln) eq "ARRAY") {
	    push @lns, @$ln;
	    $port += ($#$ln + 1);
	} else {
	    push @lns, $ln;
	    $port++;
	}
    }
    $self->{_pat_ports}->{_int_to_gw} = $port ++;
    push @lns, "nochange 0";
    $self->{_pat_ports}->{_ext_to_gw} = $port ++;
    push @lns, "nochange 2";
    $self->{_pat_ports}->{_drop} = $port ++;
    push @lns, "drop";

    my $str = $self->stdjoin (\@lns );
    print "rw :: IPRewriter(\n$str\n);\n\n";
}

sub tcp_rewriter {
    my ($self) = @_;
    $self->start_fn ("tcp_rewriter");
    my $pat = $self->{_map}->{EXTERNAL}->out_pat ();
    print <<EOF;
// internal traffic -> outside world
tcp_rw :: TCPRewriter ( pattern $pat 0 1,
			drop );

EOF
}

sub rewriter_plumbing_generic {
    my ($self) = @_;
    $self->start_fn ("rewriter_plumbing_generic");
    my $int = $self->{_map}->{INTERNAL}->mapped_label ();
    my $ext = $self->{_map}->{EXTERNAL}->mapped_label ();
    print <<EOF;
rw[0] -> ip_to_extern_class :: IPClassifier(dst host $int, -);
  ip_to_extern_class[0] -> ip_to_host_int;
  ip_to_extern_class[1] -> ip_to_extern;
rw[1] -> ip_to_intern;
rw[2] -> IPClassifier(dst host $ext)
      -> ip_to_host_ext;

// tcp_rw is used only for FTP control traffic
tcp_rw[0] -> ip_to_extern;
tcp_rw[1] -> ip_to_intern;

EOF
}

sub rewriter_plumbing {
    my ($self) = @_;
    $self->start_fn ("rewriter_plumbing");
    foreach my $r (@{$self->{_rules}}) {
	$r->rewriter_plumbing ($self->{_pat_ports}, $self->{_map});
    }
}

sub from_int_ip_classify {
    my ($self) = @_;
    $self->start_fn ("from_int_ip_classify");
    my $int = $self->{_map}->{INTERNAL}->mapped_label ();
    print <<EOF;
// FILTER & REWRITE IP PACKETS FROM INSIDE

// Clasify by destination
ip_from_intern :: IPClassifier(dst host $int,
			       dst net $int,
			       dst tcp port ftp,
			       -);

EOF

}

sub stdjoin {
    my ($self, $arr) = @_;
    return join ",\n", map { "\t$_" } @$arr;
}

sub from_int_src_classify {
    my ($self) = @_;
    $self->start_fn ("from_int_src_classify");
    my @srcs;
    my $port = 0;
    foreach my $r (@{$self->{_rules}}) {
	my $s = $r->get_intern_src ($port);
	next unless $s;
	push @srcs, $s;
	$port ++;
    }
    $self->{_map}->{INTERNAL}->{_intern_src_out_port} = $port;
    push @srcs, "-";
    my $str = $self->stdjoin (\@srcs);
    print "ip_from_intern_src :: IPClassifier(\n$str\n);\n\n";
}

sub from_int_to_gw_tcp_classify {
    my ($self) = @_;
    $self->start_fn ("from_int_gw_tcp_classify");

    my $int = $self->{_map}->{INTERNAL};
    $int->{_tcp_classifier} = "gw_ip_from_intern";
    my $x = $int->{_tcp_classifier};

    my @els = ( "dst tcp ssh");
    my $port = 0;
    $int->{_tcp_portmap}->{_ssh} = $port ++;

    if ($self->{_map}->{INTDNS}) {
	push @els, "dst port dns";
	$int->{_tcp_portmap}->{_dns} = $port ++;
    }
    push @els, "tcp or udp";
    $int->{_tcp_portmap}->{_tcp_or_udp} = $port ++;
    push @els,  "-";
    $int->{_tcp_portmap}->{_def} = $port ++;

    my $str = $self->stdjoin (\@els);
    
    print "// For GW, classify by TCP port\n" .
	"$x :: IPClassifier(\n$str\n);\n\n";
}

sub from_int_plumbing_generic {
    my ($self) = @_;
    $self->start_fn ("from_int_plumbing_generic");
    my $int = $self->{_map}->{INTERNAL};
    my $pat = $int->out_pat ();
    my $ix = $int->{_tcp_classifier};
    print <<EOF;
intern_arp_class[2] -> Strip(14)
  	-> CheckIPHeader
	-> ip_from_intern;

ip_from_intern[0] -> $ix;
ip_from_intern[1] -> ip_to_host_int;          // net 10.X stuff, like broadcast
ip_from_intern[2] -> FTPPortMapper(tcp_rw, rw, $pat 0 1)
		  -> [0]tcp_rw;              // FTP traffic
ip_from_intern[3] -> ip_from_intern_src;

EOF
    
}

sub from_int_to_gw_plumbing {
    my ($self) = @_;
    $self->start_fn ("from_int_to_gw_plumbing");

    my $int = $self->{_map}->{INTERNAL};
    my $pat = $int->out_pat ();
    my $ix = $int->{_tcp_classifier};

    $int->connect ($ix, $int->{_tcp_portmap}->{_ssh}, "ip_to_host_int", undef,
		   "SSH to linux");
    my $p1;
    if (defined ($p1 = $int->{_tcp_portmap}->{_dns})) {
	my $dns = $self->{_map}->{INTDNS};
	$int->connect ($ix, $p1, "rw", $dns->{_in_pat_port},
		       "DNS to internal DNS");
    }
    $int->connect ($ix, $int->{_tcp_portmap}->{_tcp_or_udp}, "rw",
		   $self->{_pat_ports}->{_int_to_gw},
		   "send to linux via rw");
    $int->connect ($ix, $int->{_tcp_portmap}->{_def}, "ip_to_host_int", undef,
		   "non TCP/UDP to linux");
    print "\n";
}

sub from_int_plumbing {
    my ($self) = @_;
    $self->start_fn ("from_int_plumbing");
    my $l = "ip_from_intern_src";
    foreach my $r (@{$self->{_rules}}) {
	$r->from_int_plumbing ($l, "rw");
    }
    my $int = $self->{_map}->{INTERNAL};
    my $ext = $self->{_map}->{EXTERNAL};
    $int->connect ($l, $int->{_intern_src_out_port}, "rw",
		   $ext->{_out_pat_port}, 
		   "Everything else through def");
    print "\n";
}

sub start_fn {
    my ($self, $l) = @_;
    print <<EOF;
//
// CLGW::Output::$l
//
EOF
}


my $parser = CLGW::Parser->new ();
my @rules;
while ($parser->parse_rule_group (\@rules) >= 0) {}

my %rule_map;
foreach my $r (@rules) {
    my $l = $r->{_label}->str ();
    $parser->parse_error ("duplicate label: $l\n") if $rule_map{$l};
    $rule_map{$l} = $r;
}
$parser->parse_error ("Label 'gw' is reserved\n") if $rule_map{gw};

foreach my $c (qw [ EXTERNAL INTERNAL NEXTHOP ]) {
    $parser->parse_error ("Undefined address: " . $c)
	unless $rule_map{$c};
}

my $out = CLGW::Output->new ( "_rules" => \@rules,
			       "_map" => \%rule_map );

my $tm = `date`;
chomp ($tm);
print <<EOF;
//
//-----------------------------------------------------------------------
// Click Script Generated by $0
//
//    $tm
//-----------------------------------------------------------------------
//

EOF

$out->addr_info ();
$out->lb_mappers ();
$out->dev_setup ();
$out->host_setup ();
$out->arp_machinery ();
$out->output_path ();

$out->ip_from_extern_classify ();
$out->tcp_from_extern_classify ();
$out->from_ext_plumbing ();

$out->rewriter_patterns ();
$out->ip_rewriter ();
$out->tcp_rewriter ();
$out->rewriter_plumbing_generic ();
$out->rewriter_plumbing ();

$out->from_int_ip_classify ();
$out->from_int_src_classify ();
$out->from_int_to_gw_tcp_classify ();
$out->from_int_plumbing_generic ();
$out->from_int_to_gw_plumbing ();
$out->from_int_plumbing ();

#
# End of File -- Start POD Documentation
#

__END__

=head1 NAME

click-mkclgw - Makes a Click configuration for a cluster GW

=head1 SYNOPSIS

click-mkclgw < in.gw > out.click

=head1 DESCRIPTION

If you have a block of real IP addresses and are mapping to a NAT-ted,
firewalled, and perhaps load-balanced world of internal IP addresses,
then this script might help.  The point is to output a click 
configuration that will serve as a NAT-box, a firewall, and a 
poor-man's load-balancer all-in-one.  You actually don't need to
be all that poor to not be able to afford a $10k Cisco Local Director.

Output configurations can include simple NAT holes through the firewall,
and obviously load-balanced clusters.  In the latter case, machines
in a cluster make connections out via  NAPT.  This script will allow
some control over which IP address such clusters show to the outside
world when making such connections.  Overlapping server pools are
supported, but not very well error-checked.  

The resultant Click configurations are based on the mazu-nat.click
configuration in the Click release.  They also use a newly-fangled
B<SourceIPHashMapper> module.  The goal of this module is to consistent-hash
source IPs, so that users will be mapped to the same node in a load-balanced
cluster over multiple different TCP sessions. 


This script takes no flags, but expects configuration files of a 
pretty arbitrary variety.  The best way to describe this is just
to refer the reader to example.clgw in the F<conf/> directory
of Click.

=head1 BUGS

- Internal DNS redirection is not working.

- NAT resolutions should use IPAddrRewriter, and not IPRewriter.

- Better error-checking throughout is needed, especially for overlapping pools.

- More ports need to be supported.

=head1 AUTHOR

Max Krohn -- Electronic mail address is my last name at the
Massachusetts Institute of Technology.  



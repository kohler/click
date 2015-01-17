#!/usr/bin/perl 

sub usage {
    print "make-all-config.pl [nodelist]\n";
}

sub main {
    $count = 0;
    $line_num = 0;

    # Figure out where the node list is
    if (scalar(@ARGV) < 1) {
	if ($verbose) {print "Using STDIN\n";}
    } else {
	if ($verbose) {print "Using $ARGV[0]\n"; }
	open(NODELIST, $ARGV[0]) or die "Could not open $ARGV[0]\n";
    }

    # Read in node list
    while($line = scalar(@ARGV) < 1? <STDIN> : <NODELIST>) {

	$line_num++;
	#if (1 || $line !=~ /\#/) {
	if ($line =~ /(\S+)\s+([\d\.]+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(I2?)/) {
		print "adding $5\n";
		push @device, $1;
		push @ip, $2;
		push @hw, $3;
		push @gw, $4;
		if ($6 eq "I") {push @I2, 0;}
		elsif ($6 eq "I2") {push @I2, 1;}
		else { 
		    print "1Syntax error at line $line_num\n"; 
		    pop @device;
		    pop @ip;
		    pop @hw;
		    pop @gw;
		    pop @name;
		}
		$5 =~ /(\S+).ron.lcs.mit.edu/;
		push @name, $1;
	    } 
	#}

    }

    # Create server configuration
    for($i=0; $i<scalar(@ip); $i++) {
	$s = "./make-server-ron.pl $device[$i] $ip[$i] $hw[$i] $gw[$i] ";
	
	for($j=0; $j<scalar(@ip); $j++) {
	    if ($j != $i) {
		$s = "$s$ip[$j] ";
	    }
	}
	#$s = "$s> ";
	$s = "$s 18.26.4.89 > ";
	$s = "$s$name[$i]-server.conf";
	@args = ("tcsh", "-c", $s);
	system(@args);
	#print "$s\n";
    }

    # Create client configuration
    for($i=0; $i<scalar(@ip); $i++) {
	$s = "./make-multiclient-ron.pl $device[$i] $ip[$i] $hw[$i] $gw[$i] ";
	
	for($j=0; $j<scalar(@ip); $j++) {
	    if ($j != $i and !($I2[$i] and $I2[$j])) {
		$s = "$s$ip[$j] ";
	    }
	}
	$s = "$s> ";
	$s = "$s$name[$i]-client.conf";
	@args = ("tcsh", "-c", $s);
	system(@args);
	#print "$s\n";
    }




    close(STDIN);
    close(NODELIST);

}

&main();




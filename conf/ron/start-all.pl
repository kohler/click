#!/usr/bin/perl 

sub usage {
    print "start-all.pl [nodelist]\n";
}

sub main {
    $verbose = 1;
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
	if ( (substr $line, 0, 1) ne '#'){
	    if ($line =~ /(\S+)\s+([\d\.]+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(I2?)/) {
		push @device, $1;
		push @ip, $2;
		push @hw, $3;
		push @gw, $4;
		push @name, $5;
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
	    } else {
		#print "Sytax error at line $line_num\n";
	    }
	}

    }

    
    for($i=0; $i<scalar(@name); $i++) {
	$n = $name[$i];
	if ($name[$i] =~ /(\S+).ron.lcs.mit.edu/) {
	    $n = $1;
	}

	printf stderr "Working on $name[$i]\n";
	printf stderr " starting traceroute ";
	$command = "ssh $name[$i] -l ron yipal/click-export/conf/start-traceroute.sh $n";
	@args = ("tcsh", "-c", $command);
	system(@args);

	printf stderr " starting server ";
	$command = "ssh $name[$i] -l ron yipal/click-export/conf/start-server.sh $n";
	@args = ("tcsh", "-c", $command);
	system(@args);

	printf stderr " starting client ";
	$command = "ssh $name[$i] -l ron yipal/click-export/conf/start-client.sh $n";
	@args = ("tcsh", "-c", $command);
	system(@args);

	sleep 1;
    }

    for($i=0; $i<scalar(@name); $i++) {
	$n = $name[$i];
	if ($name[$i] =~ /(\S+).ron.lcs.mit.edu/) {
	    $n = $1;
	}
	printf stderr "Working on $name[$i]\n";
	printf stderr " starting datacollection ";
	$command = "ssh $name[$i] -l ron yipal/click-export/conf/start-datacollect.sh $n";
	@args = ("tcsh", "-c", $command);
	system(@args);
    sleep 1;
    }
}

&main();

#!/usr/bin/perl 

sub usage {
    print "push-click.pl [nodelist]\n";
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
		print "Sytax error at line $line_num\n";
	    }
	}

    }

    print "tarring up conf\n";
    $command = "tar czf /tmp/conf.tgz -C /home/am0/yipal/ron/click-export conf";
    @args = ("tcsh", "-c", $command);
    system(@args);

    for($i=0; $i<scalar(@name); $i++) {
	print "Working on $name[$i]\n";
	# commands of this form: 
	#   cat /tmp/click.tgz | ssh ron@mit.ron.lcs.mit.edu tar xzf - -C yipal/

	$command = "cat /tmp/conf.tgz | ssh $name[$i] -l ron tar xzf - -C yipal/click-export";
	@args = ("tcsh", "-c", $command);
	system(@args);

	sleep 1;
	
    }
    
    # remove temporary tar files
    print "Removing conf.tgz\n";
    @args = ("rm", "/tmp/conf.tgz");
    system(@args);
}

&main();

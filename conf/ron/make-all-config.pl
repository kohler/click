#!/usr/bin/perl 

# For click
#./make-client-ron.pl de0 18.26.4.89 00:00:c0:bd:6d:ef 18.26.4.1 24.218.249.231 155.101.134.253 208.246.45.8 206.71.69.222 128.84.154.59 66.123.174.76 206.197.119.141 204.168.181.39 18.31.0.144 130.37.30.16 128.2.181.105 130.240.65.20 204.119.59.136 4.19.249.125 209.213.214.92 24.162.251.208 192.249.24.10


sub usage {
    print "make-all-config.pl [nodelist]\n";
}

sub main {

    if (scalar(@ARGV) < 1) {
	if ($verbose) {print "Using STDIN\n";}
    } else {
	if ($verbose) {print "Using $ARGV[0]\n"; }
	open(NODELIST, $ARGV[0]) or die "Could not open $ARGV[0]\n";
    }


    while($line = scalar(@ARGV) < 1? <STDIN> : <NODELIST>) {
	if ( !($line =~ /\#/)) {

	    if ($line =~ /([\d\.]+)\s/) {
		print "$1\n";
	    }
	}



    }


}

&main();




#! /usr/bin/perl -w

sub mysystem ($) {
  my($sys) = @_;
  print STDERR $sys, "\n";
  my($ret) = system($sys);
  $ret && die "'$sys' failed ($ret)";
}

my($INSTALL) = 1;
my($ELEMENTS) = 0;
my($PROGMAN) = 0;
my($EXAMPLES) = 0;
my($DOXYGEN) = 0;
my($DOC_TAR_GZ) = 0;
my($NEWS) = 0;
while (@ARGV && $ARGV[0] =~ /^-/) {
    $_ = shift @ARGV;
    if (/^-x$/ || /^--no-install$/) {
	$INSTALL = 0;
    } elsif (/^-p$/ || /^--progman$/) {
	$PROGMAN = $DOXYGEN = 1;
    } elsif (/^-D$/ || /^--doxygen$/) {
	$DOXYGEN = 1;
    } else {
	die "Usage: ./mkwebdoc.pl [-x] [-D] [-p|--progman] CLICKWEBDIR";
    }
}
$ELEMENTS = $PROGMAN = $EXAMPLES = $DOXYGEN = $DOC_TAR_GZ = $NEWS = 1
    if !($ELEMENTS + $PROGMAN + $DOXYGEN + $DOC_TAR_GZ + $NEWS + $EXAMPLES);

@ARGV == 1 || die "Usage: ./mkwebdoc.pl [-x] [-p|--progman] CLICKWEBDIR";
my($WEBDIR) = $ARGV[0];
$WEBDIR =~ s/\/+$//;
my($DOCDIR, $EXDIR) = ("$WEBDIR/doc", "$WEBDIR/ex");
if ($ELEMENTS) {
    -r "$DOCDIR/template" || die "'$DOCDIR/template' not found";
}
if ($EXAMPLES) {
    -r "$EXDIR/template" || die "'$EXDIR/template' not found";
}

# -1. get into correct directory
chdir('..') if !-d 'linuxmodule';
-d 'linuxmodule' || die "must be in CLICKDIR or CLICKDIR/doc";
$top_srcdir = `grep '^top_srcdir' Makefile 2>/dev/null | sed 's/^.*= *//'`;
chomp $top_srcdir;
$top_srcdir = "." if $top_srcdir eq "" && -f "Makefile.in";
$top_srcdir || die "cannot extract top_srcdir";

# 0. create distdir
$MAKE = `which gmake`;
chomp $MAKE;
$MAKE = "make" if $MAKE eq "";
mysystem("$MAKE distdir") if ($INSTALL);

my($VERSION);
open(CONFIN, "$top_srcdir/configure.in") || die "no configure.in";
while (<CONFIN>) {
  if (/AC_INIT\(\[?click\]?, \[?(\S*?)\]?\)/) {
    $VERSION = $1;
    last;
  }
}
defined $VERSION || die "VERSION not defined in configure.in";
close CONFIN;

# 1. install manual pages and click-pretty tool
if ($INSTALL) {
    mysystem("/bin/rm -rf /tmp/%click-webdoc");
    mysystem("cd click-$VERSION && ./configure --prefix=/tmp/%click-webdoc --disable-linuxmodule --disable-bsdmodule --enable-snmp --enable-ipsec --enable-ip6 --enable-etherswitch --enable-radio --enable-grid --enable-analysis --enable-aqm --enable-test --enable-wifi");
    mysystem("cd click-$VERSION && $MAKE install-local EXTRA_PROVIDES='linuxmodule bsdmodule ns i586 i686 linux_2_2 linux_2_4 linux_2_6 smpclick int64' MKELEMMAPFLAGS='--dochref \"../doc/%s.n.html\"'");
    if ($ELEMENTS) {
	mysystem("cd click-$VERSION && $MAKE install-doc EXTRA_PROVIDES='linuxmodule bsdmodule ns i586 i686 linux_2_2 linux_2_4 linux_2_6 smpclick int64'");
    }
    if ($EXAMPLES) {
	mysystem("cd tools/click-pretty && $MAKE install");
    }
}

# 1.5. install examples
my(@examples) = ([ 'test.click', 'Trivial Test Configuration' ],
		 [ 'test2.click', 'Simple RED Example' ],
		 [ 'test3.click', 'Simple Scheduler Example' ],
		 [ 'test-device.click', 'Device Test' ],
		 [ 'test-tun.click', 'KernelTun Test' ],
		 [ 'udpgen.click', 'UDP Generator' ],
		 [ 'udpcount.click', 'UDP Counter' ],
		 [ 'mazu-nat.click', 'Sample Firewall/NAT' ],
		 [ 'thomer-nat.click', 'Sample One-Interface Firewall/NAT' ],
		 [ 'fake-iprouter.click', 'IP Router Simulation' ],
		 [ 'fromhost-tunnel.click', 'Kernel FromHost Tunnel Example' ],
		 [ 'dnsproxy.click', 'Trivial DNS Proxy' ],
		 [ 'simple-dsdv.click', 'Simple DSDV Configuration' ],
		 [ 'print-pings.click', 'ICMP Ping Printer' ]);
if ($EXAMPLES) {
    foreach $i (@examples) {
	my($fn, $title) = ('"' . $i->[0] . '"', '"' . $i->[1] . '"');
	my($html) = $fn;
	$html =~ s/\.click\"$/.html\"/;
	mysystem("click-pretty -C /tmp/%click-webdoc -t $EXDIR/template -dfilename=$fn -dtitle=$title click-$VERSION/conf/$fn > $EXDIR/$html");
    }
}

# 2.0. read elementmap
my(%ereq, $reqindex, $classindex, $provindex, $docnameindex);

sub em_unquote ($) {
    my($x) = @_;
    return '' if !defined($x);
    if ($x =~ /^\370/s) {
	$x = substr($x, 1, length($x) - 2);
	$x =~ tr/\372/ /;
	$x =~ s/\\\\/\373\373/g;
	$x =~ s/\\n/\n/g;
	$x =~ s/\\(.)/$1/g;
	$x =~ tr/\373/\\/;
    }
    $x;
}

my(%xml_entity) = ('amp' => '&', 'lt' => '<', 'gt' => '>', 'apos' => "'", 'quot' => '"');
sub xml_unquote_entity ($) {
    my($x) = @_;
    if ($x =~ /^\#(\d+)$/) {
	chr($1);
    } elsif ($x =~ /^\#(x\w+)$/) {
	chr(oct("0$1"));
    } else {
	$xml_entity{$x};
    }
}
sub xml_unquote ($) {
    my($x) = @_;
    $x =~ s/&([^;]*);/xml_unquote_entity($1)/eg;
    $x;
}

if ($ELEMENTS) {
    open(EMAP, "/tmp/%click-webdoc/share/click/elementmap.xml") || die "/tmp/%click-webdoc/share/click/elementmap.xml: $!\n";
    local($/) = undef;
    my($text) = <EMAP>;
    close(EMAP);

    $text =~ s/<!--.*?-->//gs;	# remove comments

    while ($text =~ /\A.*?<\s*entry\s+(.*)\Z/s) {
	my(%x, $n, $v);
	$text = $1;
	
	while ($text =~ /\A(\w+)\s*=\s*(.*)\Z/s) {
	    ($n, $text) = ($1, $2);
	    if ($text =~ /\A\'([^\']*)\'\s*(.*)\Z/s) {
		$text = $2;
		$x{$n} = xml_unquote($1);
	    } elsif ($text =~ /\A\"([^\"]*)\"\s*(.*)\Z/s) {
		$text = $2;
		$x{$n} = xml_unquote($1);
	    } else {
		last;
	    }
	}

	next if !($v = $x{'requires'});
	
	$ereq{ $x{'docname'} } = $v if $x{'docname'};
	$ereq{ $x{'name'} } = $v if $x{'name'} && !$x{'docname'};
	map { $ereq{$_} = $v } split(/\s+/, $x{'provides'}) if $x{'provides'};
    }
}

# 2.1. spread requirements
my(%ereq_expanded);

sub expand_ereq ($) {
    my($e) = @_;
    return $ereq{$e} if $ereq_expanded{$e};
    $ereq_expanded{$e} = 1;
    return ($ereq{$e} = '') if !$ereq{$e};
    my(@req) = split(/\s+/, $ereq{$e});
    my($i, $t, $r);
    for ($i = 0; $i < @req; $i++) {
	push @req, split(/\s+/, &expand_ereq($req[$i]))
	    if $ereq{$req[$i]};
    }
    $ereq{$e} = join(' ', @req);
    $ereq{$e};
}


# 2.2. changetemplate.pl
my(@esubj, @ealpha, @esections, %edefn, $cocked, %edeprecated);
if ($ELEMENTS) {
    open(IN, "/tmp/%click-webdoc/man/mann/elements.n") || die "/tmp/%click-webdoc/man/mann/elements.n: $!\n";
    $cocked = 0;
    while (<IN>) {
	s/\\f[BRPI]//g;
	push @{$esections[-1]}, scalar(@esubj) if /^\.SS/ && @esections;
	push @esections, [$1, scalar(@esubj)] if /^\.SS \"(.*)\"/;
	if (/^\.M (.*) n/ && $cocked == 1) {
	    push @esubj, $1;
	    $edefn{$1} = '';
	    $cocked = 2;
	} elsif (/^[^\.]/ && $cocked >= 2) {
	    $edefn{$esubj[-1]} .= $_;
	    $cocked = 3;
	} elsif (/^\.M (\S*)/ && $cocked == 3) {
	    $edefn{$esubj[-1]} .= " " . $1 . " ";
	} else {
	    $edefn{$esubj[-1]} = '' if $cocked == 2;
	    $cocked = ($_ =~ /^\.TP/);
	}
	last if (/^\.SH \"ALPHABETICAL/);
    }
    push @{$esections[-1]}, scalar(@esubj);
    while (<IN>) {
	push @ealpha, $1 if /^\.M (.*) n/ && $cocked;
	$edeprecated{$1} = 1 if /^\.M (.*) n .*deprecated/ && $cocked;
	$cocked = ($_ =~ /^\.TP/);
    }
    @ealpha = sort { lc($a) cmp lc($b) } @ealpha;
    close IN;
}

my($eli_rownum) = 0;
sub eli ($) {
    my($e) = @_;

    my($pe) = $e;
    $pe =~ s/\..*//;
    
    my($t) = "<tr class='list" . ($eli_rownum + 1) . "'><td>&nbsp;</td>";
    $eli_rownum = ($eli_rownum + 1) % 2;
    $t .= "<td><a href='$e.n.html'>$pe</a></td><td>&nbsp;&nbsp;&nbsp;</td>";

    if ($edeprecated{$e}) {
	$t .= "<td class='deprecated'><s>" . $edefn{$e} . "</s> (deprecated)</td>";
    } else {
	$t .= "<td>" . $edefn{$e} . "</td>";
    }

    $t .= "<td>&nbsp;&nbsp;&nbsp;</td>";
    
    if ($ereq{$e}) {
	my(@x);
	for (my $i = 0; $i < 2; $i++) {
	    my($r) = $ereq{$e};
	    push @x, "U" if $r =~ /\buserlevel\b/;
	    push @x, "L" if $r =~ /\blinuxmodule\b/;
	    push @x, "B" if $r =~ /\bbsdmodule\b/;
	    push @x, "ns" if $r =~ /\bns\b/;
	    last if @x;
	    expand_ereq($e);
	}
	if (@x) {
	    $t .= "<td class='drivers'>" . join(', ', @x) . "</td>";
	} else {
	    $t .= "<td class='drivers'></td>";
	}
    } else {
	$t .= "<td class='drivers'></td>";
    }
    
    $t . "<td>&nbsp;&nbsp;&nbsp;</td></tr>\n";
}

sub elist_file ($) {
    my($file) = @_;
    open(IN, "$file") || die "$file: $!\n";
    open(OUT, ">$file.new") || die "$file.new: $!\n";
    while (<IN>) {
	if (/^<!-- clickdoc: ename/) {
	    print OUT;

	    for ($i = 0; $i < @ealpha; $i++) {
		print OUT eli($ealpha[$i]);
	    }

	    1 while (defined($_ = <IN>) && !/^<!-- \/clickdoc/);
	    print OUT;
	} elsif (/^<!-- clickdoc: ecat/) {
	    print OUT;

	    my($ncat) = 0;
	    foreach my $cat (@esections) {
		print OUT "<tr><td>&nbsp;</td></tr>\n" if $ncat != 0;
		$ncat++;

		# print subject heading
		print OUT "<tr class='listh'><td>&nbsp;</td><th colspan='4'>", $cat->[0], "</th><th colspan='2'><a href='#drivers'>Drivers</a></th></tr>\n";

		# print elements
		$eli_rownum = 0;
		for (my $i = $cat->[1]; $i < $cat->[2]; $i++) {
		    print OUT eli($esubj[$i]);
		}
	    }
	
	    1 while (defined($_ = <IN>) && !/^<!-- \/clickdoc/);
	    print OUT;
	} else {
	    print OUT;
	}
    }
    close IN;
    close OUT;
    if (system("cmp $file $file.new >/dev/null 2>&1")) {
	unlink("$file") || die "unlink $file: $!\n";
	rename("$file.new", "$file") || die "rename $file.new: $!\n";
    } else {
	unlink("$file.new") || die "unlink $file.new: $!\n";
    }
}


if ($ELEMENTS) {
    elist_file("$DOCDIR/elemcat.html");
    elist_file("$DOCDIR/elemname.html");
}

# 3. call 'man2html'
if ($ELEMENTS) {
    mysystem("man2html -l -m '<b>@</b>' -t $DOCDIR/template -d $DOCDIR /tmp/%click-webdoc/man/man*/*.?");
}

# 5. call 'changelog2html'
if ($NEWS) {
    mysystem("changelog2html -d $DOCDIR click-$VERSION/NEWS $WEBDIR/news.html");
}

# 6. edit 'news.html'
if ($NEWS) {
    open(IN, "$WEBDIR/news.html") || die "$WEBDIR/news.html: $!\n";
    open(OUT, ">$WEBDIR/news.html.new") || die "$WEBDIR/news.html.new: $!\n";
    my(%good);
    while (<IN>) {
	while (/\b([A-Z][A-Za-z0-9]*)\b/g) {
	    if (!exists $good{$1}) {
		$good{$1} = -r "$DOCDIR/$1.n.html";
	    }
	}
	s#\b([A-Z][A-Za-z0-9]*)\b#$good{$1} ? '<a href="doc/' . $1 . '.n.html">' . $1 . '</a>' : $1#eg;
	print OUT;
    }
    close IN;
    close OUT;
    unlink("$WEBDIR/news.html") || die "unlink $WEBDIR/news.html: $!\n";
    rename("$WEBDIR/news.html.new", "$WEBDIR/news.html") || die "rename $WEBDIR/news.html.new: $!\n";
}

# 7. install programming manual
if ($PROGMAN) {
    mysystem("cd click-$VERSION/doc && $MAKE click.html") if ($INSTALL || !-r "click-$VERSION/doc/click.html");

    open(IN, "click-$VERSION/doc/click.html") || die "couldn't make click.html";
    open(OUT, ">$DOCDIR/progman.html") || die;
    open(TMP, "$DOCDIR/template") || die;

    while (<TMP>) {
	s/&mantitle;/Click Programming Manual/g;
	print OUT;
	if (/^\<!-- man2html -->/) {
	    1 while defined($_ = <IN>) && !m{^\</head>};
	    $_ = <IN>;		# get rid of line
	    print OUT $_ while defined($_ = <IN>) && !m{^\</body>};
	    1 while defined($_ = <TMP>) && !m{^\<!-- /man2html -->};
	    print OUT $_;
	}
    }

    close IN;
    close OUT;
    close TMP;
}

# 8. install FAQ
if ($NEWS) {
    open(IN, "click-$VERSION/FAQ") || die;
    select IN;
    local($/) = undef;
    my($faq) = <IN>;
    close IN;
    $faq =~ s/\&/&amp;/g;
    $faq =~ s/</&lt;/g;
    open(FAQ, "$WEBDIR/faq.html") || die;
    select FAQ;
    local($/) = undef;
    my($htmlfaq) = <FAQ>;
    $htmlfaq =~ s{<!-- faq -->.*<!-- /faq -->}{<!-- faq -->$faq<!-- /faq -->}s;
    close FAQ;
    open(FAQ, ">$WEBDIR/faq.html") || die;
    print FAQ $htmlfaq;
    close(FAQ);
}

# 9. install clickconfig.dtd
if ($NEWS) {
    mysystem("cp click-$VERSION/tools/click2xml/clickconfig.dtd $WEBDIR");
}

# 10. doxycute
if ($DOXYGEN) {
    $directory = ($INSTALL ? "click-$VERSION" : $top_srcdir);
    open(IN, "$directory/doc/Doxyfile");
    open(OUT, "| (cd $directory; doxygen -)");
    while (<IN>) {
	if (/^OUTPUT_DIRECTORY/) {
	    print OUT "OUTPUT_DIRECTORY = $WEBDIR\n";
	} elsif (/^HTML_OUTPUT/) {
	    print OUT "HTML_OUTPUT = doxygen\n";
	} elsif (/^GENERATE_LATEX/) {
	    print OUT "GENERATE_LATEX = NO\n";
	} else {
	    print OUT;
	}
    }
    close IN;
    close OUT;
}

# 11. create doc.tar.gz
if ($DOC_TAR_GZ) {
    $DOCDIR = "/tmp/%click-webdoc/click-doc-$VERSION";
    mysystem("rm -rf $DOCDIR && mkdir $DOCDIR");
    mysystem("cp $WEBDIR/doc/*.css $DOCDIR");
    mysystem("cp $WEBDIR/_.gif $DOCDIR");
    mysystem("cp $WEBDIR/el_*.gif $DOCDIR");

    opendir(DIR, "$WEBDIR/doc") || die;
    my(@htmlfiles) = grep { /\.html$/ } readdir(DIR);
    closedir(DIR);

    local($/) = undef;
    foreach $f (@htmlfiles) {
	open(IN, "$WEBDIR/doc/$f");
	$_ = <IN>;
	close IN;
	open(OUT, ">$DOCDIR/$f");
	s{src='\.\./}{src='}g;
	s{href='\.\./?'}{href='http://www.pdos.lcs.mit.edu/click/'}g;
	print OUT;
	close OUT;
    }

    mysystem("cd /tmp/%click-webdoc && gtar czf click-doc-$VERSION.tar.gz click-doc-$VERSION");
    mysystem("mv /tmp/%click-webdoc/click-doc-$VERSION.tar.gz $WEBDIR");
}

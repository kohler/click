#!/usr/bin/perl -w

sub mysystem ($) {
  my($sys) = @_;
  print STDERR $sys, "\n";
  my($ret) = system($sys);
  $ret && die "`$sys' failed ($ret)";
}

my($INSTALL) = 1;
my($ELEMENTS) = 1;
my($PROGMAN) = 1;
my($DOC_TAR_GZ) = 1;
my($NEWS) = 1;
while ($ARGV[0] =~ /^-/) {
    $_ = shift @ARGV;
    if (/^-x$/ || /^--no-install$/) {
	$INSTALL = 0;
    } elsif (/^-p$/ || /^--progman$/) {
	$PROGMAN = 1;
	$ELEMENTS = $DOC_TAR_GZ = $NEWS = 0;
    } else {
	die "Usage: ./mkwebdoc.pl [-x] [-p|--progman] CLICKWEBDIR";
    }
}

@ARGV == 1 || die "Usage: ./mkwebdoc.pl [-x] [-p|--progman] CLICKWEBDIR";
my($WEBDIR) = $ARGV[0];
$WEBDIR =~ s/\/+$//;
my($DOCDIR, $EXDIR) = ("$WEBDIR/doc", "$WEBDIR/ex");
-r "$DOCDIR/template" || die "`$DOCDIR/template' not found";
-r "$EXDIR/template" || die "`$EXDIR/template' not found";

# -1. get into correct directory
chdir('..') if !-d 'linuxmodule';
-d 'linuxmodule' || die "must be in CLICKDIR or CLICKDIR/doc";

# 0. create distdir
mysystem("gmake dist") if ($INSTALL);

my($VERSION);
open(MK, 'Makefile') || die "no Makefile";
while (<MK>) {
  if (/VERSION\s*=\s*(\S*)/) {
    $VERSION = $1;
    last;
  }
}
defined $VERSION || die "VERSION not defined in Makefile";
close MK;

# 1. install manual pages and click-pretty tool
if ($INSTALL) {
    mysystem("/bin/rm -rf /tmp/%click-webdoc");
    mysystem("cd click-$VERSION && ./configure --prefix=/tmp/%click-webdoc --disable-linuxmodule --disable-bsdmodule --enable-snmp --enable-ipsec --enable-ip6 --enable-etherswitch --enable-radio --enable-grid --enable-analysis --enable-aqm");
    mysystem("cd click-$VERSION && gmake install-local EXTRA_PROVIDES='linuxmodule bsdmodule ns i586 i686 linux_2_2 linux_2_4' MKELEMMAPFLAGS='--webdoc \"../doc/%s.n.html\"'");
    if ($ELEMENTS) {
	mysystem("cd click-$VERSION && gmake install-doc EXTRA_PROVIDES='linuxmodule bsdmodule ns i586 i686 linux_2_2 linux_2_4'");
    }
    mysystem("cd tools/click-pretty && gmake install");
}

# 1.5. install examples
my(@examples) = ([ 'test.click', 'Trivial Test Configuration' ],
		 [ 'test2.click', 'Simple RED Example' ],
		 [ 'test3.click', 'Simple Scheduler Example' ],
		 [ 'test-device.click', 'Device Test' ],
		 [ 'test-tap.click', 'KernelTap Test' ],
		 [ 'udpgen.click', 'UDP Generator' ],
		 [ 'mazu-nat.click', 'Sample Firewall/NAT' ],
		 [ 'fake-iprouter.click', 'IP Router Simulation' ],
		 [ 'fromhost-tunnel.click', 'Kernel FromHost Tunnel Example' ]);
foreach $i (@examples) {
    my($fn, $title) = ('"' . $i->[0] . '"', '"' . $i->[1] . '"');
    my($html) = $fn;
    $html =~ s/\.click\"$/.html\"/;
    mysystem("click-pretty -C /tmp/%click-webdoc -t $EXDIR/template -dfilename=$fn -dtitle=$title click-$VERSION/conf/$fn > $EXDIR/$html");
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
    $x =~ s/&([^;]*);/xml_quote_entity($1)/eg;
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

	$v = $x{'requires'};
	next if !$v;

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

if ($ELEMENTS) {
    map { expand_ereq($_) } keys %ereq;
}


# 2.2. changetemplate.pl
my(@esubj, @ealpha, @esections, $cocked, %edeprecated);
if ($ELEMENTS) {
    open(IN, "/tmp/%click-webdoc/man/mann/elements.n") || die "/tmp/%click-webdoc/man/mann/elements.n: $!\n";
    while (<IN>) {
	push @{$esections[-1]}, scalar(@esubj) if /^\.SS/ && @esections;
	push @esections, [$1, scalar(@esubj)] if /^\.SS \"(.*)\"/;
	push @esubj, $1 if /^\.M (.*) n/ && $cocked;
	$cocked = ($_ =~ /^\.TP/);
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

sub element_li ($) {
    my($e) = @_;
    my($t) = "<li><a href='$e.n.html'>$e</a>";
    my(@x);
    push @x, "<a href='#D'>D</a>" if $edeprecated{$e};
    if ($ereq{$e}) {
	my($r) = $ereq{$e};
	push @x, "<a href='#U'>U</a>" if $r =~ /\buserlevel\b/;
	push @x, "<a href='#L'>L</a>" if $r =~ /\blinuxmodule\b/;
	push @x, "<a href='#B'>B</a>" if $r =~ /\bbsdmodule\b/;
	push @x, "<a href='#Ns'>Ns</a>" if $r =~ /\bns\b/;
    }
    $t .= " <small>[" . join('&nbsp;', @x) . "]</small>" if @x;
    "$t</li>\n";
}

if ($ELEMENTS) {
    open(IN, "$DOCDIR/index.html") || die "$DOCDIR/index.html: $!\n";
    open(OUT, ">$DOCDIR/index.html.new") || die "$DOCDIR/index.html.new: $!\n";
    while (<IN>) {
	if (/^<!-- clickdoc: ealpha (\d+)\/(\d+)/) {
	    print OUT;
	    my($num, $total) = ($1, $2);
	    my($amt) = int((@ealpha - 1) / $2) + 1;
	    my($index) = ($num - 1) * $amt;
	    for ($i = $index; $i < $index + $amt && $i < @ealpha; $i++) {
		print OUT element_li($ealpha[$i]);
	    }
	    1 while (defined($_ = <IN>) && !/^<!-- \/clickdoc/);
	    print OUT;
	} elsif (/^<!-- clickdoc: esubject (\d+)\/(\d+)/) {
	    print OUT;
	    my($num, $total) = ($1, $2);
	    my($amt) = int((@esubj + 2*@esections - 1) / $2) + 1;
	    my($index) = ($num - 1) * $amt;
	    my($ul) = "<ul>";
	    $ul = "<ul class='$1'>" if m/ulclass='(\w+)'/;

	    # find first section number
	    my($secno, $secno2);
	    for ($secno = 0; $secno < @esections; $secno++) {
		my($diffa, $diffb) = ($esections[$secno]->[1] + 2*$secno - $index, $esections[$secno]->[2] + 2*($secno + 1) - $index);
		last if $diffa >= 0;
		last if $diffb > 0 && $diffa < 0 && -$diffa < $diffb;
	    }

	    # find last section number
	    $index += $amt;
	    for ($secno2 = $secno; $secno2 < @esections; $secno2++) {
		my($diffa, $diffb) = ($esections[$secno2]->[1] + 2*$secno2 - $index, $esections[$secno2]->[2] + 2*($secno2 + 1) - $index);
		last if $diffa >= 0;
		last if $diffb > 0 && $diffa < 0 && -$diffa < $diffb;
	    }

	    # iterate over sections
	    for ($i = $secno; $i < $secno2; $i++) {
		print OUT "<p class='esubject'>", $esections[$i]->[0], "</p>\n";
		print OUT "$ul\n";
		for ($j = $esections[$i]->[1]; $j < $esections[$i]->[2]; $j++) {
		    print OUT element_li($esubj[$j]);
		}
		print OUT "</ul>\n";
	    }
	
	    1 while (defined($_ = <IN>) && !/^<!-- \/clickdoc/);
	    print OUT;
	} else {
	    print OUT;
	}
    }
    close IN;
    close OUT;
    if (system("cmp $DOCDIR/index.html $DOCDIR/index.html.new >/dev/null 2>&1")) {
	unlink("$DOCDIR/index.html") || die "unlink $DOCDIR/index.html: $!\n";
	rename("$DOCDIR/index.html.new", "$DOCDIR/index.html") || die "rename $DOCDIR/index.html.new: $!\n";
    } else {
	unlink("$DOCDIR/index.html.new") || die "unlink $DOCDIR/index.html.new: $!\n";
    }
}

# 3. call `man2html'
mysystem("man2html -l -m '<b>@</b>' -t $DOCDIR/template -d $DOCDIR /tmp/%click-webdoc/man/man*/*.?");

# 5. call `changelog2html'
if ($NEWS) {
    mysystem("changelog2html -d $DOCDIR click-$VERSION/NEWS $WEBDIR/news.html");
}

# 6. edit `news.html'
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
    mysystem("cd click-$VERSION/doc && gmake click.html") if ($INSTALL);

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

# 8. create doc.tar.gz
if ($DOC_TAR_GZ) {
    $DOCDIR = "/tmp/%click-webdoc/click-doc-$VERSION";
    mysystem("rm -rf $DOCDIR && mkdir $DOCDIR");
    mysystem("cp $WEBDIR/doc/*.css $DOCDIR");
    mysystem("cp $WEBDIR/_.gif $DOCDIR");
    mysystem("cp $WEBDIR/el_*.gif $DOCDIR");

    opendir(DIR, "$WEBDIR/doc") || die;
    my(@htmlfiles) = grep { /\.html$/ } readdir(DIR);
    closedir(DIR);

    undef $/;
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

#!/usr/bin/perl -w

sub mysystem ($) {
  my($sys) = @_;
  print STDERR $sys, "\n";
  my($ret) = system($sys);
  $ret && die "`$sys' failed ($ret)";
}

my($INSTALL) = 1;
my($MANUALS) = 1;
while ($ARGV[0] =~ /^-/) {
    $_ = shift @ARGV;
    if (/^-x$/ || /^--no-install$/) {
	$INSTALL = 0;
    } elsif (/^-m$/ || /^--no-manuals$/) {
	$MANUALS = 0;
    } else {
	die "Usage: ./mkwebdoc.pl [-x] [-m] CLICKWEBDIR";
    }
}

@ARGV == 1 || die "Usage: ./mkwebdoc.pl [-x] [-m] CLICKWEBDIR";
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
    mysystem("cd click-$VERSION && ./configure --prefix=/tmp/%click-webdoc --enable-snmp --enable-ipsec --enable-ip6 --enable-etherswitch --enable-radio --enable-grid --enable-analysis --enable-aqm && gmake install-man EXTRA_PROVIDES='linuxmodule i586 i686 linux_2_2 linux_2_4' && gmake install-local EXTRA_PROVIDES='linuxmodule i586 i686 linux_2_2 linux_2_4'");
    mysystem("cd tools/click-pretty && gmake install");
    mysystem("cd /tmp/%click-webdoc/share/click && echo '\$webdoc ../doc/%s.n.html' | cat - elementmap > emap2 && mv emap2 elementmap");
}

# 1.5. install examples
my(@examples) = ([ 'test.click', 'Trivial Test Configuration' ],
		 [ 'test2.click', 'Simple RED Example' ],
		 [ 'test3.click', 'Simple Scheduler Example' ],
		 [ 'test-device.click', 'Device Test' ],
		 [ 'test-tap.click', 'KernelTap Test' ],
		 [ 'udpgen.click', 'UDP Generator' ],
		 [ 'mazu-nat.click', 'Sample Firewall/NAT' ],
		 [ 'fake-iprouter.click', 'IP Router Simulation' ]);
foreach $i (@examples) {
    my($fn, $title) = ('"' . $i->[0] . '"', '"' . $i->[1] . '"');
    my($html) = $fn;
    $html =~ s/\.click\"$/.html\"/;
    mysystem("click-pretty -C /tmp/%click-webdoc -t $EXDIR/template -dfilename=$fn -dtitle=$title click-$VERSION/conf/$fn > $EXDIR/$html || true");
}

# 2.0. read elementmap
exit 0 if !$MANUALS;
open(EMAP, "/tmp/%click-webdoc/share/click/elementmap") || die "/tmp/%click-webdoc/share/click/elementmap: $!\n";
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

while (<EMAP>) {
    s/\"(([^\"]|\\.)*)\"/\370$1\371/g;
    1 while s/(\370[^\371]*)\s/$1\372/;
    my(@x) = split(/\s+/, $_);
    if ($x[0] eq '$data') {
	for ($i = 1; $i < @x; $i++) {
	    $reqindex = $i - 1 if $x[$i] eq 'requirements';
	    $classindex = $i - 1 if $x[$i] eq 'class';
	    $provindex = $i - 1 if $x[$i] eq 'provisions';
	    $docnameindex = $i - 1 if $x[$i] eq 'doc_name';
	}
    } elsif ($x[0] !~ /^\$/ && defined($reqindex) && $reqindex < @x) {
	my($e, $r, $p, $dn) = (em_unquote($x[$classindex]), em_unquote($x[$reqindex]), em_unquote($x[$provindex]), em_unquote($x[$docnameindex]));
	$ereq{$e} = ($ereq{$e} ? 'xxx' : $r) if $e;
	foreach $i (split(/\s+/, $p)) {
	    $ereq{$i} = $r;
	}
    }
}

# 2.1. spread requirements
my(%ereq_expanded);

sub expand_ereq ($) {
    my($e) = @_;
    return $ereq{$e} if $ereq_expanded{$e};
    $ereq_expanded{$e} = 1;
    my(@req) = split(/\s+/, $ereq{$e});
    my($i, $t, $r);
    for ($i = 0; $i < @req; $i++) {
	push @req, split(/\s+/, &expand_ereq($req[$i]))
	    if $ereq{$req[$i]};
    }
    $ereq{$e} = join(' ', @req);
    $ereq{$e};
}

map { expand_ereq($_) } keys %ereq;


# 2.2. changetemplate.pl
my(@esubj, @ealpha, @esections, $cocked, %edeprecated);
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

sub element_li ($) {
    my($e) = @_;
    my($t) = "<li><a href='$e.n.html'>$e</a>";
    my($x) = '';
    $x .= "<a href='#D'>D</a>" if $edeprecated{$e};
    if ($ereq{$e}) {
	my($r) = $ereq{$e};
	$x .= "<a href='#B'>B</a>" if $r =~ /\bbsdmodule\b/;
	$x .= "<a href='#L'>L</a>" if $r =~ /\blinuxmodule\b/;
	$x .= "<a href='#U'>U</a>" if $r =~ /\buserlevel\b/;
    }
    $t .= " <small>[$x]</small>" if $x ne '';
    "$t</li>\n";
}

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
	    for ($j = $esections[$i]->[1]; $j < $esections[$i]->[2]; $j++) {
		print OUT element_li($esubj[$j]);
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
if (system("cmp $DOCDIR/index.html $DOCDIR/index.html.new >/dev/null 2>&1")) {
    unlink("$DOCDIR/index.html") || die "unlink $DOCDIR/index.html: $!\n";
    rename("$DOCDIR/index.html.new", "$DOCDIR/index.html") || die "rename $DOCDIR/index.html.new: $!\n";
} else {
    unlink("$DOCDIR/index.html.new") || die "unlink $DOCDIR/index.html.new: $!\n";
}

# 3. call `man2html'
mysystem("man2html -l -m '<b>@</b>' -t $DOCDIR/template -d $DOCDIR /tmp/%click-webdoc/man/man*/*.?");

# 4. change `elements.n.html' into `index.html'
if (0) {
    open(IN, "$DOCDIR/elements.n.html") || die "$DOCDIR/elements.n.html: $!\n";
    open(OUT, ">$DOCDIR/index.html") || die "$DOCDIR/index.html: $!\n";
    while (<IN>) {
	s|<h1><a.*?>elements</a></h1>|<h1>Click documentation</h1>|;
	s|<p>documented Click element classes||;
	s|<h2><a.*?>DESCRIPTION</a></h2>||;
	s|<a href="index\.html">(.*?)</a>|<b>$1</b>|;
	if (/<p>This page lists all Click element classes that have manual page documentation./) {
	    print OUT <<"EOF";
	    <p>Here is the programmer\'s documentation available for Click. All
these files have been automatically translated from documentation provided
with the distribution, which you can get <a
href=\"http://www.pdos.lcs.mit.edu/click/\">here</a>. You may also be
interested in <a
href=\"http://www.pdos.lcs.mit.edu/papers/click:tocs00/\">our TOCS
paper</a>.</p>
<p>The Click element classes that have manual page documentation are:</p>
EOF
            next;
	}
	print OUT;
    }
    close IN;
    close OUT;
}

# 5. call `changelog2html'
mysystem("changelog2html -d $DOCDIR click-$VERSION/NEWS $WEBDIR/news.html");

# 6. edit `news.html'
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

# 7. install programming manual
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

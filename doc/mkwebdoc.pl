#!/usr/bin/perl -w

sub mysystem ($) {
  my($sys) = @_;
  print STDERR $sys, "\n";
  my($ret) = system($sys);
  $ret && die "`$sys' failed ($ret)";
}

my($INSTALL) = 1;
while ($ARGV[0] =~ /^-/) {
  $_ = shift @ARGV;
  if (/^-x$/ || /^--no-install$/) {
    $INSTALL = 0;
  } else {
    die "Usage: ./mkwebdoc.pl [-x] CLICKWEBDIR";
  }
}

@ARGV == 1 || die "Usage: ./mkwebdoc.pl [-x] CLICKWEBDIR";
my($WEBDIR) = $ARGV[0];
$WEBDIR =~ s/\/+$//;
$WEBDIR .= "/doc" if !-r "$WEBDIR/template";
-r "$WEBDIR/template" || die "`$WEBDIR/template' not found";

# 1. install documentation into fake directory
chdir('..') if -r 'click-install.1';
-d 'linuxmodule' || die "must be in CLICKDIR or CLICKDIR/doc";

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

if ($INSTALL) {
    mysystem("/bin/rm -rf /tmp/%click-webdoc");
    mysystem("cd click-$VERSION && ./configure --prefix=/tmp/%click-webdoc --enable-snmp --enable-ipsec --enable-ip6 --enable-etherswitch --enable-radio --enable-grid --enable-analysis --enable-aqm && gmake install-man EXTRA_MAN_ELEMENTS='linuxmodule i586 i686 linux_2_2 linux_2_4' && gmake install-local");
}

# 1.5. read elementmap
open(EMAP, "/tmp/%click-webdoc/share/click/elementmap") || die "/tmp/%click-webdoc/share/click/elementmap: $!\n";
my(%ereq, $reqindex, $classindex, $provindex);

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
	}
    } elsif ($x[0] !~ /^\$/ && defined($reqindex) && $reqindex < @x) {
	my($e, $r, $p) = (em_unquote($x[$classindex]), em_unquote($x[$reqindex]), em_unquote($x[$provindex]));
	$ereq{$e} = ($ereq{$e} ? 'xxx' : $r) if $e;
	foreach $i (split(/\s+/, $p)) {
	    $ereq{$i} = $r;
	}
    }
}

# 1.6. spread requirements
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


# 2. changetemplate.pl
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

open(IN, "$WEBDIR/index.html") || die "$WEBDIR/index.html: $!\n";
open(OUT, ">$WEBDIR/index.html.new") || die "$WEBDIR/index.html.new: $!\n";
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
if (system("cmp $WEBDIR/index.html $WEBDIR/index.html.new >/dev/null 2>&1")) {
    unlink("$WEBDIR/index.html") || die "unlink $WEBDIR/index.html: $!\n";
    rename("$WEBDIR/index.html.new", "$WEBDIR/index.html") || die "rename $WEBDIR/index.html.new: $!\n";
} else {
    unlink("$WEBDIR/index.html.new") || die "unlink $WEBDIR/index.html.new: $!\n";
}

# 3. call `man2html'
mysystem("man2html -l -m '<b>@</b>' -t $WEBDIR/template -d $WEBDIR /tmp/%click-webdoc/man/man*/*.?");

# 4. change `elements.n.html' into `index.html'
if (0) {
    open(IN, "$WEBDIR/elements.n.html") || die "$WEBDIR/elements.n.html: $!\n";
    open(OUT, ">$WEBDIR/index.html") || die "$WEBDIR/index.html: $!\n";
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
mysystem("changelog2html -d $WEBDIR click-$VERSION/NEWS $WEBDIR/../news.html");

# 6. edit `news.html'
open(IN, "$WEBDIR/../news.html") || die "$WEBDIR/../news.html: $!\n";
open(OUT, ">$WEBDIR/../news.html.new") || die "$WEBDIR/../news.html.new: $!\n";
my(%good);
while (<IN>) {
  while (/\b([A-Z][A-Za-z0-9]*)\b/g) {
    if (!exists $good{$1}) {
      $good{$1} = -r "$WEBDIR/$1.n.html";
    }
  }
  s#\b([A-Z][A-Za-z0-9]*)\b#$good{$1} ? '<a href="doc/' . $1 . '.n.html">' . $1 . '</a>' : $1#eg;
  print OUT;
}
close IN;
close OUT;
unlink("$WEBDIR/../news.html") || die "unlink $WEBDIR/../news.html: $!\n";
rename("$WEBDIR/../news.html.new", "$WEBDIR/../news.html") || die "rename $WEBDIR/../news.html.new: $!\n";

# 7. install programming manual
mysystem("cd click-$VERSION/doc && gmake click.html") if ($INSTALL);

open(IN, "click-$VERSION/doc/click.html") || die "couldn't make click.html";
open(OUT, ">$WEBDIR/progman.html") || die;
open(TMP, "$WEBDIR/template") || die;

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

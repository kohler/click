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
my($VERSION);
if ($INSTALL) {
  chdir('..') if -r 'click-install.1';
  -d 'linuxmodule' || die "must be in CLICKDIR or CLICKDIR/doc";
  mysystem("gmake dist");
  
  open(MK, 'Makefile') || die "no Makefile";
  while (<MK>) {
    if (/VERSION\s*=\s*(\S*)/) {
      $VERSION = $1;
      last;
    }
  }
  defined $VERSION || die "VERSION not defined in Makefile";
  close MK;
  
  mysystem("/bin/rm -rf /tmp/%click-webdoc");
  mysystem("cd click-$VERSION && ./configure --prefix=/tmp/%click-webdoc && gmake install-man");
}

# 2. changetemplate.pl
my(@elements);
open(IN, "/tmp/%click-webdoc/man/mann/elements.n") || die "/tmp/%click-webdoc/man/mann/elements.n: $!\n";
while (<IN>) {
  last if (/^\.SH "ALPHABETICAL/);
}
while (<IN>) {
  push @elements, $1 if /^\.M (.*) n/;
}
close IN;

open(IN, "$WEBDIR/template") || die "$WEBDIR/template: $!\n";
open(OUT, ">$WEBDIR/template.new") || die "$WEBDIR/template.new: $!\n";
my($active) = 0;
while (<IN>) {
  if ($active == 1 && m{</p}) {
    $active = -1;
  } elsif ($active == 0 && m{<!-- elements go here -->}) {
    print OUT;
    foreach $_ (sort { lc($a) cmp lc($b) } @elements) {
      print OUT "<a href=\"$_.n.html\">$_</a><br>\n";
    }
    $active = 2;
    next;
  } elsif ($active == 2 && m{\.n\.html}) {
    next;
  }
  print OUT;
}
close IN;
close OUT;
if (system("cmp $WEBDIR/template $WEBDIR/template.new")) {
  unlink("$WEBDIR/template") || die "unlink $WEBDIR/template: $!\n";
  rename("$WEBDIR/template.new", "$WEBDIR/template") || die "rename $WEBDIR/template.new: $!\n";
} else {
  unlink("$WEBDIR/template.new") || die "unlink $WEBDIR/template.new: $!\n";
}

# 3. call `man2html'
mysystem("man2html -l -m '<b>@</b>' -t $WEBDIR/template -d $WEBDIR /tmp/%click-webdoc/man/man*/*.?");

# 4. change `elements.n.html' into `index.html'
open(IN, "$WEBDIR/elements.n.html") || die "$WEBDIR/elements.n.html: $!\n";
open(OUT, ">$WEBDIR/index.html") || die "$WEBDIR/index.html: $!\n";
while (<IN>) {
  s|<h1><a.*?>elements</a></h1>|<h1>Click documentation</h1>|;
  s|<p>documented Click element classes||;
  s|<h2><a.*?>DESCRIPTION</a></h2>||;
  s|<a href="index\.html">(.*?)</a>|<b>$1</b>|;
  if (/<p>This page lists all Click element classes that have manual page documentation./) {
    print OUT <<'EOF';
<p>Here is the programmer's documentation available for Click. All
these files have been automatically translated from documentation provided
with the distribution, which you can get <a
href="http://www.pdos.lcs.mit.edu/click/">here</a>. You may also be
interested in <a
href="http://www.pdos.lcs.mit.edu/papers/click:tocs00/">our TOCS
paper</a>.</p>
<p>The Click element classes that have manual page documentation are:</p>
EOF
    next;
  }
  print OUT;
}
close IN;
close OUT;

# 5. call `changelog2html'
if ($INSTALL) {
  mysystem("changelog2html -d $WEBDIR click-$VERSION/NEWS $WEBDIR/../news.html");
}

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

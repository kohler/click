#!/usr/local/bin/perl -w

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
if ($INSTALL) {
  chdir('..') if !-d 'elements';
  -d 'elements' || die "must be in CLICKDIR or CLICKDIR/doc";
  mysystem("gmake dist");
  
  open(MK, 'Makefile') || die "no Makefile";
  my($VERSION);
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
  if (/^\.M (.*) n/) {
    push @elements, $1;
  }
}
close IN;

open(IN, "$WEBDIR/template") || die "$WEBDIR/template: $!\n";
open(OUT, ">$WEBDIR/template.new") || die "$WEBDIR/template.new: $!\n";
my($active) = 0;
while (<IN>) {
  if ($active == 0 && /elements\.n\.html/) {
    $active = 1;
  } elsif ($active == 1 && m{</p}) {
    $active = -1;
  } elsif ($active == 1 && m{\.n\.html}) {
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

# 3. change template to index.html
open(IN, "$WEBDIR/template") || die "$WEBDIR/template: $!\n";
open(OUT, ">$WEBDIR/index.html") || die "$WEBDIR/index.html: $!\n";
while (<IN>) {
  if (m|(.*)<title>.*</title>(.*)|) {
    print OUT $1, "<title>Click documentation</title>", $2;
  } elsif (m|(.*)<a href="index.html">(.*?)</a>(&nbsp;&gt;)?(.*)|) {
    print OUT $1, "<b>", $2, "</b>", $4;
  } elsif (m|(.*)<b>&mantitle;</b>(.*)|) {
    print OUT $1, $2;
  } else {
    print OUT;
  }
}
close IN;
close OUT;

# 4. call `man2html'
mysystem("man2html -l -m '<b>@</b>' -t $WEBDIR/template -d $WEBDIR /tmp/%click-webdoc/man/man*/*.?");

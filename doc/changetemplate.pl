#!/usr/local/bin/perl -w

die "./changetemplate.pl elements.n TEMPLATE" if @ARGV != 2;
my($elements, $template) = @ARGV;
die "./changetemplate.pl elements.n TEMPLATE" if $elements !~ /elements\.n/ && $elements ne '-';

my(@elements);
if ($elements eq '-') {
  open(IN, "<&STDIN");
} else {
  open(IN, $elements) || die "$elements: $!\n";
}
while (<IN>) {
  if (/^\.M (.*) n/) {
    push @elements, $1;
  }
}
close IN if $elements ne '-';

open(IN, "$template") || die "$template: $!\n";
open(OUT, ">$template.new") || die "$template.new: $!\n";
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
unlink("$template") || die "unlink $template: $!\n";
rename("$template.new", "$template") || die "rename $template.new: $!\n";

#!/usr/local/bin/perl -w

# element2man.pl -- creates man pages from structured comments in element
# source code
# Eddie Kohler
# Robert Morris - original make-faction-html script
#
# Copyright (c) 1999 Massachusetts Institute of Technology.
#
# This software is being provided by the copyright holders under the GNU
# General Public License, either version 2 or, at your discretion, any later
# version. For more information, see the `COPYRIGHT' file in the source
# distribution.

my(%section_break) =
    ( 'head1' => 1, 'c' => 1, 's' => 1, 'io' => 1, 'processing' => 1,
      'd' => 1, 'n' => 1, 'e' => 1, 'h' => 1, 'a' => 1, 'title' => 1 );
my(%section_takes_args) =
    ( 'head1' => 1, 'c' => 0, 's' => 2, 'io' => 0, 'processing' => 0,
      'd' => 0, 'n' => 0, 'e' => 0, 'h' => 1, 'a' => 2, 'title' => 1 );
my(%section_takes_text) =
    ( 'head1' => 1, 'c' => 1, 's' => 2, 'io' => 1, 'processing' => 1,
      'd' => 1, 'n' => 1, 'e' => 1, 'h' => 1, 'a' => 2, 'title' => 0 );
my(%xsection_takes_args) =
    ( 'head1' => 1, 'head2' => 1, 'item' => 1, 'over' => 1, 'back' => 0,
      'for' => 1, 'begin' => 1, 'end' => 1 );

my(%podentities) =
    ( 'lt' => '<', 'gt' => '>', 'amp' => '&', 'solid' => '/',
      'verbar' => '|', 'eq' => '=' );

my $directory;
my $section = 'n';
my(@all_outnames, %all_outsections, %all_summaries, %all_roff_summaries,
   %class_name, %processing);

my(%processing_constants) =
    ( 'AGNOSTIC' => 'a/a', 'PUSH' => 'h/h', 'PULL' => 'l/l',
      'PUSH_TO_PULL' => 'h/l', 'PULL_TO_PUSH' => 'l/h' );
my(%processing_text) =
    ( 'a/a' => 'Agnostic', 'h/h' => 'Push', 'l/l' => 'Pull',
      'h/l' => 'Push inputs, pull outputs',
      'l/h' => 'Pull inputs, push outputs',
      'a/ah' => 'Agnostic, but output 1 is push' );

# find date
my($today) = '';
if (localtime =~ /\w*\s+(\w*)\s+(\d*)\s+\S*\s+(\d*)/) {
  $today = "$2/$1/$3";
}

my $prologue = <<'EOD;';
.de M
.IR "\\$1" "(\\$2)\\$3"
..
.de RM
.RI "\\$1" "\\$2" "(\\$3)\\$4"
..
EOD;
chomp $prologue;

# Unrolling [^A-Z>]|[A-Z](?!<) gives:    // MRE pp 165.
my $nonest = '(?:[^A-Z>]*(?:[A-Z](?!<)[^A-Z>]*)*)';



# XXX one paragraph at a time

my($Filename, @Related, %RelatedSource, @Over, $Begun);

sub quote ($) {
  my($x) = @_;
  $x =~ tr/\000-\177/\200-\377/;
  $x;
}

sub unentity ($) {
  my($x) = @_;
  $x =~ tr/\200-\377/\000-\177/;
  if ($x =~ /^\d+$/) {
    chr($x);
  } elsif ($podentities{$x}) {
    $podentities{$x};
  } else {
    print STDERR "$Filename: unknown entity E<$x>\n";
    "";
  }
}

sub fixfP ($$) {
  my($x, $f) = @_;
  $x =~ s/\\fP/\\f$f/g;
  $x;
}

sub nroffize_text ($) {
  my($t) = @_;
  my($i);

  # embolden & manpageize
  foreach $i (@Related) {
    $t =~ s{(^|[^\w@/<])($i)($|[^\w@/]|[A-Z]<)}{$1L<$2>$3}gs;
  }

  $t =~ s/\\/\\\\/g;
  $t =~ s/^\./$1\\&./gm;
  $t =~ s/^'/$1\\&'/gm;
  $t =~ s/^\s*$/.PP\n/gm;
  $t =~ s/^(\.PP\n)+/.PP\n/gm;
  $t =~ s{\A\s*(\.PP\n)?}{}s;
  if (@Over > 0) {
    $t =~ s/^\.PP$/.IP \"\" $Over[-1]/gm;
  }

  # get rid of entities
  $t =~ s{[\200-\377]}{"E<" . ord($1) . ">"}ge;
  $t =~ s{(E<[^>]*>)}{quote($1)}ge;
  
  my $maxnest = 10;
  while ($maxnest-- && $t =~ /[A-Z]</) {
    
    # can't do C font here
    $t =~ s/([BIR])<($nonest)>/"\\f$1" . fixfP($2, $1) . "\\fP"/eg;
    
    # files and filelike refs in italics
    $t =~ s/F<($nonest)>/I<$1>/g;
    
    # LREF: man page references
    $t =~ s{L<($nonest)\(([\dln])\)>(\S*)(\s*)}{\n.M $1 $2 $3\n}g;
    
    $t =~ s/V<($nonest)>//g;
    
    # LREF: a la HREF L<show this text|man/section>
    $t =~ s{L<($nonest)>(\S*)(\s*)}{
    if ($RelatedSource{$1}) {
      "\n.M $1 \"$RelatedSource{$1}\" $2\n";
    } else {
      $i = index($1, "|");
      ($i >= 0 ? substr($1, 0, $i) . $2 . $3 : "\\fB$1\\fP$2$3");
    }
    }eg;
    
    $t =~ s/Z<>/\\&/g;
    $t =~ s/N<>(\n?)/\n.br\n/g;

    # comes last because not subject to reprocessing
    $t =~ s/C<($nonest)>/"\\f(CW" . fixfP($1, 'R') . "\\fP"/eg;
  }

  # replace entities
  $t =~ s/\305\274([^\276]*)\276/unentity($1)/ge;
  
  # fix fonts
  $t =~ s/\\fP/\\fR/g;
    
  $t =~ s/\n+/\n/sg;
  $t =~ s/^\n+//;
  $t =~ s/\n+$//;
  $t;
}

sub nroffize ($) {
  my($t) = @_;

  $t =~ s{\n[ \t\r]+$}{\n}gm;
  
  if ($t =~ /^[ \t]/m) {
    # worry about verbatims
    my(@x) = split(/(^[ \t].*$)/m, $t);
    my($o, $i) = '';
    for ($i = 0; $i < @x; $i += 2) {
      $o .= nroffize_text($x[$i]) . "\n" if $x[$i];
      if ($x[$i+1]) {
	$x[$i+1] =~ s/\t/        /g;
	$x[$i+1] =~ s/\\/\\e/g;
	$o .= ".nf\n\\&" . $x[$i+1] . "\n.fi\n.PP\n";
      }
    }
    $o =~ s/\n\.fi\n\.PP\n\n\.nf\n/\n/g;
    $o;
  } else {
    nroffize_text($t);
  }
}

sub do_section_x($$$) {
  my($name, $args, $text) = @_;
  if (!exists($xsection_takes_args{$name})) {
    print STDERR "$Filename: unknown section \`=$name' ignored\n";
    return;
  }
  print STDERR "$Filename: section \`=$name' requires arguments\n"
      if ($xsection_takes_args{$name} && !$args);
  print STDERR "$Filename: section \`=$name' arguments ignored\n"
      if (!$xsection_takes_args{$name} && $args);

  # handle `=begin' .. `=end'
  if ($name eq 'end') {
    undef $Begun;
  } elsif ($Begun && ($Begun eq 'man' || $Begun eq 'roff')) {
    print OUT '=', $name, ($args ? ' ' . $args : ''), "\n", $text;
    return;
  } elsif ($Begun) {
    return;
  }
  
  if ($name eq 'head1') {
    print OUT ".SH \"", nroffize($args), "\"\n";
  } elsif ($name eq 'head2') {
    print OUT ".SS \"", nroffize($args), "\"\n";
  } elsif ($name eq 'over') {
    if ($args =~ /^\s*(\d+)\s*/s) {
      print OUT ".RS $Over[-1]\n" if @Over;
      push @Over, $1;
    } else {
      print STDERR "$Filename: bad arguments to \`=over'\n";
    }
  } elsif ($name eq 'item') {
    if (@Over == 0) {
      print STDERR "$Filename: \`=item' outside any \`=over' section\n";
    } else {
      print OUT ".IP \"", nroffize($args), "\" $Over[-1]\n";
    }
  } elsif ($name eq 'back') {
    if (@Over == 0) {
      print STDERR "$Filename: too many \`=back's\n";
    } else {
      pop @Over;
      print OUT ".RE\n" if @Over;
      print OUT (@Over ? ".IP \"\" $Over[-1]\n" : ".PP\n");
    }
  } elsif ($name eq 'for') {
    my($fortext);
    if ($text =~ /^(.*)\n\s*\n(.*)$/s) {
      ($fortext, $text) = ($1, $2);
    } else {
      ($fortext, $text) = ($text, '');
    }
    if ($args =~ /^\s*(man|roff)\s*(.*)/) {
      print OUT $2, $fortext;
    }
  } elsif ($name eq 'begin') {
    $Begun = $args;
    $Begun =~ s/^\s*(\S+).*/$1/;
    if ($Begun eq 'man' || $Begun eq 'roff') {
      print OUT $text;
    }
    return;
  }
  print OUT nroffize($text), "\n";
}

sub do_section($$$) {
  my($name, $args, $text) = @_;
  my(@text) = split(/^(=\w.*)$/m, $text);
  my($i);
  @Over = ();
  undef $Begun;
  for ($i = 0; $i < @text; ) {
    do_section_x($name, $args, $text[$i]);
    ($name, $args) = ($text[$i+1] =~ /=(\w+)\s*(.*)/)
	if ($i < @text - 1);
    $i += 2;
  }
  do_section_x('back', '', '') while @Over;
  print STDERR "$Filename: \`=begin' not closed by end of section\n"
      if $Begun;
}

sub process_processing ($) {
  my($t) = @_;
  if (exists($processing_constants{$t})) {
    $t = $processing_constants{$t};
  }
  $t =~ tr/\" \t//d;
  $t =~ s{\A([^/]*)\Z}{$1/$1};
  if (exists($processing_text{$t})) {
    return $processing_text{$t};
  }
  return undef;
}

sub process_comment ($$) {
  my($t, $filename) = @_;
  my($i);
  $Filename = $filename;

  # split document into sections
  my(@section_text, @section_args, @section_name, $bad_section, $ref);
  $ref = \$bad_section;
  while ($t =~ m{^=(\w+)([ \t]*)(.*)([\0-\377]*?)(?=^=\w|\Z)}mg) {
    if ($section_break{$1}) {
      push @section_name, $1;
      push @section_args, $3;
      push @section_text, $4;
      $ref = \$section_text[-1];
    } else {
      $$ref .= '=' . $1 . $2 . $3 . $4;
    }
  }

  # check document for sectioning errors
  print STDERR "$Filename: warning: comment does not start with section\n"
      if $bad_section;
  my(%num_sections, %first_in_section);
  foreach $i (0..$#section_name) {
    my($n) = $section_name[$i];
    print STDERR "$Filename: warning: section \`=$n' requires arguments\n"
	if $section_takes_args{$n} == 1 && !$section_args[$i];
    print STDERR "$Filename: warning: section \`=$n' arguments ignored\n"
	if $section_takes_args{$n} == 0 && $section_args[$i];
    print STDERR "$Filename: warning: empty section \`=$n'\n"
	if $section_takes_text{$n} == 1 && !$section_text[$i];
    print STDERR "$Filename: warning: section \`=$n' text ignored\n"
	if $section_takes_text{$n} == 0 && $section_text[$i] =~ /\S/;
    $num_sections{$n}++;
    $first_in_section{$n} = $i if $num_sections{$n} == 1;
  }
  foreach $i ('a', 'c', 'd', 'n', 'e', 'title', 'io', 'processing') {
    print STDERR "$Filename: warning: multiple \`=$i' sections; some may be ignored\n"
	if $num_sections{$i} && $num_sections{$i} > 1;
  }
  
  # read class names from configuration arguments section
  $i = $first_in_section{'c'};
  if (!defined($i)) {
    print STDERR "$Filename: section \`=c' missing; cannot continue\n";
    return;
  }
  my(@classes, %classes);
  while ($section_text[$i] =~ /^\s*(\w+)\(/mg) {
    push @classes, $1 if !exists $classes{$1};
    $classes{$1} = 1;
  }
  if (!@classes && $section_text[$i] =~ /^\s*([\w@]+)\s*$/) {
    push @classes, $1;
    $classes{$1} = 1;
  }
  if (!@classes) {
    print STDERR "$Filename: no class definitions\n    (did you forget `()' in the =c section?)\n";
    return;
  }

  # output filenames might be specified in 'title' section
  my(@outfiles, @outsections, $title);
  if (defined($first_in_section{'title'})) {
    $title = $section_args[ $first_in_section{'title'} ];
    if (!$title) {
      print STDERR "$Filename: \`=title' section present, but empty\n";
      return;
    }
    if ($title =~ /[^-.\w@+,]/) {
      print STDERR "$Filename: strange characters in \`=title', aborting\n";
      return;
    }
    foreach $i (split(/\s+/, $title)) {
      if ($i =~ /^(.*)\((.*)\)$/) {
	push @outfiles, $1;
	push @outsections, $2;
      } else {
	push @outfiles, $i;
	push @outsections, $section;
      }
    }
  } else {
    $title = join(', ', @classes);
    @outfiles = @classes;
    @outsections = ($section) x @classes;
  }

  # open new output file if necessary
  my($main_outname);
  if ($directory) {
    $main_outname = "$directory/$outfiles[0].$outsections[0]";
    if (!open(OUT, ">$main_outname")) {
      print STDERR "$main_outname: $!\n";
      return;
    }
  }
  push @all_outfiles, $outfiles[0];
  $all_outsections{$outfiles[0]} = $outsections[0];

  # prepare related
  %RelatedSource = ();
  $i = $first_in_section{'a'};
  if (defined($i)) {
    $section_text[$i] = $section_args[$i] . $section_text[$i]
	if $section_args[$i];
    if ($section_text[$i] =~ /\A\s*(.*?)(\n\s*\n.*\Z|\Z)/s) {
      my($bit, $last) = ($1, $2);
      while ($bit =~ m{(\b[A-Z][-\w@.+=]+)([,\s]|\Z)}g) {
	$RelatedSource{$1} = 'n';
      }
      $bit =~ s{([-\w@.+=]+)([,\s]|\Z)}{$1(n)$2}g;
      while ($bit =~ m{([-\w@.+=]+\(([0-9ln])\))}g) {
	$RelatedSource{$1} = $2;
      }
      $section_text[$i] = $bit . $last;
    }
  }
  map(delete $RelatedSource{$_}, @outfiles);
  @Related = sort { length($b) <=> length($a) } (keys %RelatedSource, @classes);
  @Related = map { s{([][^$()|\\.])}{\\$1}g; $_ } @Related;

  # front matter
  my($oneliner) = (@classes == 1 ? "Click element" : "Click elements");
  $i = $first_in_section{'s'};
  if (defined($i)) {
    $section_text[$i] = $section_args[$i] . "\n" . $section_text[$i]
	if $section_args[$i];
    $all_summaries{$outfiles[0]} = $section_text[$i];
    $section_text[$i] =~ s/\n\s*\n/\n/g;
    my($t) = nroffize($section_text[$i]);
    $oneliner .= ";\n" . $t;
    $oneliner =~ s/\n(^\.)/ /g;
    $all_roff_summaries{$outfiles[0]} = $t;
  } else {
    # avoid uninitialized value warns
    $all_summaries{$outfiles[0]} = $all_roff_summaries{$outfiles[0]} = '';
  }
  
  print OUT <<"EOD;";
.\\" -*- mode: nroff -*-
.\\" Generated by \`element2man.pl' from \`$Filename'
$prologue
.TH "\U$title\E" $outsections[0] "$today" "Click"
.SH "NAME"
$title \- $oneliner
EOD;
  
  # process order
  my(@nsec_name, @nsec_args, @nsec_text);
  my($insert_processing) = -1;
  if (!defined($first_in_section{'processing'})) {
    $insert_processing = $first_in_section{'io'};
    if (!defined($insert_processing)) {
      $insert_processing = $first_in_section{'c'};
    } else {
      $insert_processing = -1
	  if $section_text[$insert_processing] =~ /None/;
    }
  }
  
  for ($i = 0; $i < @section_text; $i++) {
    my($s) = $section_name[$i];
    my($x) = $section_text[$i];
    if ($s eq 'c') {
      $x =~ s{(\S\s*)\n}{$1N<>\n}g;
      do_section('head1', 'SYNOPSIS', $x);
    } elsif ($s eq 'io') {
      do_section('head1', 'INPUTS AND OUTPUTS', $x);
    } elsif ($s eq 'processing') {
      do_section('head1', 'PROCESSING TYPE', $x);
    } elsif ($s eq 'd') {
      do_section('head1', 'DESCRIPTION', $x);
    } elsif ($s eq 'n') {
      do_section('head1', 'NOTES', $x);
    } elsif ($s eq 'e') {
      do_section('head1', 'EXAMPLES', $x);
    } elsif ($s eq 'h') {
      my($t) = "=over 5\n";
      while ($i < @section_text && $section_name[$i] eq 'h') {
	if ($section_args[$i] =~ /\A\s*(\S+)\s*(\S+)\s*\Z/) {
	  $t .= "=item B<$1> ($2)\n";
	} else {
	  print STDERR "$Filename: bad handler section arguments (\`=h $section_args[$i]')\n";
	  $t .= "=item B<$section_args[$i]>\n";
	}
	$t .= $section_text[$i] . "\n";
	$i++;
      }
      $i--;
      do_section('head1', 'ELEMENT HANDLERS', $t);
    } elsif ($s eq 'a') {
      do_section('head1', 'SEE ALSO', $x);
    } elsif ($s eq 'title' || $s eq 's') {
      # nada
    } else {
      do_section($s, $section_args[$i], $x);
    }
    if ($i == $insert_processing) {
      my($can) = 1;
      my($ptype, $j);
      for ($j = 0; $j < @classes && $can; $j++) {
	my($t) = $processing{$classes[$j]};
	$can = 0 if !$t || (defined($ptype) && $t ne $ptype);
	$ptype = $t;
      }
      $ptype = process_processing($ptype)
	  if ($can && $ptype);
      do_section('head1', 'PROCESSING TYPE', $ptype)
	  if ($can && $ptype);
    }
  }

  # close output file & make links if appropriate
  if ($directory) {
    close OUT;
    for ($i = 1; $i < @outfiles; $i++) {
      my($outname) = "$directory/$outfiles[$i].$outsections[$i]";
      unlink($outname);
      if (link $main_outname, $outname) {
	push @all_outfiles, $outfiles[$i];
	$all_outsections{$outfiles[$i]} = $outsections[$i];
	$all_summaries{$outfiles[$i]} = $all_summaries{$outfiles[0]};
	$all_roff_summaries{$outfiles[$i]} = $all_roff_summaries{$outfiles[0]};
      } else {
	print STDERR "$outname: $!\n";
      }
    }
  }
}

sub process_file ($) {
  my($filename) = @_;
  $filename =~ s/\.cc$/\.hh/;
  if (!open(IN, $filename)) {
    print STDERR "$filename: $!\n";
    return;
  }
  my $text = <IN>;
  close IN;

  foreach $_ (split(m{^class}m, $text)) {
    my($cxx_class) = (/^\s*(\w*)/);
    if (/class_name.*return\s*\"([^\"]+)\"/) {
      $class_name{$cxx_class} = $1;
      $cxx_class = $1;
    }
    if (/processing.*return\s+(.*?);/) {
      $processing{$cxx_class} = $1;
    }
  }

  foreach $_ (split(m{(/\*.*?\*/)}s, $text)) {
    if (/^\/\*/ && /^[\/*\s]+=/) {
      s/^\/\*\s*//g;
      s/\s*\*\/$//g;
      s/^ ?\* ?//gm;
      process_comment($_, $filename);
    }
  }
}

# main program: parse options
sub read_files_from ($) {
  my($fn) = @_;
  if (open(IN, ($fn eq '-' ? "<&STDIN" : $fn))) {
    my(@a, @b, $t);
    $t = <IN>;
    close IN;
    @a = split(/\s+/, $t);
    foreach $t (@a) {
      next if $t eq '';
      if ($t =~ /[*?\[]/) {
	push @b, glob($t);
      } else {
	push @b, $t;
      }
    }
    @b;
  } else {
    print STDERR "$fn: $!\n";
    ();
  }
}

sub usage () {
  print STDERR "Usage: element2man.pl [OPTIONS] FILES...
Try \`element2man.pl --help' for more information.\n";
  exit 1;
}

sub help () {
  print STDERR <<'EOD;';
`Element2man.pl' translates Click element documentation into manual pages.

Usage: element2man.pl [-l] [-d DIRECTORY] [-f FILE | FILE]...

Each FILE is a Click header file. `-' means standard input.

Options:
  -l, --list              Generate the elements(n) manual page as well.
  -d, --directory DIR     Place generated manual pages in directory DIR.
  -f, --files FILE        Read header filenames from FILE.
  -h, --help              Print this message and exit.

Report bugs to <click@pdos.lcs.mit.edu>.
EOD;
  exit 0;
}

undef $/;
my(@files, $fn, $elementlist);
while (@ARGV) {
  $_ = shift @ARGV;
  if (/^-d$/ || /^--directory$/) {
    usage() if !@ARGV;
    $directory = shift @ARGV;
  } elsif (/^--directory=(.*)$/) {
    $directory = $1;
  } elsif (/^-f$/ || /^--files$/) {
    usage() if !@ARGV;
    push @files, read_files_from(shift @ARGV);
  } elsif (/^--files=(.*)$/) {
    push @files, read_files_from($1);
  } elsif (/^-l$/ || /^--list$/) {
    $elementlist = 1;
  } elsif (/^-h$/ || /^--help$/) {
    help();
  } elsif (/^-./) {
    usage();
  } elsif (/^-$/) {
    push @files, "-";
  } else {
    push @files, glob($_);
  }
}
push @files, "-" if !@files;

umask(022);
open(OUT, ">&STDOUT") if !$directory;
foreach $fn (@files) {
  process_file($fn);
}
close OUT if !$directory;

my(%el_generated);
sub one_elementlist (@) {
  my($t);
  $t .= ".PP\n.PD 0\n";
  foreach $_ (sort { lc($a) cmp lc($b) } @_) {
    $t .= ".TP 20\n.M " . $_ . " " . $all_outsections{$_} . "\n";
    $t .= $all_roff_summaries{$_} . "\n"
	if $all_roff_summaries{$_};
    $el_generated{$_} = 1;
  }
  $t . ".PD\n";
}

my(@Links, $Text);

sub one_summary ($&) {
  my($name, $func) = @_;
  my($a) = $name;
  $a =~ s{([+\&\#\"\000-\037\177-\377])}{sprintf("%%%02X", $1)}eg;
  $a =~ tr/ /+/;
  my($x) = $name;
  $x =~ s{ }{&nbsp;}g;
  push @Links, "<a href=\"#$a\">$x</a>";
  $Text .= ".SS \"$name\"\n";
  $Text .= one_elementlist(grep(&$func, @all_outfiles));
};

sub make_elementlist () {
  if ($directory) {
    if (!open(OUT, ">$directory/elements.$section")) {
      print STDERR "$directory/elements.$section: $!\n";
      return;
    }
  }
  print OUT <<"EOD;";
.\\" -*- mode: nroff -*-
.\\" Generated by \`element2man.pl'
$prologue
.TH "ELEMENTS" $section "$today" "Click"
.SH "NAME"
elements \- documented Click element classes
.SH "DESCRIPTION"
This page lists all Click element classes that have manual page documentation.
EOD;

  $Text = "";
  my($s) = \%all_summaries;
  one_summary('Network Devices',	sub { $s->{$_} =~ /\bdevices\b/i });
  one_summary('Other Sources and Sinks', sub { $s->{$_} =~ /\bsources\b|\bsinks\b/i });
  one_summary('Classification',		sub { $s->{$_} =~ /\bclassification\b/i });
  one_summary('Checking Validity',	sub { $s->{$_} =~ /\bchecking\b/i });
  one_summary('Duplication',		sub { $s->{$_} =~ /\bduplication\b/i });
  one_summary('Storage',		sub { $s->{$_} =~ /\bstorage\b/i });
  one_summary('Dropping',		sub { $s->{$_} =~ /\bdropping\b/i });
  one_summary('Packet Scheduling',	sub { $s->{$_} =~ /\bpacket\s+scheduling\b|\bpull-to-push\b/i });
  one_summary('Encapsulation',		sub { $s->{$_} =~ /\bencapsulation\b/i });
  one_summary('Modification',		sub { $s->{$_} =~ /\bmodification\b/i });
  one_summary('Annotations',		sub { $s->{$_} =~ /\bannotations?\b/i });
  one_summary('Measurement',		sub { $s->{$_} =~ /\bmeasurement\b/i });
  one_summary('Debugging and Profiling', sub { $s->{$_} =~ /\bdebugging\b/i });
  one_summary('Informational Elements',	sub { $s->{$_} =~ /\binformation\b/i });
  one_summary('Ethernet',		sub { $s->{$_} =~ /\bEthernet\b|\bARP\b/i });
  one_summary('IP, ICMP, and IGMP',	sub { $s->{$_} =~ /\bIP\b|\bICMP\b|\bIGMP\b/i && $s->{$_} !~ /\bTCP\b|\bUDP\b|\bEthernet\b/i });
  one_summary('UDP and TCP',		sub { $s->{$_} =~ /\bTCP\b|\bUDP\b/i });
  one_summary('IPv6',			sub { $s->{$_} =~ /\bIPv6\b/i });
  one_summary('Miscellaneous',		sub { !$el_generated{$_} });

  my($links) = join("&nbsp;- ", @Links);
  print OUT <<"EOD;";
.\\"html <p><a href="#BY+FUNCTION"><b>By Function</b></a>:
.\\"html $links<br>
.\\"html <a href="#ALPHABETICAL+LIST"><b>Alphabetical List</b></a></p>
EOD;

  print OUT ".SH \"BY FUNCTION\"\n";
  print OUT $Text;
  print OUT ".SH \"ALPHABETICAL LIST\"\n";
  print OUT one_elementlist(@all_outfiles);

  close OUT if $directory;
}

if ($elementlist && @all_outfiles) {
  make_elementlist();
}

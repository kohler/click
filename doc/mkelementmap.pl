#!/usr/local/bin/perl

# mkelementmap.pl -- make map of element name to C++ class and file
# Eddie Kohler
#
# Copyright (c) 1999 Massachusetts Institute of Technology.
#
# This software is being provided by the copyright holders under the GNU
# General Public License, either version 2 or, at your discretion, any later
# version. For more information, see the `COPYRIGHT' file in the source
# distribution.

my(%processing_constants) =
    ('AGNOSTIC' => 'a/a', 'PUSH' => 'h/h', 'PULL' => 'l/l',
     'PUSH_TO_PULL' => 'h/l', 'PULL_TO_PUSH' => 'l/h');
%processing = ('Element' => 'a/a');
my(@class_file, @class_name, @cxx_name,
   @parents, @processing, @requirements, @provisions,
   %cxx_name_to_id);

sub process_file ($) {
  my($filename) = @_;
  my($headername) = $filename;
  $headername =~ s/\.cc$/\.hh/;
  if (!open(IN, $headername)) {
    print STDERR "$headername: $!\n";
    return;
  }
  my $text = <IN>;
  close IN;

  my $first;
  $first = @cxx_name;
  foreach $_ (split(m{^class(?=.*\{)}m, $text)) {
    my($cxx_class) = (/^\s*(\w+)(\s|:\s).*\{/);
    next if !$cxx_class;
    push @cxx_name, $cxx_class;
    push @class_file, $headername;
    $cxx_name_to_id{$cxx_class} = @cxx_name - 1;
    if (/\A\s*\w*\s*:\s*([\w\s,]+)/) {
      my $p = $1;
      $p =~ s/\bpublic\b//g;
      push @parents, [ split(/[\s,]+/, $p) ];
    } else {
      push @parents, [];
    }
    if (/class_name.*return\s*\"([^\"]+)\"/) {
      push @class_name, $1;
    } else {
      push @class_name, "";
    }
    if (/processing.*return\s*(.*?);/) {
      my $p = $1;
      $p = $processing_constants{$p} if exists($processing_constants{$p});
      $p =~ tr/\"\s//d;
      $p =~ s{\A([^/]+)\Z}{$1/$1};
      push @processing, $p;
    } else {
      push @processing, "";
    }
  }

  # process ELEMENT_REQUIRES and ELEMENT_PROVIDES
  if (!open(IN, $filename)) {
    print STDERR "$filename: $!\n";
    return;
  }
  $text = <IN>;
  close IN;

  my($req, $prov, $i) = ('', '');
  $req .= " " . $1 while $text =~ /^ELEMENT_REQUIRES\((.*)\)/mg;
  $prov .= " " . $1 while $text =~ /^ELEMENT_PROVIDES\((.*)\)/mg;
  $req =~ s/^\s+//;
  $req =~ s/"/\\"/g;
  $prov =~ s/^\s+//;
  $prov =~ s/"/\\"/g;
  for ($i = $first; $i < @processing; $i++) {
    push @requirements, $req;
    push @provisions, $prov;
  }
}

sub parents_processing ($) {
  my($classid) = @_;
  if (!$processing[$classid]) {
    my($parent);
    foreach $parent (@{$parents[$classid]}) {
      if ($parent eq 'Element') {
	$processing[$classid] = 'a/a';
	last;
      } elsif ($parent ne '') {
	$processing[$classid] = parents_processing($cxx_name_to_id{$parent});
	last if $processing[$classid];
      }
    }
  }
  return $processing[$classid];
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

undef $/;
my(@files, $fn, $prefix);
while (@ARGV) {
  $_ = shift @ARGV;
  if (/^-f$/ || /^--files$/) {
    die "not enough arguments" if !@ARGV;
    push @files, read_files_from(shift @ARGV);
  } elsif (/^--files=(.*)$/) {
    push @files, read_files_from($1);
  } elsif (/^-p$/ || /^--prefix$/) {
    die "not enough arguments" if !@ARGV;
    $prefix = shift @ARGV;
  } elsif (/^--prefix=(.*)$/) {
    $prefix = $1;
  } elsif (/^-./) {
    die "unknown option `$_'\n";
  } elsif (/^-$/) {
    push @files, "-";
  } else {
    push @files, glob($_);
  }
}
push @files, "-" if !@files;

foreach $fn (@files) {
  process_file($fn);
}

umask(022);
open(OUT, ">&STDOUT");
print OUT "# Click class name\tC++ class name\theader file\tprocessing code\trequirements\tprovisions\n";
foreach $id (sort { $class_name[$a] cmp $class_name[$b] } 0..$#class_name) {
  my($n) = $class_name[$id];
  $n = '""' if !$n;
  
  my($f) = $class_file[$id];
  $f =~ s/^$prefix\/*//;

  my($p) = $processing[$id];
  $p = parents_processing($class) if !$p;
  $p = '?' if !$p;

  my($req) = $requirements[$id];
  my($prov) = $provisions[$id];
  
  print OUT $n, "\t", $cxx_name[$id], "\t", $f, "\t", $p,
  "\t\"", $req, "\"\t\"", $prov, "\"\n";
}
close OUT;

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
my(%class_file, %class_name, %processing);
%processing =
    ('Element' => 'a/a', 'UnlimitedElement' => 'a/a', 'TimedElement' => 'a/a');

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
    $class_file{$cxx_class} = $filename;
    if (/\A\s*\w*\s*:\s*([\w\s,]+)/) {
      my $p = $1;
      $p =~ s/\bpublic\b//g;
      $parents{$cxx_class} = [ split(/[\s,]+/, $p) ];
    }
    if (/class_name.*return\s*\"([^\"]+)\"/) {
      $class_name{$cxx_class} = $1;
    }
    if (/processing.*return\s*(.*?);/) {
      my $p = $1;
      $p = $processing_constants{$p} if exists($processing_constants{$p});
      $p =~ tr/\"\s//d;
      $p =~ s{\A([^/]+)\Z}{$1/$1};
      $processing{$cxx_class} = $p;
    }
  }
}

sub parents_processing ($) {
  my($class) = @_;
  my($p) = $processing{$class};
  if (!defined($p)) {
    my($parent);
    foreach $parent (@{$parents{$class}}) {
      if ($parent ne '') {
	my($new_p) = parents_processing($parent);
	$p = $new_p if defined $new_p;
      }
    }
    $processing{$class} = $p;
  }
  $p;
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
    print STDERR "2\n";
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
print OUT "# Click class name\tC++ class name\theader file\tprocessing code\n";
foreach $class (sort keys %class_name) {
  my($f) = $class_file{$class};
  $f =~ s/^$prefix\/*//;

  my($p) = $processing{$class};
  $p = parents_processing($class) if !defined($p);
  $p = '?' if !$p;
  
  print OUT $class_name{$class}, "\t", $class, "\t", $f, "\t", $p, "\n";
}
close OUT;

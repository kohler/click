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
    if (/class_name.*return\s*\"([^\"]+)\"/) {
      $class_name{$cxx_class} = $1;
      $cxx_class = $1;
    }
    if (/default_processing.*return\s+(\w*)/) {
      $processing{$cxx_class} = $1;
    }
  }
}

# main program: parse options
sub read_files_from ($) {
  my($fn) = @_;
  if (open(IN, ($fn eq '-' ? "<&STDIN" : $fn))) {
    my($t) = <IN>;
    close IN;
    map { glob($_) } split(/\s+/, $t);
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
print OUT "# Click class name\tC++ class name\theader file\n";
foreach $class (sort keys %class_name) {
  my($f) = $class_file{$class};
  $f =~ s/^$prefix\/*//;
  print OUT $class_name{$class}, "\t", $class, "\t", $f, "\n";
}
close OUT;

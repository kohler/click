#!/usr/local/bin/perl

# change a class name to a C++ name

sub class_to_xx ($) {
  my($x) = $_[0];
  $x =~ s/@/-a/g;
  $x =~ s/\//-s/g;
  $x =~ s/_/-u/g;
  $x =~ tr/-/_/;
  $x;
}

# mangle a C++ symbol
# can only mangle GNU C++-style symbols 

%mangle_map = ( 'int' => 'i', 'long' => 'l',
		'short' => 's', 'char' => 'c',
		'void' => 'v',
		'unsigned int' => 'Ui', 'unsigned long' => 'Ul',
		'unsigned short' => 'Us', 'unsigned char' => 'Uc',
		'signed int' => 'i', 'signed long' => 'l',
		'signed short' => 's', 'signed char' => 'Sc' );

sub mangle ($$@) {
  my($func, @args) = @_;
  my($i, $t, $any);
  $func =~ s/^(.*)::(.*)$/$2 . "__" . length($1) . $1/e;
  $t = $func;
  foreach $i (@args) {
    next if $i eq 'void';
    $i =~ s/\s+/ /g;
    $i =~ s/^\s+//;
    $i =~ s/\s+$//;

    $any = 1;
    while ($any) {
      $any = 0;
      $t .= 'P', $any = 1 if ($i =~ s/ ?\*$//);
      $t .= 'C', $any = 1 if ($i =~ s/ const$//);
    }
    if ($i =~ s/^const //) {
      $t .= 'C';
    }

    if ($mangle_map{$i}) {
      $t .= $mangle_map{$i};
    } else {
      $t .= length($i) . $i;
    }
  }
  $t;
}

# main program: parse options
undef $/;
my(@inputs, @outputs, $new_class, $old_class, $new_class_xx, $old_class_xx,
   $devirtualize);
$devirtualize = 1;
while (@ARGV) {
  $_ = shift @ARGV;
  if (/^-c$/ || /^--class$/) {
    die "not enough arguments" if !@ARGV;
    $old_class = shift @ARGV;
  } elsif (/^--class=(.*)$/) {
    $old_class = $1;
  } elsif (/^-n$/ || /^--new-class$/) {
    die "not enough arguments" if !@ARGV;
    $new_class = shift @ARGV;
  } elsif (/^--new-class=(.*)$/) {
    $new_class = $1;
  } elsif (/^-i(\d+)$/ || /^--input(\d+)$/) {
    die "not enough arguments" if !@ARGV;
    $inputs[$1] = shift @ARGV;
  } elsif (/^--input(\d+)=(.*)$/) {
    $inputs[$1] = $2;
  } elsif (/^-o(\d+)$/ || /^--output(\d+)$/) {
    die "not enough arguments" if !@ARGV;
    $outputs[$1] = shift @ARGV;
  } elsif (/^--output(\d+)=(.*)$/) {
    $outputs[$1] = $2;
  } elsif (/^-./) {
    die "unknown option `$_'\n";
  } else {
    push @files, glob($_);
  }
}
push @files, "-" if !@files;

my($ninputs, $noutputs) = ($#inputs + 1, $#outputs + 1);

# find input and output class names
$new_class_xx = class_to_xx($new_class);

# canonicalize inputs and outputs; print declarations
for ($i = 0; $i < $ninputs; $i++) {
  $inputs[$i] .= "::pull" if $inputs[$i] && $inputs[$i] !~ /::/;
  if ($inputs[$i] =~ /::pull$/) {
    if ($devirtualize) {
      $inputs[$i] = mangle($inputs[$i], "int");
      print "extern \"C\" Packet *", $inputs[$i], "(Element *, int);\n";
    } else {
      $inputs[$i] = undef;
    }
  }
}
for ($i = 0; $i < $noutputs; $i++) {
  $outputs[$i] .= "::push" if $outputs[$i] && $outputs[$i] !~ /::/;
  if ($outputs[$i] =~ /::push$/) {
    if ($devirtualize) {
      $outputs[$i] = mangle($outputs[$i], "int", "Packet *");
      print "extern \"C\" void ", $outputs[$i], "(Element *, int, Packet *);\n";
    } else {
      $outputs[$i] = undef;
    }
  }
}

# print pull_input function
print "inline Packet *\n${new_class_xx}::pull_input(int port) const\n{\n";
for ($i = 0; $i < $ninputs; $i++) {
  print "  if (port == $i)";
  if (!$inputs[$i]) {
    print "\n    return input($i).pull();\n";
  } elsif ($inputs[$i] =~ /^(.*)::(.*)$/) {
    print " {\n    $1 *e = ($1 *)(input($i).element());\n";
    print "    return e->$2($i);\n  }\n";
  } else {
    print "\n    return ", $inputs[$i], "(input($i).element(), $i);\n";
  }
}
print "  return 0;\n}\n";

# print push_output function
print "inline void\n${new_class_xx}::push_output(int port, Packet *p) const\n{\n";
for ($i = 0; $i < $noutputs; $i++) {
  print "  if (port == $i)";
  if (!$outputs[$i]) {
    print " {\n    output($i).push(p);\n    return;\n  }\n";
  } elsif ($outputs[$i] =~ /^(.*)::(.*)$/) {
    print " {\n    $1 *e = ($1 *)(output($i).element());\n";
    print "    e->$2($i, p);\n    return;\n  }\n";
  } else {
    print " {\n    ", $outputs[$i], "(output($i).element(), $i, p);\n";
    print "    return;\n  }\n";
  }
}
print "  p->kill();\n}\n";

# print element definition
print <<"EOD;";
class $new_class_xx : public $old_class_xx {
 public:
  $new_class_xx() { }
  $new_class_xx *clone() const { return new $new_class_xx; }
  const char *class_name() const { return \"$new_class\"; }
  bool is_a(const char *) const;
};
EOD;

print <<"EOD;";
bool
${new_class_xx}::is_a(const char *n) const
{
  if (strcmp(n, "$new_class") == 0)
    return true;
  else
    return ${old_class_xx}::is_a(n);
}
EOD;

#!/usr/local/bin/perl

# change a class name to a C++ name

sub class_to_cxx ($) {
  my($x) = $_[0];
  if ($element_cxx{$x}) {
    $element_cxx{$x};
  } else {
    $x =~ s/_/_u/g;
    $x =~ s/@/_a/g;
    $x =~ s/\//_s/g;
    $x;
  }
}

sub load_elementmaps ($$) {
  my($path, $ext) = @_;
  my(@path) = split(/:/, $path);
  my($comp);
  local($/) = "\n";
  foreach $comp (@path) {
    $comp = "$comp/$ext" if !-r "$comp/elementmap";
    if (-r "$comp/elementmap") {
      open(IN, "$comp/elementmap") || next;
      while (<IN>) {
	next if /^\s*#/;
	if (/^(\S+)\s*(\S+)\s*(\S+)/) {
	  $element_cxx{$1} = $2;
	  $element_file{$1} = $3;
	}
      }
      close IN;
    }
  }
}

sub open_on_path (*$$) {
  my($handle, $filename, $path) = @_;
  my(@path) = split(/:/, $path);
  my($comp);
  foreach $comp (@path) {
    if (-r "$comp/$filename") {
      open $handle, "$comp/$filename";
      return 1;
    }
  }
  $! = "`$filename' not on path `$path'";
  undef;
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

sub parse_port ($$$) {
  my($x, $io, $num) = @_;
  my($p, $m);
  
  if ($x =~ /^(.*)\.(\d+)$/) {
    ($x, $p) = ($1, $2);
  } else {
    $p = "$io($num).port()";
  }

  if ($x =~ /^(.*)::(.*)$/) {
    ($x, $m) = ($1, $2);
  } else {
    $m = ($io eq 'input' ? 'pull' : 'push');
  }

  eval("(\$${io}_class[$num], \$${io}_port[$num], \$${io}_method[$num])
		= (\"$x\", \"$p\", \"$m\");");
}

undef $/;
my(@input_class, @input_port, @input_method,
   @output_class, @output_port, @output_method,
   $new_class, $old_class, $new_class_cxx, $old_class_cxx,
   $elementmap_path, $elementmap_path_ext,
   $devirtualize);
$devirtualize = 1;
$elementmap_path = $ENV{'ELEMENTMAPPATH'};
if (!$elementmap_path) {
  $elementmap_path = $ENV{'CLICKPATH'};
  $elementmap_path_ext = 'share';
}

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
    parse_port(shift @ARGV, 'input', $1);
  } elsif (/^--input(\d+)=(.*)$/) {
    parse_port($2, 'input', $1);
  } elsif (/^-o(\d+)$/ || /^--output(\d+)$/) {
    die "not enough arguments" if !@ARGV;
    parse_port(shift @ARGV, 'output', $1);
  } elsif (/^--output(\d+)=(.*)$/) {
    parse_port($2, 'output', $1);
  } elsif (/^-./) {
    die "unknown option `$_'\n";
  } else {
    push @files, glob($_);
  }
}
push @files, "-" if !@files;

my($ninputs, $noutputs) = ($#input_class + 1, $#output_class + 1);

load_elementmaps($elementmap_path, $elementmap_path_ext);

# find input and output class names
$old_class_cxx = class_to_cxx($old_class);
$new_class_cxx = class_to_cxx($new_class);

# find old class definition
die "no old class name given (use -c CLASSNAME)\n" if !$old_class;
die "no old class source file found\n" if !$element_file{$old_class};
open_on_path IN, $element_file{$old_class}, "../.." ||
    die "can't open old class header file: $!\n";
$old_header = <IN>;
close IN;
my($f) = $element_file{$old_class};
$f =~ s/\.hh$/.cc/;
open_on_path IN, $f, "../.." ||
    die "can't open old class source file: $!\n";
$old_source = <IN>;
close IN;

print STDERR $old_source;


# canonicalize inputs and outputs; print declarations
my(@input_func, @output_func);

for ($i = 0; $i < $ninputs; $i++) {
  $input_func[$i] = undef;
  if ($input_method[$i] eq 'pull' && $devirtualize) {
    $input_func[$i] = mangle("$input_class[$i]::$input_method[$i]", "int");
    print "extern \"C\" Packet *", $input_func[$i], "(Element *, int);\n";
  }
}

for ($i = 0; $i < $noutputs; $i++) {
  $output_func[$i] = undef;
  if ($output_method[$i] eq 'push' && $devirtualize) {
    $output_func[$i] = mangle("$output_class[$i]::$output_method[$i]", "int", "Packet *");
    print "extern \"C\" void ", $output_func[$i], "(Element *, int, Packet *);\n";
  }
}

# print pull_input function
print "inline Packet *\n${new_class_cxx}::pull_input(int port) const\n{\n";
for ($i = 0; $i < $ninputs; $i++) {
  print "  if (port == $i)";
  if ($input_func[$i]) {
    print "\n    return ", $input_func[$i], "(input($i).element(), $input_port[$i]);\n";
  } elsif ($input_class[$i]) {
    print " {\n    $input_class[$i] *e = ($input_class[$i] *)(input($i).element());\n";
    print "    return e->$input_method[$i]($input_port[$i]);\n  }\n";
  } else {
    print "\n    return input($i).pull();\n";
  }
}
print "  return 0;\n}\n";

# print push_output function
print "inline void\n${new_class_cxx}::push_output(int port, Packet *p) const\n{\n";
for ($i = 0; $i < $noutputs; $i++) {
  print "  if (port == $i)";
  if ($output_func[$i]) {
    print " {\n    $output_func[$i](output($i).element(), $output_port[$i], p);\n    return;\n  }\n";
  } elsif ($output_class[$i]) {
    print " {\n    $output_class[$i] *e = ($output_class[$i] *)(output($i).element());\n";
    print "    e->$output_method[$i]($output_port[$i], p);\n    return;\n  }\n";
  } else {
    print " {\n    output($i).push(p);\n    return;\n  }\n";
  }
}
print "  p->kill();\n}\n";

# print element definition
print <<"EOD;";
class $new_class_cxx : public $old_class_cxx {
 public:
  $new_class_cxx() { }
  $new_class_cxx *clone() const { return new $new_class_cxx; }
  const char *class_name() const { return \"$new_class\"; }
  bool is_a(const char *) const;
  void push_output(int, Packet *) const;
  Packet *pull_input(int) const;
};
EOD;

print <<"EOD;";
bool
${new_class_cxx}::is_a(const char *n) const
{
  if (strcmp(n, "$new_class") == 0)
    return true;
  else
    return ${old_class_cxx}::is_a(n);
}
EOD;

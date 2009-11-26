#! /usr/bin/perl -w
use bytes;
undef $/;

my($outputroot, $verbose);
while (@ARGV) {
    if ($ARGV[0] eq "-o" && @ARGV > 1) {
	$outputroot = $ARGV[1];
	shift;
	shift;
    } elsif ($ARGV[0] =~ /^-o(.*)$/) {
	$outputroot = $1;
	shift;
    } elsif ($ARGV[0] eq "-V" || $ARGV[0] eq "--verbose") {
	$verbose = 1;
	shift;
    } else {
	last;
    }
}
if (!$outputroot || !@ARGV) {
    print STDERR "Usage: fixincludes.pl -o OUTPUTDIR CFLAGS\n";
    exit 1;
}

# create superdirectories
@outputroot = split(m{/}, $outputroot);
for (my $i = 0; $i < @outputroot; ++$i) {
    $dir = join("/", @outputroot[0..$i]);
    next if $dir eq "" || -d $dir;
    mkdir($dir) || die "fixincludes.pl: mkdir $dir: $!";
}

sub sprotect ($) {
    my($t) = $_[0];
    $t =~ tr/\000-\177/\200-\377/;
    $t;
}

sub sunprotect ($) {
    my($t) = $_[0];
    $t =~ tr/\200-\377/\000-\177/;
    $t;
}

sub expand_initializer ($$) {
    my($var, $v) = @_;
    my(@prefix, $r, $slot);
    push @prefix, $var;
    $r = "";
    $v =~ tr/\n/ /;
    while (1) {
	$v =~ s/\A\s+//;
	if ($v =~ /\A\}(.*)\z/s) {
	    die "(1)" if @prefix == 1;
	    pop @prefix;
	    $v = $1;
	} elsif ($v =~ /\A,(.*)\z/s) {
	    $v = $1;
	} elsif ($v =~ /\A\s*(\w+)\s*:\s*(.*)\z/s) {
	    ($slot, $v) = ($1, $2);
	    if ($v =~ /\A\{(.*)\z/s) {
		push @prefix, $slot;
		$v = $1;
	    } elsif ($v =~ /\A(.*?)([,\{\}].*)\z/s) {
		$r .= join(".", @prefix, $slot) . " = $1;\n";
		$v = $2;
	    } else {
		die "$v (2)";
	    }
	} elsif ($v eq "") {
	    last;
	} else {
	    die "$v [$r] (3)";
	}
    }
    die "(4)" if @prefix != 1;
    return $r;
}

sub one_includeroot ($$) {
    my($includeroot, $outputroot) = @_;
    my(@dirs, $d, $dd, $f);

    @dirs = ("");

    while (@dirs) {
	$ddx = shift @dirs;
	$dd = ($ddx eq "" ? "" : "/$ddx");
	$ddy = ($ddx eq "" ? "" : "$ddx/");

	opendir(D, "$includeroot$dd") || die "fixincludes.pl: $includeroot$dd: $!";
	-d "$outputroot$dd" || mkdir("$outputroot$dd") || die "fixincludes.pl: mkdir $outputroot$dd: $!";
	while (($d = readdir(D))) {
	    next if $d =~ /^\./;
	    $f = "$includeroot$dd/$d";
	    if (-d $f) {
		push @dirs, "$ddy$d";
		next;
	    } elsif (!-f $f) {
		next;
	    }
	    print STDERR "$ddy$d\n" if $verbose;

	    # Now we definitely have a file.
	    open(F, $f) || die "fixincludes.pl: $f: $!";
	    $_ = <F>;
	    close(F);

	    # Fix include files for C++ compatibility.

	    # First obscure comments and things that would confuse parsing.

	    # DO NOT do preprocessor directives; we need to fix their
	    # definitions
	    # s{(?<=\n)(#.*?[^\\])(?=\n)}{sprotect($1)}gse;
	    # C comments (assume no string includes '/*')
	    s{(/\*.*?\*/)}{sprotect($1)}gse;
	    # C++ comments (assume no string includes '//')
	    s{(//.*?)}{sprotect($1)}ge;
	    # strings
	    s{("")}{sprotect($1)}ge;
	    s{"(.*?[^\\])"}{"\"" . sprotect($1) . "\""}sge;
	    # characters
	    s{'(.*?[^\\])'}{"\'" . sprotect($1) . "\'"}sge;

	    # Now fix problems.

	    # empty structures
	    s{(\bstruct[\s\w\200-\377]+\{)([\s\200-\377]+\})}{$1 . "\n  int __padding_convince_cplusplus_zero_size__[0];\n" . $2}sge;

	    # colon-colon
	    s{:(?=:)}{: }g;

	    # "extern asmlinkage"
	    s{\bextern\s+asmlinkage\b}{asmlinkage}g;
	    s{\bextern\s+void\s+asmlinkage\b}{asmlinkage void}g;

	    # advanced C initializers
	    s{([\{,]\s*)\.\s*([a-zA-Z_]\w*)\s*=}{$1$2: }g;
	    s{([\{,]\s*\\\n\s*)\.\s*([a-zA-Z_]\w*)\s*=}{$1$2: }g;

	    # ktime initializers
	    s{(return|=)\s*\((\w+)\)\s*(\{[^\{\}]*\})}{$1 \(\{ $2 __magic_$2__ = $3; __magic_$2__; \}\)}g;
	    s{(return|=)\s*\((\w+)\)\s*(\{[^\{\}]*\{[^\{\}]*\}[^\{\}]*\})}{$1 \(\{ $2 __magic_$2__ = $3; __magic_$2__; \}\)}g;
	    s{\(struct\s+(\w+)\)\s*(__.*\(.*?\));}{\(\{ struct $1 __magic_$1__ = $2; __magic_$1__; \}\);}g;

	    # "new"
	    s{\bnew\b}{new_value}g;

	    # casts
	    s{((?:const )?(?:volatile )?)type \*(\w+) = (\w+)\s*;}{$1type \*$2 = ($1type \*) $3;}g;
	    s{__xchg_u32\((\w+)\s*,}{__xchg_u32((volatile int *) $1,}g;
	    s{__xchg_u64\((\w+)\s*,}{__xchg_u64((volatile __u64 *) $1,}g;
	    s{void\s+([\w\s]+)\s*;}{void *$1;}g;

	    # stuff for particular files (what a shame)
	    if ($d eq "page-flags.h") {
		s{(#define PAGE_FLAGS_H)}{$1\n#undef private};
		s{(#endif.*[\s\n]*)\z}{#define private linux_private\n$1};
	    }
	    if ($d eq "timer.h") {
		s{enum hrtimer_restart}{int};
	    }
	    if ($d eq "route.h") {
		s{\b(\w+)\s*=\s*\{(\s*\w+:.*)\}\s*;}{"$1;\n" . expand_initializer($1, $2)}sge;
	    }
	    if ($d eq "types.h") {
		s{(typedef.*bool\s*;)}{#ifndef __cplusplus\n$1\n#endif};
	    }

	    # unquote.
	    $_ = sunprotect($_);

	    # perhaps nothing has changed; avoid changing the timestamp
	    if (-f "$outputroot$dd/$d") {
		open(F, "$outputroot$dd/$d");
		$old = <F>;
		close(F);
		$old =~ s/\A.*\n.*\n//;
		next if $old eq $_;
	    }

	    # Write the fixed file.
	    open(F, ">$outputroot$dd/$d") || die "fixincludes.pl: $outputroot$dd/$d: $!";
	    print F "/* created by click/linuxmodule/fixincludes.pl on " . localtime() . " */\n/* from $f */\n", $_;
	    close(F);
	}
    }
}

my(@new_argv, %done, $dir, $numdirs);
$numdirs = 0;
foreach my $i (@ARGV) {
    if ($i =~ /^-I(.*)/) {
	if (!-d $1) {
	    # do not change argument
	    push @new_argv, $i;
	} elsif (!$done{$1}) {
	    $dir = "$outputroot/include$numdirs";
	    -d $dir || mkdir $dir || die "fixincludes.pl: mkdir $dir: $!";
	    $done{$1} = $dir;
	    ++$numdirs;
	    one_includeroot($1, $dir);
	    push @new_argv, "-I" . $dir;
	} else {
	    push @new_argv, "-I" . $done{$1};
	}
    } else {
	push @new_argv, $i;
    }
}
print join(" ", @new_argv), "\n";

my(@sed, $k, $v);
while (($k, $v) = each %done) {
    push @sed, "-e s,$k,$v,";
}
@sed = sort { length($a) <=> length($b) } @sed;
print join(" ", @sed), "\n";

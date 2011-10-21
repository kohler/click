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

sub expand_initializer ($$$) {
    my($var, $v, $f) = @_;
    my(@prefix, $r, $slot, $originalv);
    push @prefix, $var;
    $r = "";
    $v =~ tr/\n/ /;
    $originalv = $v;
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
	    } elsif ($v =~ /\A([^;\{\}]*?)\s*\z/s) {
		$r .= join(".", @prefix, $slot) . " = $1;\n";
		$v = "";
	    } else {
		die "$f: $slot : $v @ $originalv (2)";
	    }
	} elsif ($v eq "") {
	    last;
	} else {
	    die "$f: $v [$r] (3)";
	}
    }
    die "$f: (4)" if @prefix != 1;
    return $r;
}

sub expand_array_initializer ($$$) {
    my($var, $size, $val) = @_;
    my(@sizes) = (4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048);
    my($text) = "";
    foreach my $s (@sizes) {
	$text .= ($s == $sizes[0] ? "#if" : "#elif") . " $size == $s\n"
	    . "#define $var {{" . join(",", ($val) x $s) . "}}\n";
    }
    return $text . "#else\n#error \"fixincludes.pl needs updating\"\n#endif\n";
}

my($click_cxx_protect) = "#if defined(__cplusplus) && !CLICK_CXX_PROTECTED\n# error \"missing #include <click/cxxprotect.h>\"\n#endif\n";

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

	opendir(OD, "$outputroot$dd");
	my(%previousfiles);
	foreach $d (readdir(OD)) {
	    $previousfiles{$d} = 1 if $d !~ /^\./;
	}
	closedir(OD);

	while (($d = readdir(D))) {
	    next if $d =~ /^\./;
	    delete $previousfiles{$d};
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

	    # The ".smp_locks" ASM alternatives section interacts badly
	    # with C++ style weak linkage.  The .smp_locks section refers
	    # to the globally-linked versions of templated inline functions
	    # like Vector<String>::~Vector().  The weakly-linked sections
	    # are dropped, and then the linker dies, because .smp_locks
	    # references them.  Too bad, but the simplest fix is to get rid
	    # of the alternatives.  Since this is in strings, must
	    # implement before string obfuscation
	    s{\.section\s+\.smp_locks.*?\.previous(?:\\n)?}{}sg;

	    # Obscure comments and things that would confuse parsing.

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

	    # "extern asmlinkage" and other declaration weirdness
	    s{\bextern\s+asmlinkage\b}{asmlinkage}g;
	    s{\bextern\s+void\s+asmlinkage\b}{asmlinkage void}g;
	    s&^notrace\s*((?:static)?\s*inline[^;]*)\{&$1 notrace;\n$1\{&mg;
	    s&^((?:static)?\s*inline\s*)notrace\s*([^;]*)\{&$1 $2 notrace;\n$1 $2\{&mg;

	    # advanced C initializers
	    s{([\{,]\s*)\.\s*([a-zA-Z_]\w*)\s*=}{$1$2: }g;
	    s{([\{,]\s*\\\n\s*)\.\s*([a-zA-Z_]\w*)\s*=}{$1$2: }g;

	    # "new" and other keywords
	    s{\bnew\b}{new_value}g;
	    s{\band\b}{and_value}g;
	    s{\bswap\b}{linux_swap}g;
	    # including "P[new]" in inline assembly string (look for
	    # protected version)
	    1 while (s{(asm.*\333)\356\345\367\335}{$1\356\345\367\337\366\341\354\365\345\335}g);

	    # "sizeof" isn't nice to the preprocessor
	    s{sizeof(?:\s+(?:unsigned\s+)?long|\s*\(\s*(?:unsigned\s+)?long\s*\))}{(BITS_PER_LONG/8 /*=BITS_PER_BYTE*/)}g;

	    # casts
	    s{((?:const )?(?:volatile )?)type \*(\w+) = (\w+)\s*;}{$1type \*$2 = ($1type \*) $3;}g;
	    s{__xchg_u32\((\w+)\s*,}{__xchg_u32((volatile int *) $1,}g;
	    s{__xchg_u64\((\w+)\s*,}{__xchg_u64((volatile __u64 *) $1,}g;
	    s{void\s+([\w\s]+)\s*;}{void *$1;}g;

	    # constant expressions
	    s{__cpu_to_be32 *\( *([0-9][0-9a-fxA-FX]*) *\)}{__constant_htonl($1)}g;

	    # stuff for particular files (what a shame)
	    if ($d eq "page-flags.h") {
		s{(#define PAGE_FLAGS_H)}{$1\n#undef private};
		s{(#endif.*[\s\n]*)\z}{#define private linux_private\n$1};
	    }
	    if ($d eq "timer.h") {
		s{enum hrtimer_restart}{int};
	    }
	    if ($d eq "route.h") {
		s{\b(\w+)\s*=\s*\{(\s*\w+:.*)\}\s*;}{"$1;\n" . expand_initializer($1, $2, $f)}sge;
	    }
	    if ($d eq "types.h") {
		s{(typedef.*bool\s*;)}{#ifndef __cplusplus\n$1\n#endif};
	    }
	    if ($d eq "mpspec.h") {
		s<^\#define\s+(\w+)\s+\{\s*\{\s*\[\s*0\s*\.\.\.\s*(\w+)\s*-\s*1\s*\]\s*=\s*(.*?)\s*\}\s*\}><expand_array_initializer($1, $2, $3)>emg;
	    }
	    if ($d eq "kernel.h") {
		# BUILD_BUG_ON_* cannot define a struct inside sizeof().
		# Rather than negative-bitfield size, produce a negative
		# array dimension.
		s&sizeof\s*\(\s*struct\s*\{\s*\w+\s*:\s*-\s*\!\s*\!\s*\(e\)\s*;\s*\}\s*\)&(sizeof(int[-!!(e)])*(size_t)0)&g;
	    }
	    if ($d eq "sched.h") {
		s<^(extern char ___assert_task_state)((?:.*?\n)*?.*?\;.*)$><\#ifndef __cplusplus\n$1$2\n\#endif>mg;
	    }
	    if ($d eq "kobject.h") {
		s<(^#include[\000-\377]*)(^enum kobj_ns_type\s+\{[\000-\377]*?\}.*\n)><$2$1>mg;
	    }
	    if ($d eq "netdevice.h") {
		1 while (s<(^struct net_device \{[\000-\377]*)^\tenum( \{[^}]*\}) (\w+)><enum net_device_$3$2;\n$1\tenum net_device_$3 $3>mg);
	    }

	    # ktime initializers
	    if ($d eq "ktime.h") {
		s{(return|=)\s*\((\w+)\)\s*(\{[^\{\}]*\})}{$1 \(\{ $2 __magic_$2__ = $3; __magic_$2__; \}\)}g;
		s{(return|=)\s*\((\w+)\)\s*(\{[^\{\}]*\{[^\{\}]*\}[^\{\}]*\})}{$1 \(\{ $2 __magic_$2__ = $3; __magic_$2__; \}\)}g;
		s{\(struct\s+(\w+)\)\s*(__.*\(.*?\));}{\(\{ struct $1 __magic_$1__ = $2; __magic_$1__; \}\);}g;
	    }

	    if ($d eq "semaphore.h") {
		s{(static inline void sema_init)}{#ifndef __cplusplus\n$1};
	        s/(lockdep.*})/$1\n#endif\n/s;
	    }
	    if ($d eq "radix-tree.h") {
		# errors in the RCU macros with rcu_dereference(*pslot)
		1 while s<void \*\*pslot([^\}]*?)\{><void **____pslot$1\{char **pslot = (char **) ____pslot;>;
		1 while s<void \*item([^\}]*?)\{><void *____item$1\{char *item = (char *) ____item;>;
	    }

	    # CLICK_CXX_PROTECTED check
	    if (m<\A[\s\200-\377]*\z>) {
		# empty file, do nothing
	    } elsif (m<(\A[\s\200-\377]*^\#ifndef.*\n)>m
		     || m<(\A[\s\200-\377]*^\#if\s+!\s*defined.*\n)>m) {
		$_ = $1 . $click_cxx_protect . substr($_, length($1));
	    } elsif ($d ne "version.h" && $d ne "autoconf.h") {
		$_ = $click_cxx_protect . $_;
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

	# delete unused files
	foreach $d (keys(%previousfiles)) {
	    if (-d "$outputroot$dd/$d") {
		system("rm -rf \"$outputroot$dd/$d\"");
	    } else {
		unlink("$outputroot$dd/$d");
	    }
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
push @sed, "-e s,\\ -I,\\ -isystem\\ ,g";
print join(" ", @sed), "\n";

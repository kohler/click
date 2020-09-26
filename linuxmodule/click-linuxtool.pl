#! /usr/bin/perl -w
use bytes;
use MIME::Base64;

sub usage () {
    print STDERR "Usage: click-linuxtool.pl [-V] -o OUTPUTDIR CFLAGS
   or: click-linuxtool.pl installtree [-m MODE] SRC DST\n";
    exit 1;
}

sub mkpath (\%$) {
    my($dirs, $dir) = @_;
    my($superdir) = $dir;
    $superdir =~ s{/+[^/]+/*$}{};
    if (!exists($dirs->{$superdir}) && $superdir ne "") {
        &mkpath($dirs, $superdir);
    }
    if (!-d $dir && !mkdir($dir)) {
        print STDERR "$dir: $!\n";
        exit 1;
    }
    $dirs->{$dir} = 1;
}

if (@ARGV > 0 && $ARGV[0] eq "installtree") {
    shift;
    my($mode) = 755;
    my($src, $dst);
    while (@ARGV) {
        if ($ARGV[0] =~ /^(?:-m|--m=|--mo=|--mod=|--mode=)(\d+)$/s) {
            $mode = $1;
            shift;
        } elsif ($ARGV[0] =~ /^(?:-m|--m|--mo|--mod|--mode)$/s) {
            usage if @ARGV < 2 || $ARGV[1] !~ /^\d+$/s;
            $mode = $ARGV[1];
            shift;
            shift;
        } elsif ($ARGV[0] =~ /^-/) {
            usage;
        } elsif (!defined($src)) {
            $src = $ARGV[0];
            shift;
        } elsif (!defined($dst)) {
            $dst = $ARGV[0];
            shift;
        } else {
            usage;
        }
    }
    usage if !defined($src) || !defined($dst) || "$src$dst" =~ /[\\"<>]/;
    $mode = oct("0$mode");

    $MD5SUM = $ENV{"MD5SUM"};
    $null_md5sum = "d41d8cd98f00b204e9800998ecf8427e";
    if (!defined($MD5SUM)) {
        if (`sh -c "md5sum < /dev/null" 2>/dev/null | awk '{print \$1}'` =~ /^$null_md5sum/) {
            $MD5SUM = "md5sum";
        } elsif (`sh -c "md5 < /dev/null" 2>/dev/null | awk '{print \$1}'` =~ /^$null_md5sum/) {
            $MD5SUM = "md5";
        } elsif (`sh -c "sum < /dev/null" 2>/dev/null | awk '{print \$1}'` =~ /^$null_md5sum/) {
            $MD5SUM = "sum";
        } else {
            print STDERR "Sorry, 'click-linuxtool.pl installtree' requires a working 'md5sum' program.\n";
            exit 1;
        }
    }

    open(SRC, "cd \"$src\"; find . -type f -print | xargs -L 100 $MD5SUM |") || die;
    open(DST, "cd \"$dst\"; find . -type f -print | xargs -L 100 $MD5SUM |") || die;
    my(%ch, %dst, $k, $v);
    while (<DST>) {
        $dst{$2} = $1 if /^([0-9a-f]+)\s*(.*)$/i && $1 ne $null_md5sum;
    }
    close DST;
    while (<SRC>) {
        if (/^([0-9a-f]+)\s*(.*)$/i && $1 ne $null_md5sum) {
            $ch{$2} = 1 if !exists($dst{$2}) || $dst{$2} ne $1;
            delete $dst{$2};
        }
    }
    close SRC;
    foreach $k (keys %dst) {
        $ch{$k} = 0;
    }

    # compare and remove
    my($nchanges, $ndone, $ttyout, $lastpercent, $percent);
    $nchanges = scalar(keys(%ch));
    $ttyout = -t STDOUT;
    $lastpercent = "";
    $ndone = 0;
    my(%dirs);
    undef $/;
    while (($k, $v) = each %ch) {
        my($file) = "$dst/$k";
        if ($v) {
            my($dir) = $file;
            $dir =~ s{/+[^/]*$}{};
            mkpath(%dirs, $dir) if !exists($dirs{$dir});
            if (!open(F, "$src/$k")) {
                print STDERR "$src/$k: $!\n";
                exit 1;
            }
            if (!open(G, ">$file")) {
                print STDERR "$file: $!\n";
                exit 1;
            }
            while (<F>) {
                print G $_;
            }
            close F;
            close G;
            if (!chmod $mode, $file) {
                print STDERR "$file: $!\n";
                exit 1;
            }
        } else {
            if (!unlink($file)) {
                print STDERR "$file: $!\n";
                exit 1;
            }
        }
        ++$ndone;
        $percent = int(($ndone * 100) / $nchanges);
        if ($ttyout && $percent ne $lastpercent) {
            print "\r                                                                           \r  ... $percent% done";
        }
        $lastpercent = $percent;
    }

    if ($ttyout && $ndone) {
        print "\r                                                                           \r  ... done, $ndone ", ($ndone == 1 ? "file installed\n" : "files installed\n");
    }

    exit 0;
}

undef $/;
my($outputroot, $verbose);
while (@ARGV) {
    if ($ARGV[0] eq "-o" && @ARGV > 1) {
	$outputroot = $ARGV[1];
	shift;
	shift;
    } elsif ($ARGV[0] =~ /^-o(.+)$/s) {
	$outputroot = $1;
	shift;
    } elsif ($ARGV[0] =~ /^(?:-V|--v|--ve|--ver|--verb|--verbo|--verbos|--verbose)$/s) {
	$verbose = 1;
	shift;
    } else {
	last;
    }
}
usage if !$outputroot || !@ARGV;

# create superdirectories
@outputroot = split(m{/}, $outputroot);
for (my $i = 0; $i < @outputroot; ++$i) {
    $dir = join("/", @outputroot[0..$i]);
    next if $dir eq "" || -d $dir;
    mkdir($dir) || die "click-linuxtool.pl: mkdir $dir: $!";
}

sub sprotect ($) {
    my($t) = encode_base64($_[0]);
    $t =~ tr/\000-\177/\200-\377/;
    "\177" . $t;
}

sub sprotect_str ($) {
    my($t) = $_[0];
    $t =~ s{%P\[new\]}{%P[new_value]}g;
    sprotect($t);
}

sub sunprotect1 ($) {
    my($t) = $_[0];
    $t =~ tr/\200-\377/\000-\177/;
    decode_base64($t);
}

sub sunprotect ($) {
    my($t) = $_[0];
    $t =~ s{\177([\200-\377]+)}{sunprotect1($1)}eg;
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
	    die "(1: $originalv)" if @prefix == 1;
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
    return $text . "#else\n#error \"click-linuxtool.pl needs updating\"\n#endif\n";
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

	opendir(D, "$includeroot$dd") || die "click-linuxtool.pl: $includeroot$dd: $!";
	-d "$outputroot$dd" || mkdir("$outputroot$dd") || die "click-linuxtool.pl: mkdir $outputroot$dd: $!";

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
	    open(F, $f) || die "click-linuxtool.pl: $f: $!";
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
	    s{\.pushsection\s+\.smp_locks.*?\.popsection(?:\\n)?}{}sg;

	    # Obscure comments and things that would confuse parsing.
            # Assume no \177 characters.
            die if m/\177/;

	    # DO NOT do preprocessor directives; we need to fix their
	    # definitions
	    # s{(?<=\n)(#.*?[^\\])(?=\n)}{sprotect($1)}gse;
	    # C comments (assume no string includes '/*')
	    s{(/\*.*?\*/)}{sprotect($1)}gse;
	    # C++ comments (assume no string includes '//')
	    s{(//[^\r\n]*)}{sprotect($1)}gse;
            # #error
	    s{^(#\s*error[^\r\n]*)}{sprotect($1)}gme;
	    # strings
	    s{("")}{sprotect($1)}ge;
	    s{"((?:[^\\\"]|\\.)*)"}{"\"" . sprotect_str($1) . "\""}sge;
	    # characters
	    s{'((?:[^\\\']|\\.)*)'}{"\'" . sprotect($1) . "\'"}sge;

	    # Now fix problems.

	    # empty structures
	    s{(\bstruct[\s\w\200-\377]+\{)([\s\200-\377]+\})}{$1 . "\n  int __padding_convince_cplusplus_zero_size__[0];\n" . $2}sge;

	    # colon-colon
	    s{:(?=:)}{: }g;

	    # "extern asmlinkage" and other declaration weirdness
	    s{\bextern\s+asmlinkage\b}{asmlinkage}g;
	    s{\bextern\s+void\s+asmlinkage\b}{asmlinkage void}g;
	    s&^(asmlinkage[^;]*)asmlinkage&$1&mg;
	    s&^notrace\s*((?:static)?\s*inline[^;]*)\{&$1 notrace;\n$1\{&mg;
	    s&^((?:static)?\s*inline\s*)notrace\s*([^;]*)\{&$1 $2 notrace;\n$1 $2\{&mg;

	    # advanced C initializers
	    s{([\{,]\s*)\.\s*([a-zA-Z_]\w*)\s*=}{$1$2: }g;
	    s{([\{,]\s*\\\n\s*)\.\s*([a-zA-Z_]\w*)\s*=}{$1$2: }g;

	    # "new" and other keywords
	    s{\bnew\b}{new_value}g;
	    s{\band\b}{and_value}g;
	    s{\bcompl\b}{compl_value}g;
            s{\bprivate\b}{kprivate}g;
            s{(PAGEFLAG[^)]*)kprivate}{$1private}g;
	    s{\bswap\b}{linux_swap}g;

	    # "sizeof" isn't nice to the preprocessor
	    s{sizeof(?:\s+(?:unsigned\s+)?long|\s*\(\s*(?:unsigned\s+)?long\s*\))}{(BITS_PER_LONG/8 /*=BITS_PER_BYTE*/)}g;

	    # casts
	    s{((?:const )?(?:volatile )?)type \*(\w+) = (\w+)\s*;}{$1type \*$2 = ($1type \*) $3;}g;
	    s{__xchg_u32\((\w+)\s*,}{__xchg_u32((volatile int *) $1,}g;
	    s{__xchg_u64\((\w+)\s*,}{__xchg_u64((volatile __u64 *) $1,}g;
	    s{void\s+([\w\s]+)\s*;}{void *$1;}g;

	    # constant expressions
	    s{__cpu_to_be32 *\( *([0-9][0-9a-fxA-FX]*) *\)}{__constant_htonl($1)}g;

            # user-defined string literals
            s{("[^"\n]*")([_a-zA-Z]+)}{$1 $2}g;

	    # de-const typeof in unions
            if ($d eq "compiler.h") {
		s{(union\s*\{\s*typeof\(x\).*__u);}{$1 = {0};}g;
	    }

	    # BUILD_BUG_ON_* cannot define a struct inside sizeof().
	    # Rather than negative-bitfield size, produce a negative array dimension.
	    if ($d eq "bug.h") {
		s{(#define BUILD_BUG_ON_ZERO\(e\)) \(sizeof\(struct \{ int:-!!\(e\); \}\)\)}{$1 (sizeof(int[-!!(e)]))}g;
		s{(#define BUILD_BUG_ON_NULL\(e\) \(\(void \*\))sizeof\(struct { int:-!!\(e\); }\)\)}{$1(sizeof(int[-!!(e)]))}g;
	    }

	    if ($d eq "cxxprotect.h") {
		s{(#\s+define\s+typename\s+linux_typename)}{# define typename	typename}g;
	    }  

	    if ($d eq "syscall.h") {
		s{(typedef\s+asmlinkage\s+long\s+\(\*sys_call_ptr_t\))}{asmlinkage typedef long (*sys_call_ptr_t)}g;
	    }

	    if ($d eq "ip.h") {
		s{(return\s+min\(READ_ONCE\(dst->dev->mtu\),\s+IP_MAX_MTU\))}{const typeof(dst->dev->mtu) a = READ_ONCE(dst->dev->mtu);
		  return min(a, IP_MAX_MTU)}g;

		s{return\s+min\(READ_ONCE\(skb_dst\(skb\)->dev->mtu\),\s+IP_MAX_MTU\)}{const typeof(skb_dst(skb)->dev->mtu) a = READ_ONCE(skb_dst(skb)->dev->mtu);
		  return min(a, IP_MAX_MTU)}g;
	    }


	    # fix illegal void* arithmetic
	    s{(\w+)\s*-\s*\(\s*void\s*\*\s*\)}{(uintptr_t)$1 - (uintptr_t)}g;

	    # stuff for particular files (what a shame)
	    if ($d eq "timer.h") {
		s{enum hrtimer_restart}{int};
	    }
	    if ($d eq "route.h") {
		s{\b(\w+)\s*=\s*\{(\s*\w+:.*?)\}\s*;}{"$1;\n" . expand_initializer($1, $2, $f)}sge;
	    }
            if ($d =~ /^atomic/) {
                s{^(ATOMIC.*?(?:and|or|compl))_value}{$1}mg;
            }
	    if ($d eq "fs.h") {
		s{\(struct\s+(\w+)\)\s*\{(\s*\w+:.*?)\}}{"({ struct $1 __magic_struct_$1;\n" . expand_initializer("__magic_struct_$1", $2, $f) . "\n__magic_struct_$1; })"}sge;
	    }
	    if ($d eq "types.h") {
		s{(typedef.*bool\s*;)}{#ifndef __cplusplus\n$1\n#endif};
	    }
	    if ($d eq "mpspec.h") {
		s<^\#define\s+(\w+)\s+\{\s*\{\s*\[\s*0\s*\.\.\.\s*(\w+)\s*-\s*1\s*\]\s*=\s*(.*?)\s*\}\s*\}><expand_array_initializer($1, $2, $3)>emg;
	    }
            if ($d =~ m<\Adma->) {
                s<DEFINE_DMA_ATTRS\((\w+)\);><struct dma_attrs $1; init_dma_attrs(&$1);>g;
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
            if ($d eq "projid.h" || $d eq "uidgid.h") {
                s{\bK([A-Z]+)T_INIT\(-1\)}{K$1T_INIT((\L$1\E_t) -1)}g;
            }
	    if ($d eq "kobject.h") {
		s<(^#include[\000-\377]*)(^enum kobj_ns_type\s+\{[\000-\377]*?\}.*\n)><$2$1>mg;
	    }
	    if ($d eq "netdevice.h") {
		1 while (s<(^struct net_device[ \n]*\{[\000-\377]*)^\tenum( \{[^}]*\}) (\w+)><enum net_device_$3$2;\n$1\tenum net_device_$3 $3>mg);
                s{\((\w+\s*\+\s*\w+)\)\s*-\s*\(void\s*\*\)(\w+\s*)->head}{((unsigned char*) $1) - $2->head}g;
            }
            if ($d eq "aio.h") {
                while (1) {
                    my($a) = s<^(\s*)(\S+)\s*=\s*\(struct kiocb\)\s*\{\s*(\w+):\s*(.*?),\s*><$1\($2\).$3 = $4;\n$1$2 = (struct kiocb) {>m;
                    my($b) = s<^(\s*)(\S+)\s*=\s*\(struct kiocb\)\s*\{\s*(\.[\.\w]+)\s*=\s*(.*?),\s*><$1\($2\)$3 = $4;\n$1$2 = (struct kiocb) {>m;
                    s<^(\s*)(\S+)\s*=\s*\(struct kiocb\)\s*\{\s*\}\s*;><>m;
                    last if !$a && !$b;
                }
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
		1 while s<pslot, void \*item([^\}]*?)\{><pslot, void *____item$1\{char *item = (char *) ____item;>;
	    }
	    if ($d eq "spinlock_types.h") {
		s<(typedef\s+struct[^\}]+)(struct\s+__raw_tickets\s+)({[^\}]+})><$2$3;\n$1$2>;
	    }
	    if ($d eq "spinlock.h") {
		s<struct\s+__raw_tickets\s+(\w+)\s*=\s*\{\s*tail:\s*(\S+?)\s*\};><struct __raw_tickets $1 = {}; $1.tail = $2;>;
	    }
	    if ($d eq "compiler.h" || $d eq "linkage.h") {
		# s<^#define ACCESS_ONCE\(x\) \(\*\(volatile typeof\(x\) \*\)\&\(x\)\)><#define ACCESS_ONCE(x) (*(typeof(x) * volatile)&(x))>m;
		s<^(#define\s+notrace\s+__attribute__\(\(no_instrument_function\)\))><// g++ has stricter rules about this attribute. We can't deal.\n#ifdef __cplusplus\n#define notrace\n#else\n$1\n#endif>m;
	    }
	    if ($d eq "sysctl.h") {
		s<^(\s+)(proc_handler \*proc_handler;.*)$><#ifdef __cplusplus\n$1::$2\n#else\n$1$2\n#endif>m;
	    }
            if ($d eq "rbtree_latch.h") {
                s{container_of\(node, (struct latch_tree_node), node\[idx\]\)}{(\{ ($1*) ((char*) node - offsetof($1, node[0]) - sizeof((($1*)0)->node[0]) * idx); \})};
            }

	    if ($d eq "fs.h" || $d eq "netfilter.h") {
		s<enum (migrate_mode|ip_conntrack_info);><enum $1 \{$1_DUMMY\};>;
	    }
            if ($d eq "context_tracking_state.h") {
                s<(struct.*?\{[^}]*)enum\s+(\w+)(\s*[^{]*[{][^}]*[}])(\s*\w+)><enum $2$3;\n$1enum $2$4>;
            }

            if ($d eq "filter.h") {
                s<offsetof\(([^,]*),\s*(\w+)\s*\[\s*([a-zA-Z]\w*)\s*\]\s*\)><(offsetof($1, $2) + sizeof((($1*) 0)-\>$2\[0]) * $3)>;
            }
            if ($d eq "uaccess.h" || $d eq "syscalls.h") {
                s<^#define (.*?) \\\n__typeof__\(__builtin_choose_expr\((.*?), (.*?), (.*?)\)\)(.*)><#if __cplusplus\n#define $1 typename conditional<($2), __typeof($3), __typeof($4)>::type$5\n#else\n#define $1 __typeof__(__builtin_choose_expr($2, $3, $4))$5\n#endif>m;
            }
            if ($d eq "cpufeature.h") {
                s{^#include <linux/bitops.h>}{/* #include <linux/bitops.h> */}mg;
            }
            if ($d eq "sections.h") {
                s{^extern(.*?) const void}{extern$1 const char}mg;
            }
            if ($d eq "irq.h") {
                s{enum irqchip_irq_state;}{}g;
                s{enum irqchip_irq_state\b}{int}g;
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
	    open(F, ">$outputroot$dd/$d") || die "click-linuxtool.pl: $outputroot$dd/$d: $!";
	    print F "/* created by click/linuxmodule/click-linuxtool.pl on " . localtime() . " */\n/* from $f */\n", $_;
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
while (@ARGV) {
    my($i) = shift @ARGV;
    if ($i =~ /^-I(.*)/) {
	if (!-d $1) {
	    # do not change argument
	    push @new_argv, $i;
	} elsif (!$done{$1}) {
	    $dir = "$outputroot/include$numdirs";
	    -d $dir || mkdir $dir || die "click-linuxtool.pl: mkdir $dir: $!";
	    $done{$1} = $dir;
	    ++$numdirs;
	    one_includeroot($1, $dir);
	    push @new_argv, "-I" . $dir;
	} else {
	    push @new_argv, "-I" . $done{$1};
	}
    } elsif ($i eq "-include") {
        push @new_argv, $i;
        $i = shift @ARGV;
        if ($i =~ /^\//) {
            push @new_argv, $i;
        } else {
            my($cwd) = `pwd`;
            $cwd =~ s/\s+\z//;
            $cwd .= "/" if $cwd !~ /\/\z/;
            push @new_argv, $cwd . $i;
            $done{$i} = $cwd . $i;
        }
    } else {
	push @new_argv, $i;
    }
}
print join(" ", @new_argv), "\n";

my(@sed, $k, $v);
while (($k, $v) = each %done) {
    push @sed, "-e s,-I$k,-I$v,";
}
@sed = sort { length($b) <=> length($a) } @sed;
push @sed, "-e s,\\ -I,\\ -isystem\\ ,g";
print join(" ", @sed), "\n";

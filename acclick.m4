dnl -*- mode: shell-script; -*-

dnl
dnl Common Click configure.in functions
dnl

dnl
dnl CLICK_INIT
dnl Initialize Click configure functionality. Must be called before
dnl CC or CXX are defined.
dnl Check whether the user specified which compilers we should use.
dnl If so, we don't screw with their choices later.
dnl

AC_DEFUN([CLICK_INIT], [
    ac_user_cc= ; test -n "$CC" && ac_user_cc=y
    ac_user_kernel_cc= ; test -n "$KERNEL_CC" && ac_user_kernel_cc=y
    ac_user_cxx= ; test -n "$CXX" && ac_user_cxx=y
    ac_user_build_cxx= ; test -n "$BUILD_CXX" && ac_user_build_cxx=y
    ac_user_kernel_cxx= ; test -n "$KERNEL_CXX" && ac_user_kernel_cxx=y
    ac_compile_with_warnings=y

    conf_auxdir=$1
    AC_SUBST(conf_auxdir)
])


dnl
dnl CLICK_PROG_CC
dnl Find the C compiler, and make sure it is suitable.
dnl

AC_DEFUN([CLICK_PROG_CC], [
    AC_REQUIRE([AC_PROG_CC])

    ac_base_cc="$CC"
    test -z "$ac_user_cc" -a -n "$GCC" -a -n "$ac_compile_with_warnings" && \
	CC="$CC -W -Wall -MD"

    CFLAGS_NDEBUG=`echo "$CFLAGS" | sed 's/-g//'`
    AC_SUBST(CFLAGS_NDEBUG)
])


dnl
dnl CLICK_PROG_CXX
dnl Find the C++ compiler, and make sure it is suitable.
dnl

AC_DEFUN([CLICK_PROG_CXX], [
    AC_REQUIRE([AC_PROG_CXX])

    dnl work around Autoconf 2.53, which #includes <stdlib.h> inappropriately
    if grep __cplusplus confdefs.h >/dev/null 2>&1; then
	sed 's/#ifdef __cplusplus/#if defined(__cplusplus) \&\& !defined(__KERNEL__)/' < confdefs.h > confdefs.h~
	mv confdefs.h~ confdefs.h
    fi

    if test -z "$GXX"; then
	AC_MSG_WARN([
=========================================

Your C++ compiler ($CXX) is not a GNU C++ compiler!
Either set the 'CXX' environment variable to tell me where
a GNU C++ compiler is, or compile at your own risk.
(This code uses a few GCC extensions and GCC-specific compiler options,
and Linux header files are GCC-specific.)

=========================================])
    fi

    AC_LANG_CPLUSPLUS
    if test -n "$GXX"; then
	changequote(<<,>>)GXX_VERSION=`$CXX --version | head -1 | sed 's/^[^0-9]*\([0-9.]*\).*/\1/'`
	GXX_MAJOR=`echo $GXX_VERSION | sed 's/\..*//'`
	GXX_MINOR=`echo $GXX_VERSION | sed 's/^[^.]*\.\([^.]*\).*/\1/'`changequote([,])

	if test $GXX_MAJOR -lt 2 -o \( $GXX_MAJOR -eq 2 -a $GXX_MINOR -le 7 \); then
	    AC_MSG_ERROR([
=========================================

Your GNU C++ compiler ($CXX) is too old!
Either download a newer compiler, or tell me to use a different compiler
by setting the 'CXX' environment variable and rerunning me.

=========================================])
	fi
    fi

    dnl check for <new> and <new.h>

    AC_CACHE_CHECK(whether <new> works, ac_cv_good_new_hdr,
	AC_TRY_LINK([#include <new>], [
  int a;
  int *b = new(&a) int;
  return 0;
], ac_cv_good_new_hdr=yes, ac_cv_good_new_hdr=no))
    if test "$ac_cv_good_new_hdr" = yes; then
	AC_DEFINE(HAVE_NEW_HDR)
    else
	AC_CACHE_CHECK(whether <new.h> works, ac_cv_good_new_h,
	    AC_TRY_LINK([#include <new.h>], [
  int a;
  int *b = new(&a) int;
  return 0;
], ac_cv_good_new_h=yes, ac_cv_good_new_h=no))
	if test "$ac_cv_good_new_h" = yes; then
	    AC_DEFINE(HAVE_NEW_H)
	fi
    fi

    dnl check for -fvtable-thunks

    VTABLE_THUNKS=
    test -n "$GXX" && test "$GXX_MAJOR" -lt 3 && VTABLE_THUNKS=-fvtable-thunks

    dnl define correct warning options

    CXX_WARNINGS=
    test -z "$ac_user_cxx" -a -n "$GXX" -a -n "$ac_compile_with_warnings" && \
	CXX_WARNINGS='-W -Wall'

    ac_base_cxx="$CXX"
    test -z "$ac_user_cxx" -a -n "$GXX" -a -n "$ac_compile_with_warnings" && \
	CXX="$CXX $CXX_WARNINGS -fno-exceptions -fno-rtti $VTABLE_THUNKS -MD"

    CXXFLAGS_NDEBUG=`echo "$CXXFLAGS" | sed 's/-g//'`
    AC_SUBST(CXXFLAGS_NDEBUG)
])


dnl
dnl CLICK_PROG_BUILD_CXX
dnl Prepare the C++ compiler for the build host.
dnl

AC_DEFUN([CLICK_PROG_BUILD_CXX], [
    dnl This doesn't really work, but it's close.
    ac_base_build_cxx="$CXX"
    test -z "$ac_user_build_cxx" -a -n "$ac_compile_with_warnings" && \
	BUILD_CXX="$BUILD_CXX $CXX_WARNINGS -fno-exceptions -fno-rtti $VTABLE_THUNKS"
])


dnl
dnl CLICK_PROG_KERNEL_CC
dnl Prepare the kernel-ready C compiler.
dnl

AC_DEFUN([CLICK_PROG_KERNEL_CC], [
    AC_REQUIRE([CLICK_PROG_CC])
    test -z "$ac_user_kernel_cc" && \
	KERNEL_CC="$ac_base_cc -MD"
    test -z "$ac_user_kernel_cc" -a -n "$GCC" -a -n "$ac_compile_with_warnings" && \
	KERNEL_CC="$ac_base_cc -w $CXX_WARNINGS -MD"
    AC_SUBST(KERNEL_CC)
])


dnl
dnl CLICK_PROG_KERNEL_CXX
dnl Prepare the kernel-ready C++ compiler.
dnl

AC_DEFUN([CLICK_PROG_KERNEL_CXX], [
    AC_REQUIRE([CLICK_PROG_CXX])
    test -z "$ac_user_kernel_cxx" && \
	KERNEL_CXX="$ac_base_cxx -MD"
    test -z "$ac_user_kernel_cxx" -a -n "$GXX" -a -n "$ac_compile_with_warnings" && \
	KERNEL_CXX="$ac_base_cxx -w $CXX_WARNINGS -fno-exceptions -fno-rtti $VTABLE_THUNKS -MD"
    AC_SUBST(KERNEL_CXX)
])


dnl
dnl CLICK_CHECK_DYNAMIC_LINKING
dnl Defines HAVE_DYNAMIC_LINKING and DL_LIBS if <dlfcn.h> and -ldl exist 
dnl and work.
dnl

AC_DEFUN([CLICK_CHECK_DYNAMIC_LINKING], [
    DL_LIBS=
    AC_CHECK_HEADERS(dlfcn.h, ac_have_dlfcn_h=yes, ac_have_dlfcn_h=no)
    AC_CHECK_FUNC(dlopen, ac_have_dlopen=yes,
	[AC_CHECK_LIB(dl, dlopen, [ac_have_dlopen=yes; DL_LIBS="-ldl"], ac_have_dlopen=no)])
    if test "x$ac_have_dlopen" = xyes -a "x$ac_have_dlfcn_h" = xyes; then
	AC_DEFINE(HAVE_DYNAMIC_LINKING)
	ac_have_dynamic_linking=yes
    fi
    AC_SUBST(DL_LIBS)
])


dnl
dnl CLICK_CHECK_BUILD_DYNAMIC_LINKING
dnl Defines HAVE_DYNAMIC_LINKING and DL_LIBS if <dlfcn.h> and -ldl exist 
dnl and work, on the build system. Must have done CLICK_CHECK_DYNAMIC_LINKING
dnl already.
dnl

AC_DEFUN([CLICK_CHECK_BUILD_DYNAMIC_LINKING], [
    saver="CXX='$CXX' CXXCPP='$CXXCPP' ac_cv_header_dlfcn_h='$ac_cv_header_dlfcn_h' ac_cv_func_dlopen='$ac_cv_func_dlopen' ac_cv_lib_dl_dlopen='$ac_cv_lib_dl_dlopen'"
    CXX="$BUILD_CXX"; CXXCPP="$BUILD_CXX -E"
    unset ac_cv_header_dlfcn_h ac_cv_func_dlopen ac_cv_lib_dl_dlopen
    BUILD_DL_LIBS=
    AC_CHECK_HEADERS(dlfcn.h, ac_build_have_dlfcn_h=yes, ac_build_have_dlfcn_h=no)
    AC_CHECK_FUNC(dlopen, ac_build_have_dlopen=yes,
	[AC_CHECK_LIB(dl, dlopen, [ac_build_have_dlopen=yes; BUILD_DL_LIBS="-ldl"], ac_have_dlopen=no)])
    if test "x$ac_build_have_dlopen" = xyes -a "x$ac_build_have_dlfcn_h" = xyes; then
	ac_build_have_dynamic_linking=yes
    fi
    if test "x$ac_build_have_dynamic_linking" != "x$ac_have_dynamic_linking"; then
	AC_MSG_ERROR([
=========================================

Build system and host system don't have the same dynamic linking state!

=========================================])
    fi
    AC_SUBST(BUILD_DL_LIBS)
    eval "$saver"
])


dnl
dnl CLICK_CHECK_LIBPCAP
dnl Finds header files and libraries for libpcap.
dnl

AC_DEFUN([CLICK_CHECK_LIBPCAP], [

    dnl header files

    HAVE_PCAP=yes
    if test "${PCAP_INCLUDES-NO}" = NO; then
	AC_CACHE_CHECK(for pcap.h, ac_cv_pcap_header_path,
	    AC_TRY_CPP([#include <pcap.h>],
	    ac_cv_pcap_header_path="found",
	    ac_cv_pcap_header_path='not found'
	    test -r /usr/local/include/pcap/pcap.h && \
		ac_cv_pcap_header_path='-I/usr/local/include/pcap'
	    test -r /usr/include/pcap/pcap.h && \
		ac_cv_pcap_header_path='-I/usr/include/pcap'))
	if test "$ac_cv_pcap_header_path" = 'not found'; then
	    HAVE_PCAP=
	elif test "$ac_cv_pcap_header_path" != 'found'; then
	    PCAP_INCLUDES="$ac_cv_pcap_header_path"
        fi
    fi

    if test "$HAVE_PCAP" = yes; then
	AC_CACHE_CHECK(whether pcap.h works, ac_cv_working_pcap_h,
	    saveflags="$CPPFLAGS"
	    CPPFLAGS="$saveflags $PCAP_INCLUDES"
	    AC_TRY_CPP([#include <pcap.h>], ac_cv_working_pcap_h=yes, ac_cv_working_pcap_h=no)
	    CPPFLAGS="$saveflags")
	test "$ac_cv_working_pcap_h" != yes && HAVE_PCAP=
    fi

    if test "$HAVE_PCAP" = yes; then
	AC_CACHE_CHECK(for bpf_timeval in pcap.h, ac_cv_bpf_timeval,
	    saveflags="$CPPFLAGS"
	    CPPFLAGS="$saveflags $PCAP_INCLUDES"
	    AC_EGREP_HEADER(bpf_timeval, pcap.h, ac_cv_bpf_timeval=yes, ac_cv_bpf_timeval=no)
	    CPPFLAGS="$saveflags")
	if test "$ac_cv_bpf_timeval" = yes; then
	    AC_DEFINE(HAVE_BPF_TIMEVAL)
	fi
    fi

    test "$HAVE_PCAP" != yes && PCAP_INCLUDES=
    AC_SUBST(PCAP_INCLUDES)


    dnl libraries

    if test "$HAVE_PCAP" = yes; then
	if test "${PCAP_LIBS-NO}" = NO; then
	    AC_CACHE_CHECK(for -lpcap, 
                ac_cv_pcap_library_path,
		saveflags="$LDFLAGS"
		savelibs="$LIBS"
		LIBS="$savelibs -lpcap $SOCKET_LIBS"
		AC_LANG_C
		AC_TRY_LINK_FUNC(pcap_open_live, 
                                ac_cv_pcap_library_path="found",
				LDFLAGS="$saveflags -L/usr/local/lib"
		                AC_TRY_LINK_FUNC(pcap_open_live, 
				    ac_cv_pcap_library_path="-L/usr/local/lib",
				    ac_cv_pcap_library_path="not found"))
		LDFLAGS="$saveflags"
		LIBS="$savelibs")
	else
	    AC_CACHE_CHECK(for -lpcap in "$PCAP_LIBS", 
                ac_cv_pcap_library_path,
		saveflags="$LDFLAGS"
		LDFLAGS="$saveflags $PCAP_LIBS"
		savelibs="$LIBS"
		LIBS="$savelibs -lpcap $SOCKET_LIBS"
		AC_LANG_C
		AC_TRY_LINK_FUNC(pcap_open_live, 
				ac_cv_pcap_library_path="$PCAP_LIBS",
			        ac_cv_pcap_library_path="not found")
		LDFLAGS="$saveflags"
		LIBS="$savelibs")
	fi
        if test "$ac_cv_pcap_library_path" = "found"; then
	    PCAP_LIBS='-lpcap'
	elif test "$ac_cv_pcap_library_path" != "not found"; then
	    PCAP_LIBS="$ac_cv_pcap_library_path -lpcap"
	else
	    HAVE_PCAP=
	fi
    fi

    test "$HAVE_PCAP" != yes && PCAP_LIBS=
    AC_SUBST(PCAP_LIBS)

    if test "$HAVE_PCAP" = yes; then
	AC_DEFINE(HAVE_PCAP)
    fi
])


dnl
dnl CLICK_PROG_INSTALL
dnl Substitute both INSTALL and INSTALL_IF_CHANGED.
dnl

AC_DEFUN([CLICK_PROG_INSTALL], [
    AC_REQUIRE([AC_PROG_INSTALL])
    AC_MSG_CHECKING(whether install accepts -C)
    echo X > conftest.1
    if $INSTALL -C conftest.1 conftest.2 >/dev/null 2>&1; then
	INSTALL_IF_CHANGED="$INSTALL -C"
	AC_MSG_RESULT(yes)
    else
	INSTALL_IF_CHANGED="$INSTALL"
	AC_MSG_RESULT(no)
    fi
    rm -f conftest.1 conftest.2
    AC_SUBST(INSTALL_IF_CHANGED)
])


dnl
dnl CLICK_PROG_AUTOCONF
dnl Substitute AUTOCONF.
dnl

AC_DEFUN([CLICK_PROG_AUTOCONF], [
    AC_MSG_CHECKING(for working autoconf)
    AUTOCONF="${AUTOCONF-autoconf}"
    if ($AUTOCONF --version) < /dev/null > conftest.out 2>&1; then
	if test `head -1 conftest.out | sed 's/.*2\.\([[0-9]]*\).*/\1/'` -ge 13 2>/dev/null; then
	    AC_MSG_RESULT(found)
	else
	    AUTOCONF='$(conf_auxdir)/missing autoconf'
	    AC_MSG_RESULT(old)
	fi
    else
	AUTOCONF='$(conf_auxdir)/missing autoconf'
	AC_MSG_RESULT(missing)
    fi
    AC_SUBST(AUTOCONF)
])


dnl
dnl CLICK_PROG_PERL5
dnl Substitute PERL.
dnl

AC_DEFUN(CLICK_PROG_PERL5, [
    dnl A IS-NOT A
    ac_foo=`echo 'exit($A<5);' | tr A \135`

    if test "${PERL-NO}" = NO; then
	AC_CHECK_PROGS(perl5, perl5 perl, missing)
	test "$perl5" != missing && $perl5 -e "$ac_foo" && perl5=missing
	if test "$perl5" = missing; then
	    AC_CHECK_PROGS(localperl5, perl5 perl, missing, /usr/local/bin)
	    test "$localperl5" != missing && \
		perl5="/usr/local/bin/$localperl5"
	fi
    else
	perl5="$PERL"
    fi
    
    test "$perl5" != missing && $perl5 -e "$ac_foo" && perl5=missing

    if test "$perl5" = "missing"; then
	PERL='$(conf_auxdir)/missing perl'
    else
	PERL="$perl5"
    fi
    AC_SUBST(PERL)
])


dnl
dnl CLICK_PROG_GMAKE
dnl Find GNU Make, if it is available.
dnl

AC_DEFUN([CLICK_PROG_GMAKE], [
    if test "${GMAKE-NO}" = NO; then
	AC_CACHE_CHECK(for GNU make, ac_cv_gnu_make,
	[if /bin/sh -c 'make -f /dev/null -n --version | grep GNU' >/dev/null 2>&1; then
	    ac_cv_gnu_make='make'
	elif /bin/sh -c 'gmake -f /dev/null -n --version | grep GNU' >/dev/null 2>&1; then
	    ac_cv_gnu_make='gmake'
	else
	    ac_cv_gnu_make='not found'
	fi])
	test "$ac_cv_gnu_make" != 'not found' && GMAKE="$ac_cv_gnu_make"
    else
	/bin/sh -c '$GMAKE -f /dev/null -n --version | grep GNU' >/dev/null 2>&1 || GMAKE=''
    fi

    SUBMAKE=''
    test -n "$GMAKE" -a "$GMAKE" != make && SUBMAKE="MAKE = $GMAKE"
    AC_SUBST(SUBMAKE)
])


dnl
dnl CLICK_CHECK_ALIGNMENT
dnl Check whether machine is indifferent to alignment. Defines
dnl HAVE_INDIFFERENT_ALIGNMENT.
dnl

AC_DEFUN([CLICK_CHECK_ALIGNMENT], [
    AC_CACHE_CHECK(whether machine is indifferent to alignment, ac_cv_alignment_indifferent,
    [AC_TRY_RUN([#ifdef __cplusplus
extern "C" void exit(int);
#else
void exit(int status);
#endif
void get_value(char *buf, int offset, int *value) {
    int i;
    for (i = 0; i < 4; i++)
	buf[i + offset] = i;
    *value = *((int *)(buf + offset));
}
int main(int argc, char *argv[]) {
    char buf[12];
    int value, i, try_value;
    get_value(buf, 0, &value);
    for (i = 1; i < 4; i++) {
	get_value(buf, i, &try_value);
	if (value != try_value)
	    exit(1);
    }
    exit(0);
}], ac_cv_alignment_indifferent=yes, ac_cv_alignment_indifferent=no,
	ac_cv_alignment_indifferent=no)])
    if test "x$ac_cv_alignment_indifferent" = xyes; then
	AC_DEFINE(HAVE_INDIFFERENT_ALIGNMENT)
    fi])


dnl
dnl CLICK_CHECK_INTEGER_TYPES
dnl Finds definitions for 'int8_t' ... 'int32_t' and 'uint8_t' ... 'uint32_t'.
dnl Also defines shell variable 'have_inttypes_h' to 'yes' iff the header
dnl file <inttypes.h> exists.  If 'uintXX_t' doesn't exist, try 'u_intXX_t'.
dnl

AC_DEFUN([CLICK_CHECK_INTEGER_TYPES], [
    AC_CHECK_HEADERS(inttypes.h, have_inttypes_h=yes, have_inttypes_h=no)

    if test $have_inttypes_h = no; then
	AC_CACHE_CHECK(for uintXX_t typedefs, ac_cv_uint_t,
	[AC_EGREP_HEADER(dnl
changequote(<<,>>)<<(^|[^a-zA-Z_0-9])uint32_t[^a-zA-Z_0-9]>>changequote([,]),
	sys/types.h, ac_cv_uint_t=yes, ac_cv_uint_t=no)])
    fi
    if test $have_inttypes_h = no -a "$ac_cv_uint_t" = no; then
	AC_CACHE_CHECK(for u_intXX_t typedefs, ac_cv_u_int_t,
	[AC_EGREP_HEADER(dnl
changequote(<<,>>)<<(^|[^a-zA-Z_0-9])u_int32_t[^a-zA-Z_0-9]>>changequote([,]),
	sys/types.h, ac_cv_u_int_t=yes, ac_cv_u_int_t=no)])
    fi
    if test $have_inttypes_h = yes -o "$ac_cv_uint_t" = yes; then :
    elif test "$ac_cv_u_int_t" = yes; then
	AC_DEFINE(HAVE_U_INT_TYPES)
    else
	AC_MSG_ERROR([
=========================================

Neither uint32_t nor u_int32_t defined by <inttypes.h> or <sys/types.h>!

=========================================])
    fi])


dnl
dnl CLICK_CHECK_INT64_TYPES
dnl Finds definitions for 'int64_t' and 'uint64_t'.
dnl On input, shell variable 'have_inttypes_h' should be 'yes' if the header
dnl file <inttypes.h> exists.  If no 'uint64_t', looks for 'u_int64_t'.
dnl

AC_DEFUN([CLICK_CHECK_INT64_TYPES], [
    if test "x$have_inttypes_h" = xyes; then
	inttypes_hdr='inttypes.h'
    else
	inttypes_hdr='sys/types.h'
    fi

    AC_CACHE_CHECK(for int64_t typedef, ac_cv_int64_t,
	[AC_EGREP_HEADER(dnl
changequote(<<,>>)<<(^|[^a-zA-Z_0-9])int64_t[^a-zA-Z_0-9]>>changequote([,]),
	$inttypes_hdr, ac_cv_int64_t=yes, ac_cv_int64_t=no)])
    AC_CACHE_CHECK(for uint64_t typedef, ac_cv_uint64_t,
	[AC_EGREP_HEADER(dnl
changequote(<<,>>)<<(^|[^a-zA-Z_0-9])u_?int64_t[^a-zA-Z_0-9]>>changequote([,]),
	$inttypes_hdr, ac_cv_uint64_t=yes, ac_cv_uint64_t=no)])

    have_int64_types=
    if test $ac_cv_int64_t = no -o $ac_cv_uint64_t = no; then
	AC_MSG_ERROR([
=========================================

int64_t types not defined by $inttypes_hdr!
Compile with '--disable-int64'.

=========================================])
    else
	AC_DEFINE(HAVE_INT64_TYPES)
	have_int64_types=yes

	AC_CACHE_CHECK(whether long and int64_t are the same type,
	    ac_cv_long_64, [AC_LANG_CPLUSPLUS
	    AC_TRY_COMPILE([#include <$inttypes_hdr>
void f1(long);
void f1(int64_t); // will fail if long and int64_t are the same type
], [], ac_cv_long_64=no, ac_cv_long_64=yes)])
	if test $ac_cv_long_64 = yes; then
	    AC_DEFINE(HAVE_64_BIT_LONG)
	fi
    fi])


dnl
dnl CLICK_CHECK_ENDIAN
dnl Checks endianness of machine.
dnl

AC_DEFUN([CLICK_CHECK_ENDIAN], [
    AC_CHECK_HEADERS(endian.h machine/endian.h, 
dnl autoconf 2.53 versus autoconf 2.13
		    if test "x$ac_header" != x; then
		        endian_hdr=$ac_header
		    else
			endian_hdr=$ac_hdr
		    fi
		    break, endian_hdr=no)
    if test "x$endian_hdr" != xno; then
	AC_CACHE_CHECK(endianness, ac_cv_endian,
	    dnl can't use AC_TRY_CPP because it throws out the results
	    ac_cv_endian=0
	    [cat > conftest.$ac_ext <<EOF
[#]line __oline__ "configure"
#include "confdefs.h"
#include <$endian_hdr>
#ifdef __BYTE_ORDER
__BYTE_ORDER
#elif defined(BYTE_ORDER)
BYTE_ORDER
#else
0
#endif
EOF
	    ac_try="$ac_cpp conftest.$ac_ext >conftest.result 2>conftest.out"
	    AC_TRY_EVAL(ac_try)
	    ac_err=`grep -v '^ *+' conftest.out | grep -v "^conftest.${ac_ext}\$"`
	    if test -z "$ac_err"; then
		ac_cv_endian=`grep '^[[1234]]' conftest.result`
		test -z "$ac_cv_endian" && ac_cv_endian=0
	    else
		echo "$ac_err" >&5
		echo "configure: failed program was:" >&5
		cat conftest.$ac_ext >&5
	    fi
	    rm -f conftest*]
	)
    elif test "x$cross_compiling" != xyes ; then
	AC_CACHE_CHECK(endianness, ac_cv_endian,
	    [AC_TRY_RUN([#ifdef __cplusplus
extern "C" void exit(int);
#else
void exit(int status);
#endif
#include <stdio.h>
int main(int argc, char *argv[]) {
    union { int i; char c[4]; } u;
    FILE *f = fopen("conftestdata", "w");
    if (!f)
	exit(1);
    u.i = ('1') | ('2' << 8) | ('3' << 16) | ('4' << 24);
    fprintf(f, "%4.4s\n", u.c);
    exit(0);
}	    ], ac_cv_endian=`cat conftestdata`, ac_cv_endian=0, ac_cv_endian=0)]
	)
    else
	ac_cv_endian=0
    fi
    AC_DEFINE_UNQUOTED(CLICK_BYTE_ORDER, $ac_cv_endian)
    AC_CHECK_HEADERS(byteswap.h)
])

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
    ac_user_cxx= ; test -n "$CXX" && ac_user_cxx=y
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
    test -z "$ac_user_cc" -a -n "$GCC" -a -n "$ac_compile_with_warnings" && \
	CC="$CC -Wall"

    CFLAGS_NDEBUG=`echo "$CFLAGS" | sed 's/-g//'`
    AC_SUBST(CFLAGS_NDEBUG)
])


dnl
dnl CLICK_PROG_CXX
dnl Find the C++ compiler, and make sure it is suitable.
dnl

AC_DEFUN([CLICK_PROG_CXX], [
    AC_REQUIRE([AC_PROG_CXX])

    if test -z "$GXX"; then
	AC_MSG_WARN(Your C++ compiler ($CXX) is not a GNU C++ compiler!
Either set the "'`'"CXX' environment variable to tell me where
[a GNU C++ compiler is, or compile at your own risk.
(This code uses a few GCC extensions and GCC-specific compiler options,
and Linux header files are GCC-specific.)])
    fi

    AC_LANG_CPLUSPLUS
    AC_CACHE_CHECK(for recent version of C++, ac_cv_good_cxx,
	AC_TRY_COMPILE([], [#ifdef __GNUG__
#if (__GNUC__ == 2) && (__GNUC_MINOR__ <= 7)
#error "fuckers! fuckers!"
#endif
#endif
return 0;], ac_cv_good_cxx=yes, ac_cv_good_cxx=no))
    if test "$ac_cv_good_cxx" != yes; then
	AC_MSG_ERROR(Your GNU C++ compiler ($CXX) is too old!
[Either download a newer compiler, or tell me to use a different compiler
by setting the "'`'"CXX' environment variable and rerunning me.])
    fi

    dnl check for <new.h>

    AC_CACHE_CHECK(for working new.h, ac_cv_good_new_h,
	AC_TRY_LINK([#include <new.h>], [
  int a;
  int *b = new(&a) int;
  return 0;
], ac_cv_good_new_h=yes, ac_cv_good_new_h=no))
    if test "$ac_cv_good_new_h" = yes; then
	AC_DEFINE(HAVE_NEW_H)
    fi

    ac_base_cxx="$CXX"
    test -z "$ac_user_cxx" -a -n "$GXX" -a -n "$ac_compile_with_warnings" && \
	CXX="$CXX -Wp,-w -W -Wall -fno-exceptions -fno-rtti -fvtable-thunks"

    CXXFLAGS_NDEBUG=`echo "$CXXFLAGS" | sed 's/-g//'`
    AC_SUBST(CXXFLAGS_NDEBUG)
])


dnl
dnl CLICK_PROG_KERNEL_CXX
dnl Prepare the kernel-ready C++ compiler.
dnl

AC_DEFUN([CLICK_PROG_KERNEL_CXX], [
    AC_REQUIRE([CLICK_PROG_CXX])
    test -z "$ac_user_kernel_cxx" && \
	KERNEL_CXX="$ac_base_cxx"
    test -z "$ac_user_kernel_cxx" -a -n "$GXX" -a -n "$ac_compile_with_warnings" && \
	KERNEL_CXX="$ac_base_cxx -w -Wall -fno-exceptions -fno-rtti -fvtable-thunks"
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
    fi
    AC_SUBST(DL_LIBS)
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
    
    if test "$HAVE_PCAP" = yes -a ${PCAP_LIBS-NO} = NO; then
	AC_CACHE_CHECK(for -lpcap, ac_cv_pcap_library_path,
	    saveflags="$LDFLAGS"
	    savelibs="$LIBS"
	    LIBS="$savelibs -lpcap"
	    AC_LANG_C
	    AC_TRY_LINK_FUNC(pcap_open_live, ac_cv_pcap_library_path="found",
		LDFLAGS="$saveflags -L/usr/local/lib"
		AC_TRY_LINK_FUNC(pcap_open_live, ac_cv_pcap_library_path="-L/usr/local/lib",
		    ac_cv_pcap_library_path="not found"))
	    LDFLAGS="$saveflags"
	    LIBS="$savelibs")

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
    AUTOCONF=${AUTOCONF-autoconf}
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

    if test ${PERL-NO} = NO; then
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
dnl file <inttypes.h> exists.
dnl

AC_DEFUN([CLICK_CHECK_INTEGER_TYPES], [
    AC_CHECK_HEADERS(inttypes.h, have_inttypes_h=yes, have_inttypes_h=no)

    if test $have_inttypes_h = no; then
	AC_CACHE_CHECK(for uintXX_t typedefs, ac_cv_uint_t,
	[AC_EGREP_HEADER(uint32_t, sys/types.h, ac_cv_uint_t=yes, ac_cv_uint_t=no)])
	if test $ac_cv_uint_t = no; then
	    AC_MSG_ERROR("uint32_t not defined by <inttypes.h> or <sys/types.h>!")
	fi
    fi])


dnl
dnl CLICK_CHECK_INT64_TYPES
dnl Finds definitions for 'int64_t' and 'uint64_t'.
dnl On input, shell variable 'have_inttypes_h' should be 'yes' if the header
dnl file <inttypes.h> exists.
dnl

AC_DEFUN([CLICK_CHECK_INT64_TYPES], [
    if test "x$have_inttypes_h" = xyes; then
	inttypes_hdr='inttypes.h'
    else
	inttypes_hdr='sys/types.h'
    fi

    AC_CACHE_CHECK(for int64_t typedef, ac_cv_int64_t,
[AC_EGREP_HEADER(int64_t, $inttypes_hdr, ac_cv_int64_t=yes, ac_cv_int64_t=no)])
    AC_CACHE_CHECK(for uint64_t typedef, ac_cv_uint64_t,
[AC_EGREP_HEADER(uint64_t, $inttypes_hdr, ac_cv_uint64_t=yes, ac_cv_uint64_t=no)])

    if test $ac_cv_int64_t = no -o $ac_cv_uint64_t = no; then
	AC_MSG_ERROR(int64_t types not defined by $inttypes_hdr!
Compile with "'`'"--disable-int64'.)
    else
	AC_DEFINE_UNQUOTED(HAVE_INT64_TYPES)
    fi])


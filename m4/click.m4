dnl -*- mode: shell-script; -*-

dnl
dnl Common Click configure.in functions
dnl

dnl
dnl CLICK_INIT(conf_auxdir, [packagename])
dnl Initialize Click configure functionality. Must be called before
dnl CC or CXX are defined.
dnl Check whether the user specified which compilers we should use.
dnl If so, we don't screw with their choices later.
dnl

AC_DEFUN([CLICK_INIT], [
    ac_user_cc=${CC+y}
    ac_user_cflags=${CFLAGS+y}
    ac_user_kernel_cc=${KERNEL_CC+y}
    ac_user_kernel_cflags=${KERNEL_CFLAGS+y}
    ac_user_cxx=${CXX+y}
    ac_user_cxxflags=${CXXFLAGS+y}
    ac_user_kernel_cxx=${KERNEL_CXX+y}
    ac_user_kernel_cxxflags=${KERNEL_CXXFLAGS+y}
    ac_user_build_cxx=${BUILD_CXX+y}
    ac_user_depcflags=${DEPCFLAGS+y}
    ac_user_depdirflag=${DEPDIRFLAG+y}
    ac_compile_with_warnings=y

    conf_auxdir=$1
    AC_SUBST(conf_auxdir)

    ifelse([$2], [], [], [
        AC_DEFUN([CLICK_PACKAGENAME], [$2])
        CLICKPACKAGENAME=$2
        AC_SUBST(CLICKPACKAGENAME)
    ])
])


dnl
dnl CLICK_PROG_CC
dnl Find the C compiler, and make sure it is suitable.
dnl

AC_DEFUN([CLICK_PROG_CC], [
    AC_REQUIRE([AC_PROG_CC])

    ac_base_cc="$CC"
    ac_base_cflags="$CFLAGS"

    test -z "$ac_user_cflags" -a -n "$GCC" -a -n "$ac_compile_with_warnings" -a -z "$ac_user_depcflags" && \
        DEPCFLAGS="-MD -MP"
    AC_SUBST(DEPCFLAGS)
    test -z "$ac_user_cflags" -a -n "$GCC" -a -n "$ac_compile_with_warnings" -a -z "$ac_user_depdirflag" && \
        DEPDIRFLAG=['-MF $(DEPDIR)/$][*.d']
    AC_SUBST(DEPDIRFLAG)

    save_cflags="$CFLAGS"
    AC_CACHE_CHECK([whether the C compiler accepts -W -Wall], [ac_cv_c_w_wall], [
        CFLAGS="$CFLAGS -W -Wall"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[int f(int x) { return x; }]], [[]])],
            [ac_cv_c_w_wall=yes], [ac_cv_c_w_wall=no])])
    AC_CACHE_CHECK([whether the C compiler accepts -Werror], [ac_cv_c_werror], [
        CFLAGS="$CFLAGS -Werror"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[int f(int x) { return x; }]], [[]])],
            [ac_cv_c_werror=yes], [ac_cv_c_werror=no])])
    CFLAGS="$save_cflags"

    WARNING_CFLAGS=
    test -n "$ac_cv_c_w_wall" -a -n "$ac_compile_with_warnings" && \
        WARNING_CFLAGS=" -W -Wall"

    test -z "$ac_user_cflags" && \
        CFLAGS="$CFLAGS$WARNING_CFLAGS"

    AC_CHECK_HEADERS_ONCE([sys/types.h unistd.h])
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

    dnl check for <new> and <new.h>

    AC_CACHE_CHECK([whether <new> works], [ac_cv_good_new_hdr], [
        AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <new>]], [[
    int a;
    int *b = new(&a) int;
    return 0;
]])], [ac_cv_good_new_hdr=yes], [ac_cv_good_new_hdr=no])])
    if test "$ac_cv_good_new_hdr" = yes; then
        AC_DEFINE([HAVE_NEW_HDR], [1], [Define if <new> exists and works.])
    else
        AC_CACHE_CHECK([whether <new.h> works], [ac_cv_good_new_h], [
            AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <new.h>]], [[
    int a;
    int *b = new(&a) int;
    return 0;
]])], [ac_cv_good_new_h=yes], [ac_cv_good_new_h=no])])
        if test "$ac_cv_good_new_h" = yes; then
            AC_DEFINE([HAVE_NEW_H], [1], [Define if <new.h> exists and works.])
        fi
    fi

    dnl require C++11
    AC_CACHE_CHECK([whether the C++ compiler understands 'auto'], [ac_cv_cxx_auto], [
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[struct s { int a; }; int f(s x) { auto &y = x; return y.a; }]], [[]])],
            [ac_cv_cxx_auto=yes], [ac_cv_cxx_auto=no])])
    if test "$ac_cv_cxx_auto" != yes -a -z "$ac_user_cxx"; then
        CXX="${CXX} -std=gnu++0x"
        AC_MSG_CHECKING([whether the C++ compiler with -std=gnu++0x understands 'auto'])
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[struct s { int a; }; int f(s x) { auto &y = x; return y.a; }]], [[]])],
            [ac_cv_cxx_auto=yes], [ac_cv_cxx_auto=no])
        AC_MSG_RESULT([$ac_cv_cxx_auto])
    fi

    dnl check for C++11 features
    save_cxxflags="$CXXFLAGS"
    test -n "$ac_cv_c_w_wall" && CXXFLAGS="$CXXFLAGS -W -Wall"
    test -n "$ac_cv_c_werror" && CXXFLAGS="$CXXFLAGS -Werror"

    AC_CACHE_CHECK([whether the C++ compiler understands constexpr], [ac_cv_cxx_constexpr], [
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[constexpr int f(int x) { return x + 1; }]], [[]])],
            [ac_cv_cxx_constexpr=yes], [ac_cv_cxx_constexpr=no])])
    if test "$ac_cv_cxx_constexpr" = yes; then
        AC_DEFINE([HAVE_CXX_CONSTEXPR], [1], [Define if the C++ compiler understands constexpr.])
    fi

    AC_CACHE_CHECK([whether the C++ compiler understands rvalue references], [ac_cv_cxx_rvalue_references], [
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[int f(int &) { return 1; } int f(int &&) { return 0; }]], [[return f(int());]])],
            [ac_cv_cxx_rvalue_references=yes], [ac_cv_cxx_rvalue_references=no])])
    if test "$ac_cv_cxx_rvalue_references" = yes; then
        AC_DEFINE([HAVE_CXX_RVALUE_REFERENCES], [1], [Define if the C++ compiler understands rvalue references.])
    fi

    AC_CACHE_CHECK([whether the C++ compiler understands static_assert], [ac_cv_cxx_static_assert], [
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[const int f = 2;]], [[static_assert(f == 2, "f should be 2");]])],
            [ac_cv_cxx_static_assert=yes], [ac_cv_cxx_static_assert=no])])
    if test "$ac_cv_cxx_static_assert" = yes; then
        AC_DEFINE([HAVE_CXX_STATIC_ASSERT], [1], [Define if the C++ compiler understands static_assert.])
    fi

    AC_CACHE_CHECK([whether the C++ compiler understands template alias], [ac_cv_cxx_template_alias], [
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[template <typename T> struct X { typedef T type; }; template <typename T> using Y = X<T>; int f(int x) { return x; }]], [[return f(Y<int>::type());]])],
            [ac_cv_cxx_template_alias=yes], [ac_cv_cxx_template_alias=no])])
    if test "$ac_cv_cxx_template_alias" = yes; then
        AC_DEFINE([HAVE_CXX_TEMPLATE_ALIAS], [1], [Define if the C++ compiler understands template alias.])
    fi

    AC_CACHE_CHECK([[whether the C++ compiler understands #pragma interface]], [ac_cv_cxx_pragma_interface], [
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#pragma interface "foo.c"
#pragma implementation "foo.c"]], [[]])], [ac_cv_cxx_pragma_interface=yes], [ac_cv_cxx_pragma_interface=no])])
    if test "$ac_cv_cxx_pragma_interface" = yes; then
        AC_DEFINE([HAVE_CXX_PRAGMA_INTERFACE], [1], [Define if the C++ compiler understands #pragma interface.])
    fi

    CXXFLAGS="$save_cxxflags"

    dnl define correct warning options

    ac_base_cxx="$CXX"
    ac_base_cxxflags="$CXXFLAGS"

    test -z "$ac_user_cxxflags" -a -n "$GXX" && \
        CXXFLAGS="$CXXFLAGS$WARNING_CFLAGS"
])


dnl
dnl CLICK_PROG_BUILD_CXX
dnl Prepare the C++ compiler for the build host.
dnl

AC_DEFUN([CLICK_PROG_BUILD_CXX], [
    dnl This doesn't really work, but it's close.
    ac_base_build_cxx="$CXX"
    test -z "$ac_user_build_cxx" -a -n "$ac_compile_with_warnings" && \
        BUILD_CXX="$BUILD_CXX$WARNING_CFLAGS"
])


dnl
dnl CLICK_PROG_KERNEL_CC
dnl Prepare the kernel-ready C compiler.
dnl

AC_DEFUN([CLICK_PROG_KERNEL_CC], [
    AC_REQUIRE([CLICK_PROG_CC])
    test -z "$ac_user_kernel_cc" && \
        KERNEL_CC="$ac_base_cc"
    AC_SUBST(KERNEL_CC)

    test -z "$ac_kernel_cflags" && \
        KERNEL_CFLAGS=`echo "$ac_base_cflags -Wno-undef" | sed 's/-g//'`
    AC_SUBST(KERNEL_CFLAGS)
])


dnl
dnl CLICK_PROG_KERNEL_CXX
dnl Prepare the kernel-ready C++ compiler.
dnl

AC_DEFUN([CLICK_PROG_KERNEL_CXX], [
    AC_REQUIRE([CLICK_PROG_CXX])
    test -z "$ac_user_kernel_cxx" && \
        KERNEL_CXX="$ac_base_cxx"
    AC_SUBST(KERNEL_CXX)

    test -z "$ac_user_kernel_cxxflags" && \
        KERNEL_CXXFLAGS=`echo "$ac_base_cxxflags -fno-exceptions -fno-rtti -fpermissive -Wno-undef -Wno-pointer-arith" | sed 's/-g//'`
    AC_SUBST(KERNEL_CXXFLAGS)
])


dnl
dnl CLICK_CHECK_DYNAMIC_LINKING
dnl Defines HAVE_DYNAMIC_LINKING and DL_LIBS if <dlfcn.h> and -ldl exist
dnl and work.  Also defines LDMODULEFLAGS, the flags to pass to the linker
dnl when building a loadable module.
dnl

AC_DEFUN([CLICK_CHECK_DYNAMIC_LINKING], [
    DL_LIBS=
    AC_CHECK_HEADERS_ONCE([dlfcn.h])
    AC_CHECK_FUNC(dlopen, ac_have_dlopen=yes,
        [AC_CHECK_LIB(dl, dlopen, [ac_have_dlopen=yes; DL_LIBS="-ldl"], ac_have_dlopen=no)])
    if test "x$ac_have_dlopen" = xyes -a "x$ac_cv_header_dlfcn_h" = xyes; then
        AC_DEFINE([HAVE_DYNAMIC_LINKING], [1], [Define if dynamic linking is possible.])
        ac_have_dynamic_linking=yes
    fi
    AC_SUBST(DL_LIBS)

    DL_LDFLAGS=
    save_ldflags="$LDFLAGS"; LDFLAGS="$LDFLAGS -rdynamic"
    AC_MSG_CHECKING([whether linker accepts the -rdynamic flag])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],
        [ac_cv_rdynamic=yes; DL_LDFLAGS=-rdynamic], [ac_cv_rdynamic=no])
    AC_MSG_RESULT($ac_cv_rdynamic)
    LDFLAGS="$save_ldflags"
    AC_SUBST(DL_LDFLAGS)

    AC_MSG_CHECKING(compiler flags for building loadable modules)
    LDMODULEFLAGS=-shared
    SOSUFFIX=so
    if test "x$ac_have_dynamic_linking" = xyes; then
        if echo "$ac_cv_target" | grep apple-darwin >/dev/null 2>&1; then
            LDMODULEFLAGS='-dynamiclib -flat_namespace -undefined suppress'
            SOSUFFIX=dylib
        fi
    fi
    AC_MSG_RESULT($LDMODULEFLAGS)
    AC_SUBST(LDMODULEFLAGS)
    AC_SUBST(SOSUFFIX)
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
    AC_CHECK_HEADERS([dlfcn.h], [ac_build_have_dlfcn_h=yes], [ac_build_have_dlfcn_h=no])
    AC_CHECK_FUNC(dlopen, ac_build_have_dlopen=yes,
        [AC_CHECK_LIB(dl, dlopen, [ac_build_have_dlopen=yes; BUILD_DL_LIBS="-ldl"], ac_have_dlopen=no)])
    if test "x$ac_build_have_dlopen" = xyes -a "x$ac_build_have_dlfcn_h" = xyes; then
        ac_build_have_dynamic_linking=yes
    fi

    BUILD_DL_LDFLAGS=
    save_ldflags="$LDFLAGS"; LDFLAGS="$LDFLAGS -rdynamic"
    AC_MSG_CHECKING([whether linker accepts the -rdynamic flag])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[return 0;]])],
        [ac_cv_build_rdynamic=yes; BUILD_DL_LDFLAGS=-rdynamic], [ac_cv_build_rdynamic=no])
    AC_MSG_RESULT($ac_cv_build_rdynamic)
    LDFLAGS="$save_ldflags"
    AC_SUBST(BUILD_DL_LDFLAGS)

    if test "x$ac_build_have_dynamic_linking" != "x$ac_have_dynamic_linking"; then
        AC_MSG_ERROR([
=========================================

You have configured Click with '--enable-dynamic-linking', which requires
that both the build system and host system support dynamic linking.
Try running 'configure' with the '--disable-dynamic-linking' option.

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
        AC_CACHE_CHECK(for pcap.h, ac_cv_pcap_header_path, [
            AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <pcap.h>]])],
            ac_cv_pcap_header_path="found",
            [ac_cv_pcap_header_path='not found'
            test -r /usr/local/include/pcap/pcap.h && \
                ac_cv_pcap_header_path='-I/usr/local/include/pcap'
            test -r /usr/include/pcap/pcap.h && \
                ac_cv_pcap_header_path='-I/usr/include/pcap'])])
        if test "$ac_cv_pcap_header_path" = 'not found'; then
            HAVE_PCAP=
        elif test "$ac_cv_pcap_header_path" != 'found'; then
            PCAP_INCLUDES="$ac_cv_pcap_header_path"
        fi
    fi

    if test "$HAVE_PCAP" = yes; then
        AC_CACHE_CHECK(whether pcap.h works, ac_cv_working_pcap_h, [
            saveflags="$CPPFLAGS"
            CPPFLAGS="$saveflags $PCAP_INCLUDES"
            AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <pcap.h>]])], ac_cv_working_pcap_h=yes, ac_cv_working_pcap_h=no)
            CPPFLAGS="$saveflags"])
        test "$ac_cv_working_pcap_h" != yes && HAVE_PCAP=
    fi

    if test "$HAVE_PCAP" = yes; then
        saveflags="$CPPFLAGS"
        CPPFLAGS="$saveflags $PCAP_INCLUDES"
        AC_CACHE_CHECK(for bpf_timeval in pcap.h, ac_cv_bpf_timeval,
            AC_EGREP_HEADER(bpf_timeval, pcap.h, ac_cv_bpf_timeval=yes, ac_cv_bpf_timeval=no))
        if test "$ac_cv_bpf_timeval" = yes; then
            AC_DEFINE([HAVE_BPF_TIMEVAL], [1], [Define if <pcap.h> uses bpf_timeval.])
        fi
        AC_CHECK_DECLS([pcap_setnonblock], [], [], [#include <pcap.h>])
        CPPFLAGS="$saveflags"
    fi

    test "$HAVE_PCAP" != yes && PCAP_INCLUDES=
    AC_SUBST(PCAP_INCLUDES)


    dnl libraries

    if test "$HAVE_PCAP" = yes; then
        if test "${PCAP_LIBS-NO}" = NO; then
            AC_CACHE_CHECK([for -lpcap], [ac_cv_pcap_library_path], [
                saveflags="$LDFLAGS"
                savelibs="$LIBS"
                LIBS="$savelibs -lpcap $SOCKET_LIBS"
                AC_LANG_C
                AC_LINK_IFELSE([AC_LANG_CALL([[]], [[pcap_open_live]])], [ac_cv_pcap_library_path="found"],
                                [LDFLAGS="$saveflags -L/usr/local/lib"
                                AC_LINK_IFELSE([AC_LANG_CALL([[]], [[pcap_open_live]])], [ac_cv_pcap_library_path="-L/usr/local/lib"], [ac_cv_pcap_library_path="not found"])])
                LDFLAGS="$saveflags"
                LIBS="$savelibs"])
        else
            AC_CACHE_CHECK([for -lpcap in "$PCAP_LIBS"], [ac_cv_pcap_library_path], [
                saveflags="$LDFLAGS"
                LDFLAGS="$saveflags $PCAP_LIBS"
                savelibs="$LIBS"
                LIBS="$savelibs -lpcap $SOCKET_LIBS"
                AC_LANG_C
                AC_LINK_IFELSE([AC_LANG_CALL([[]], [[pcap_open_live]])], [ac_cv_pcap_library_path="$PCAP_LIBS"], [ac_cv_pcap_library_path="not found"])
                LDFLAGS="$saveflags"
                LIBS="$savelibs"])
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
        AC_DEFINE([HAVE_PCAP], [1], [Define if you have -lpcap and pcap.h.])

        savelibs="$LIBS"
        LIBS="$savelibs $PCAP_LIBS"
        AC_CHECK_FUNCS([pcap_inject pcap_sendpacket pcap_setdirection pcap_setnonblock pcap_set_immediate_mode])
        LIBS="$savelibs"
    fi
])


dnl
dnl CLICK_CHECK_NETMAP
dnl Finds header files for netmap.
dnl

AC_DEFUN([CLICK_CHECK_NETMAP], [
    AC_ARG_WITH([netmap],
        [AS_HELP_STRING([--with-netmap], [enable netmap [no]])],
        [use_netmap=$withval], [use_netmap=no])

    if test "$use_netmap" != "yes" -a "$use_netmap" != "no"; then
        if test "${NETMAP_INCLUDES-NO}" != NO; then
            :
        elif test -f "$use_netmap/net/netmap.h"; then
            NETMAP_INCLUDES="-I$use_netmap"
        elif test -f "$use_netmap/include/net/netmap.h"; then
            NETMAP_INCLUDES="-I$use_netmap/include"
        fi
    fi
    saveflags="$CPPFLAGS"
    CPPFLAGS="$saveflags $NETMAP_INCLUDES"

    HAVE_NETMAP=no
    AC_MSG_CHECKING([for net/netmap.h])
    AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <net/netmap.h>]])],
        [ac_cv_net_netmap_header_path="found"],
        [ac_cv_net_netmap_header_path="not found"])
    AC_MSG_RESULT($ac_cv_net_netmap_header_path)

    if test "$ac_cv_net_netmap_header_path" = "found"; then
        HAVE_NETMAP=yes
    fi

    if test "$HAVE_NETMAP" = yes; then
        AC_CACHE_CHECK([whether net/netmap.h works],
            [ac_cv_working_net_netmap_h], [
            AC_PREPROC_IFELSE([AC_LANG_SOURCE([[#include <net/netmap.h>]])],
                [ac_cv_working_net_netmap_h=yes],
                [ac_cv_working_net_netmap_h=no])])
        test "$ac_cv_working_net_netmap_h" != yes && HAVE_NETMAP=
    fi

    CPPFLAGS="$saveflags"
    if test "$HAVE_NETMAP" = yes -a "$use_netmap" != no; then
        AC_DEFINE([HAVE_NET_NETMAP_H], [1], [Define if you have the <net/netmap.h> header file.])
    fi
    AC_SUBST(NETMAP_INCLUDES)
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
        INSTALL_IF_CHANGED='${INSTALL} -C'
        AC_MSG_RESULT(yes)
    else
        INSTALL_IF_CHANGED='${top_builddir}/installch'
        AC_MSG_RESULT(no)
    fi
    rm -f conftest.1 conftest.2
    AC_SUBST([INSTALL_IF_CHANGED])
    CLICKINSTALL=`echo "$INSTALL" | sed 's|^\$(.*)/|\${clickdatadir}/|'`
    AC_SUBST([CLICKINSTALL])
    CLICK_BUILD_INSTALL="$INSTALL"
    AC_SUBST([CLICK_BUILD_INSTALL])
    CLICK_BUILD_INSTALL_IF_CHANGED="`echo "$INSTALL_IF_CHANGED" | sed 's|{INSTALL}|{CLICK_BUILD_INSTALL}|'`"
    AC_SUBST([CLICK_BUILD_INSTALL_IF_CHANGED])
])


dnl
dnl CLICK_PROG_AUTOCONF
dnl Substitute AUTOCONF.
dnl

AC_DEFUN([CLICK_PROG_AUTOCONF], [
    AC_MSG_CHECKING(for working autoconf)
    AUTOCONF="${AUTOCONF-autoconf}"
    if ($AUTOCONF --version) < /dev/null > conftest.out 2>&1; then
        if test `head -n 1 conftest.out | sed 's/.*2\.\([[0-9]]*\).*/\1/'` -ge 13 2>/dev/null; then
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

AC_DEFUN([CLICK_PROG_PERL5], [
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
        [if /bin/sh -c 'make -f /dev/null -n --version | grep GNU' >/dev/null 2>/dev/null; then
            ac_cv_gnu_make='make'
        elif /bin/sh -c 'gmake -f /dev/null -n --version | grep GNU' >/dev/null 2>/dev/null; then
            ac_cv_gnu_make='gmake'
        else
            ac_cv_gnu_make='not found'
        fi])
        test "$ac_cv_gnu_make" != 'not found' && GMAKE="$ac_cv_gnu_make"
    else
        /bin/sh -c '$GMAKE -f /dev/null -n --version | grep GNU' >/dev/null 2>/dev/null || GMAKE='1'
    fi

    SUBMAKE=''
    test -n "$GMAKE" -a "$GMAKE" != make && SUBMAKE="MAKE = $GMAKE"
    AC_SUBST(SUBMAKE)
    AC_SUBST(GMAKE)
])


dnl
dnl CLICK_CHECK_ALIGNMENT
dnl Check whether machine is indifferent to alignment. Defines
dnl HAVE_INDIFFERENT_ALIGNMENT.
dnl

AC_DEFUN([CLICK_CHECK_ALIGNMENT], [
    AC_CHECK_HEADERS_ONCE([inttypes.h])
    AC_CACHE_CHECK([whether machine is indifferent to alignment], [ac_cv_alignment_indifferent],
    [if test "x$ac_cv_header_inttypes_h" = xyes; then inttypes_hdr='inttypes.h'; else inttypes_hdr='sys/types.h'; fi

    AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <$inttypes_hdr>
#include <stdlib.h>
void get_value(char *buf, int offset, int32_t *value) {
    int i;
    for (i = 0; i < 4; i++)
        buf[i + offset] = i;
    *value = *((int32_t *)(buf + offset));
}
int main(int argc, char *argv[]) {
    char buf[12];
    int i;
    int32_t value, try_value;
    get_value(buf, 0, &value);
    for (i = 1; i < 4; i++) {
        get_value(buf, i, &try_value);
        if (value != try_value)
            exit(1);
    }
    exit(0);
}]])], [ac_cv_alignment_indifferent=yes], [ac_cv_alignment_indifferent=no],
        [ac_cv_alignment_indifferent=no])])
    if test "x$ac_cv_alignment_indifferent" = xyes; then
        AC_DEFINE([HAVE_INDIFFERENT_ALIGNMENT], [1], [Define if the machine is indifferent to alignment.])
    fi])


dnl
dnl CLICK_CHECK_INTEGER_TYPES
dnl Finds definitions for 'int8_t' ... 'int32_t' and 'uint8_t' ... 'uint32_t'.
dnl Also defines shell variable 'have_inttypes_h' to 'yes' iff the header
dnl file <inttypes.h> exists.  If 'uintXX_t' does not exist, try 'u_intXX_t'.
dnl Also defines __CHAR_UNSIGNED__ if 'char' is unsigned.
dnl

AC_DEFUN([CLICK_CHECK_INTEGER_TYPES], [
    AC_C_CHAR_UNSIGNED
    AC_CHECK_HEADERS_ONCE([stdint.h inttypes.h])

    if test x$ac_cv_header_inttypes_h = xno; then
        AC_CACHE_CHECK(for uintXX_t typedefs, ac_cv_uint_t,
        [AC_EGREP_HEADER(dnl
changequote(<<,>>)<<(^|[^a-zA-Z_0-9])uint32_t[^a-zA-Z_0-9]>>changequote([,]),
        sys/types.h, ac_cv_uint_t=yes, ac_cv_uint_t=no)])
    fi
    if test x$ac_cv_header_inttypes_h = xno -a "$ac_cv_uint_t" = no; then
        AC_CACHE_CHECK(for u_intXX_t typedefs, ac_cv_u_int_t,
        [AC_EGREP_HEADER(dnl
changequote(<<,>>)<<(^|[^a-zA-Z_0-9])u_int32_t[^a-zA-Z_0-9]>>changequote([,]),
        sys/types.h, ac_cv_u_int_t=yes, ac_cv_u_int_t=no)])
    fi
    if test x$ac_cv_header_inttypes_h = xyes -o "$ac_cv_uint_t" = yes; then :
    elif test "$ac_cv_u_int_t" = yes; then
        AC_DEFINE([HAVE_U_INT_TYPES], [1], [Define if you have u_intXX_t types but not uintXX_t types.])
    else
        AC_MSG_ERROR([
=========================================

Neither uint32_t nor u_int32_t defined by <inttypes.h> or <sys/types.h>!

=========================================])
    fi])


dnl
dnl CLICK_CHECK_INT64_TYPES
dnl Finds definitions for 'int64_t' and 'uint64_t'.
dnl If no 'uint64_t', looks for 'u_int64_t'.
dnl

AC_DEFUN([CLICK_CHECK_INT64_TYPES], [
    AC_CHECK_HEADERS_ONCE([inttypes.h])
    if test "x$ac_cv_header_inttypes_h" = xyes; then
        inttypes_hdr='inttypes.h'
    else
        inttypes_hdr='sys/types.h'
    fi

    AC_CHECK_TYPES(long long)
    AC_CHECK_TYPES([int64_t], [ac_cv_int64_t=yes], [ac_cv_int64_t=no], [#include <$inttypes_hdr>])
    AC_CHECK_TYPES([uint64_t], [ac_cv_uint64_t=yes], [ac_cv_uint64_t=no], [#include <$inttypes_hdr>])

    have_int64_types=
    if test $ac_cv_int64_t = no -o $ac_cv_uint64_t = no; then
        AC_MSG_ERROR([
=========================================

int64_t types not defined by $inttypes_hdr!
Compile with '--disable-int64'.

=========================================])
    else
        AC_DEFINE([HAVE_INT64_TYPES], [1], [Define if 64-bit integer types are enabled.])
        have_int64_types=yes

        AC_CACHE_CHECK(whether long and int64_t are the same type,
            ac_cv_long_64, [AC_LANG_CPLUSPLUS
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <$inttypes_hdr>
void f1(long) {
}
void f1(int64_t) { // will fail if long and int64_t are the same type
}]], [[]])], ac_cv_long_64=no, ac_cv_long_64=yes)])
        if test $ac_cv_long_64 = yes; then
            AC_DEFINE([HAVE_INT64_IS_LONG_USERLEVEL], [1], [Define if 'int64_t' is typedefed to 'long' at user level.])
        fi

        AC_CACHE_CHECK(whether long long and int64_t are the same type,
            ac_cv_long_long_64, [AC_LANG_CPLUSPLUS
            AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <$inttypes_hdr>
void f1(long long) {
}
void f1(int64_t) { // will fail if long long and int64_t are the same type
}]], [[]])], ac_cv_long_long_64=no, ac_cv_long_long_64=yes)])
        if test $ac_cv_long_long_64 = yes; then
            AC_DEFINE([HAVE_INT64_IS_LONG_LONG_USERLEVEL], [1], [Define if 'int64_t' is typedefed to 'long long' at user level.])
        fi
    fi])


dnl
dnl CLICK_CHECK_ENDIAN
dnl Checks endianness of machine.
dnl

AC_DEFUN([CLICK_CHECK_ENDIAN], [
    AC_CHECK_HEADERS_ONCE([endian.h machine/endian.h byteswap.h])
    AC_C_BIGENDIAN([endian=CLICK_BIG_ENDIAN], [endian=CLICK_LITTLE_ENDIAN], [endian=CLICK_NO_ENDIAN], [endian=CLICK_NO_ENDIAN])
    AC_DEFINE_UNQUOTED([CLICK_BYTE_ORDER], [$endian], [Define to byte order of target machine.])
])


dnl
dnl CLICK_CHECK_SIGNED_SHIFT
dnl Checks whether right-shift of a negative number is arithmetic or logical.
dnl

AC_DEFUN([CLICK_CHECK_SIGNED_SHIFT], [
    AC_CACHE_CHECK(whether signed right shift is arithmetic, ac_cv_arithmetic_right_shift,
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[int f(int x[((int) -1) >> 15]) { return x[0]; }]], [[]])], [ac_cv_arithmetic_right_shift=no], [ac_cv_arithmetic_right_shift=yes])])
    if test $ac_cv_arithmetic_right_shift = yes; then
        AC_DEFINE([HAVE_ARITHMETIC_RIGHT_SHIFT], [1], [Define if right shift of signed integers acts by sign extension.])
    fi])


dnl
dnl CLICK_CHECK_COMPILER_INTRINSICS
dnl Checks for '__builtin_clz', '__builtin_clzll', and other intrinsics.
dnl

AC_DEFUN([CLICK_CHECK_COMPILER_INTRINSICS], [
    AC_CACHE_CHECK([for __builtin_clz], [ac_cv_have___builtin_clz],
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[volatile int x = 11;]], [[int y = __builtin_clz(x);]])], [ac_cv_have___builtin_clz=yes], [ac_cv_have___builtin_clz=no])])
    if test $ac_cv_have___builtin_clz = yes; then
        AC_DEFINE([HAVE___BUILTIN_CLZ], [1], [Define if you have the __builtin_clz function.])
    fi
    AC_CACHE_CHECK([for __builtin_clzl], [ac_cv_have___builtin_clzl],
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[volatile long x = 11;]], [[int y = __builtin_clzl(x);]])], [ac_cv_have___builtin_clzl=yes], [ac_cv_have___builtin_clzl=no])])
    if test $ac_cv_have___builtin_clzl = yes; then
        AC_DEFINE([HAVE___BUILTIN_CLZL], [1], [Define if you have the __builtin_clzl function.])
    fi
    AC_CACHE_CHECK([for __builtin_clzll], [ac_cv_have___builtin_clzll],
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[volatile long long x = 11;]], [[int y = __builtin_clzll(x);]])], [ac_cv_have___builtin_clzll=yes], [ac_cv_have___builtin_clzll=no])])
    if test $ac_cv_have___builtin_clzll = yes; then
        AC_DEFINE([HAVE___BUILTIN_CLZLL], [1], [Define if you have the __builtin_clzll function.])
    fi

    AC_CACHE_CHECK([for __builtin_ffs], [ac_cv_have___builtin_ffs],
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[volatile int x = 11;]], [[int y = __builtin_ffs(x);]])], [ac_cv_have___builtin_ffs=yes], [ac_cv_have___builtin_ffs=no])])
    if test $ac_cv_have___builtin_ffs = yes; then
        AC_DEFINE([HAVE___BUILTIN_FFS], [1], [Define if you have the __builtin_ffs function.])
    fi
    AC_CACHE_CHECK([for __builtin_ffsl], [ac_cv_have___builtin_ffsl],
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[volatile long x = 11;]], [[int y = __builtin_ffsl(x);]])], [ac_cv_have___builtin_ffsl=yes], [ac_cv_have___builtin_ffsl=no])])
    if test $ac_cv_have___builtin_ffsl = yes; then
        AC_DEFINE([HAVE___BUILTIN_FFSL], [1], [Define if you have the __builtin_ffsl function.])
    fi
    AC_CACHE_CHECK([for __builtin_ffsll], [ac_cv_have___builtin_ffsll],
         [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[volatile long long x = 11;]], [[int y = __builtin_ffsll(x);]])], [ac_cv_have___builtin_ffsll=yes], [ac_cv_have___builtin_ffsll=no])])
    if test $ac_cv_have___builtin_ffsll = yes; then
        AC_DEFINE([HAVE___BUILTIN_FFSLL], [1], [Define if you have the __builtin_ffsll function.])
    fi

    AC_CACHE_CHECK([for __sync_synchronize], [ac_cv_have___sync_synchronize],
        [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[long x = 11;]], [[long *y = &x; __sync_synchronize();]])], [ac_cv_have___sync_synchronize=yes], [ac_cv_have___sync_synchronize=no])])
    if test $ac_cv_have___sync_synchronize = yes; then
        AC_DEFINE([HAVE___SYNC_SYNCHRONIZE], [1], [Define if you have the __sync_synchronize function.])
    fi
    AC_CACHE_CHECK([whether __sync_synchronize supports arguments], [ac_cv_have___sync_synchronize_args],
        [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[long x = 11;]], [[long *y = &x; __sync_synchronize(*y);]])], [ac_cv_have___sync_synchronize_args=yes], [ac_cv_have___sync_synchronize_args=no])])
    if test $ac_cv_have___sync_synchronize_args = yes; then
        AC_DEFINE([HAVE___SYNC_SYNCHRONIZE_ARGUMENTS], [1], [Define if the __sync_synchronize function supports arguments.])
    fi

    AC_CACHE_CHECK([for __has_trivial_copy], [ac_cv_have___has_trivial_copy],
        [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [[long x = 1; if (__has_trivial_copy(long)) x = 0;]])], [ac_cv_have___has_trivial_copy=yes], [ac_cv_have___has_trivial_copy=no])])
    if test $ac_cv_have___has_trivial_copy = yes; then
        AC_DEFINE([HAVE___HAS_TRIVIAL_COPY], [1], [Define if you have the __has_trivial_copy compiler intrinsic.])
    fi

    AC_CACHE_CHECK([for __thread storage class support], [ac_cv_have___thread],
        [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[__thread long x;]], [[x == 1;]])], [ac_cv_have___thread=yes], [ac_cv_have___thread=no])])
    if test $ac_cv_have___thread = yes; then
        AC_DEFINE([HAVE___THREAD_STORAGE_CLASS], [1], [Define if you have the __thread storage class specifier.])
    fi

    AC_CHECK_HEADERS_ONCE([strings.h])
    AC_CHECK_FUNCS(ffs ffsl ffsll)
    ])


dnl
dnl CLICK_CHECK_ADDRESSABLE_VA_LIST
dnl Checks whether the va_list type is addressable.
dnl

AC_DEFUN([CLICK_CHECK_ADDRESSABLE_VA_LIST], [
    AC_LANG_CPLUSPLUS
    AC_CACHE_CHECK([for addressable va_list type], [ac_cv_va_list_addr],
        [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdarg.h>
void f(va_list *) {
}
void g(va_list val) {
    f(&val);
}
void h(int a, ...) {
    va_list val;
    va_start(val, a);
    g(val);
    va_end(val);
}]], [[h(2, 3, 4);]])], ac_cv_va_list_addr=yes, ac_cv_va_list_addr=no)])
    if test "x$ac_cv_va_list_addr" = xyes; then
        AC_DEFINE([HAVE_ADDRESSABLE_VA_LIST], [1], [Define if the va_list type is addressable.])
    fi
])


dnl
dnl CLICK_CHECK_LARGE_FILE_SUPPORT
dnl Check whether C library supports large files. Defines
dnl HAVE_LARGE_FILE_SUPPORT.
dnl

AC_DEFUN([CLICK_CHECK_LARGE_FILE_SUPPORT], [
    AC_LANG_C
    AC_CACHE_CHECK([for large file support in C library],
        ac_cv_large_file_support,
        [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#define _LARGEFILE_SOURCE 1
#define _FILE_OFFSET_BITS 64
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
void h(off_t a) {
    int fd = open("/tmp/whatever", 0);
    lseek(fd, a, 0);
}]], [[h(15);]])], ac_cv_large_file_support=yes, ac_cv_large_file_support=no)])
    if test "x$ac_cv_large_file_support" = xyes; then
        AC_DEFINE([HAVE_LARGE_FILE_SUPPORT], [1], [Define if your C library contains large file support.])
    fi

    AC_CHECK_SIZEOF(off_t, [], [#if HAVE_LARGE_FILE_SUPPORT && HAVE_INT64_TYPES
# define _LARGEFILE_SOURCE 1
# define _FILE_OFFSET_BITS 64
#endif
#include <stdio.h>
#include <sys/types.h>])
])


dnl
dnl CLICK_CHECK_POLL_H
dnl Check whether <poll.h> is available and not emulated.  Defines
dnl HAVE_POLL_H.
dnl

AC_DEFUN([CLICK_CHECK_POLL_H], [
    AC_CHECK_HEADERS_ONCE([poll.h])
    if test x"$ac_cv_header_poll_h" = xyes; then
        AC_CACHE_CHECK([whether <poll.h> is emulated], [ac_cv_emulated_poll_h],
            [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <poll.h>
#ifdef _POLL_EMUL_H_
# error "error"
#endif
]], [[]])], ac_cv_emulated_poll_h=no, ac_cv_emulated_poll_h=yes)])
        if test "x$ac_cv_emulated_poll_h" = xno; then
            AC_DEFINE([HAVE_POLL_H], [1], [Define if you have a non-emulated <poll.h> header file.])
        fi
    fi
])


dnl
dnl CLICK_CHECK_POSIX_CLOCKS
dnl Check whether <time.h> defines the clock_gettime() function, and whether
dnl the -lrt library is necessary to use it.  Defines HAVE_CLOCK_GETTIME and
dnl POSIX_CLOCK_LIBS.
dnl

AC_DEFUN([CLICK_CHECK_POSIX_CLOCKS], [
    have_clock_gettime=yes; POSIX_CLOCK_LIBS=
    AC_CHECK_DECLS([clock_gettime], [], [], [#ifdef HAVE_TIME_H
# include <time.h>
#endif])
    SAVELIBS="$LIBS"
    AC_SEARCH_LIBS([clock_gettime], [rt], [:], [have_clock_gettime=no])
    if test "x$have_clock_gettime" = xyes; then
        POSIX_CLOCK_LIBS="$LIBS"
    fi
    AC_SUBST(POSIX_CLOCK_LIBS)
    AC_CHECK_FUNC([clock_gettime], [:], [have_clock_gettime=no])
    LIBS="$SAVELIBS"
    if test "x$have_clock_gettime" = xyes; then
        AC_DEFINE([HAVE_CLOCK_GETTIME], [1], [Define if you have the clock_gettime function.])
    fi
])

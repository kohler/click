Name: e1000
Summary: Intel(R) PRO/1000 driver for Linux
Version: 4.3.15
Release: %(awk '{ print tolower($0) }' /etc/redhat-release | sed '{ s/.* \([0-9][^ ]*\).*/rh\1/; s/\.//g; }' || echo 1)
Source: %{name}-%{version}.tar.gz
Vendor: Intel Corporation
License: Dual GPL / BSD + patent grant
ExclusiveOS: linux
Group: System Environment/Kernel
Requires: kernel
Provides: %{name}
URL: http://support.intel.com/support/go/linux/e1000.htm
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
This package contains the Linux driver for the
Intel(R) PRO/1000 Family of Server Adapters.

%prep
%setup

%build
mkdir -p %{buildroot}

SwitchRHKernel () {
	CFLAGS_EXTRA=""
	for K in $2 ; do
		if [ $K == $1 ] ; then
			CFLAGS_EXTRA="$CFLAGS_EXTRA -D__BOOT_KERNEL_$K=1"
		else
			CFLAGS_EXTRA="$CFLAGS_EXTRA -D__BOOT_KERNEL_$K=0"
		fi
	done
}

KV=$(uname -r)

KSP="/lib/modules/$KV/build
     /usr/src/linux-$KV
     /usr/src/linux-$(echo $KV | sed 's/-.*//')
     /usr/src/kernel-headers-$KV
     /usr/src/kernel-source-$KV
     /usr/src/linux-$(echo $KV | sed 's/\([0-9]*\.[0-9]*\)\..*/\1/')
     /usr/src/linux"

KSRC=$(for d in $KSP ; do [ -e $d/include/linux ] && echo $d; echo;  done)
KSRC=$(echo $KSRC | awk '{ print $1 }')

if [ -e $KSRC/include/linux/rhconfig.h ] ; then
	RHKL=$(grep 'BOOT_KERNEL_.* [01]' /boot/kernel.h |
	       sed 's/.*BOOT_KERNEL_\(.*\) [01]/\1/')
	for K in $RHKL ; do
		SwitchRHKernel $K "$RHKL"
		make -C src clean
		make -C src INSTALL_MOD_PATH=%{buildroot} \
			MANDIR=%{_mandir} CFLAGS_EXTRA="$CFLAGS_EXTRA" install
	done
else
	make -C src clean
	make -C src INSTALL_MOD_PATH=%{buildroot} MANDIR=%{_mandir} install
fi

%install
cd %{buildroot}
find lib -name e1000.o -exec mv {} {}.new \; \
         -fprintf %{_builddir}/%{name}-%{version}/file.list "/%p.new\n"

%clean
rm -rf %{buildroot}

%files -f %{_builddir}/%{name}-%{version}/file.list
%defattr(-,root,root)
%{_mandir}/man7/e1000.7.gz
%doc LICENSE
%doc README
%doc ldistrib.txt
%doc file.list

%post
FL=%{_docdir}/%{name}-%{version}/file.list

if [ $1 -eq 1 ]; then
	for d in $(sed 's/^\(\/lib\/modules\/[^/]*\).*/\1/' $FL) ; do
		find $d -name e1000.o -exec mv -f {} {}.old \;
	done
fi
for f in $(sed 's/\.new$//' $FL) ; do
	ln -f $f.new $f 
done

uname -r | grep BOOT || /sbin/depmod -a > /dev/null 2>&1 || true

%preun
FL=%{_docdir}/%{name}-%{version}/file.list

if [ $1 -eq 0 ]; then
	for f in $(sed 's/\.new$//' $FL) ; do
		rm $f
	done
	for d in $(sed 's/^\(\/lib\/modules\/[^/]*\).*/\1/' $FL) ; do
		for f in $(find $d -name e1000.o.old -print) ; do
			mv $f $(echo $f | sed 's/\.old$//')
		done
	done
fi

%postun
uname -r | grep BOOT || /sbin/depmod -a > /dev/null 2>&1 || true


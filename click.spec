%define name click
%define version 1.5pre
%define release 1

Summary: The Click modular router
Name: %{name}
Version: %{version}
Release: %{release}
Source0: %{name}-%{version}.tar.gz
License: Click
Group: System/Networking
BuildRoot: %{_tmppath}/%{name}-buildroot
Prefix: %{_prefix}
Url: http://www.pdos.lcs.mit.edu/click
Vendor: Click Software Team

%description
Click is a modular software router developed by MIT LCS's Parallel and
Distributed Operating Systems group, Mazu Networks, the ICSI Center
for Internet Research, and now UCLA. Click routers are flexible,
configurable, and easy to understand. They're also pretty fast, for
software routers running on commodity hardware; on a 700 MHz Pentium
III, a Click IP router can handle up to 435,000 64-byte packets a
second.

A Click router is an interconnected collection of modules called
elements; elements control every aspect of the router's behavior, from
communicating with devices to packet modification to queueing,
dropping policies and packet scheduling. Individual elements can have
surprisingly powerful behavior, and it's easy to write new ones in
C++. You write a router configuration by gluing elements together with
a simple language.

We've designed several working configurations, including an Ethernet
switch and a standards-conformant IP router. You can run a
configuration at user level, using a driver program, or in a Linux 2.2
or 2.4 kernel with a kernel module. You can also manipulate
configurations with a variety of tools.

%prep
rm -fr $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT
%setup -q

%build
%configure --prefix=/usr --disable-linuxmodule --enable-analysis --enable-etherswitch --enable-grid --enable-ip6 --enable-ipsec --enable-local --enable-radio --enable-wifi --enable-multithread --enable-intel-cpu --enable-adaptive
make

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0755)
%doc AUTHORS FAQ INSTALL LICENSE README
%{_bindir}
%{_datadir}/click
%{_mandir}
%{_includedir}/click
%{_includedir}/clicknet
%{_includedir}/clicktool
%{_libdir}

%changelog
* Mon Jan 10 2005 Jonathan Day <imipak@yahoo.com>
- update for new syntax

* Fri May 28 2004 Mark Huang <mlhuang@cs.princeton.edu>
- add scriplets to install info files correctly

* Fri Apr 16 2004 Mark Huang <mlhuang@cs.princeton.edu>
- initial version

# end of file

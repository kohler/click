Name: e1000
Summary: Intel(R) PRO/1000 driver for Linux
Version: 6.1.16.2.DB
Release: 1
Source: %{name}-%{version}.tar.gz
Vendor: Intel Corporation
License: GPL
ExclusiveOS: linux
Group: System Environment/Kernel
Provides: %{name}
URL: http://support.intel.com/support/go/linux/e1000.htm
BuildRoot: %{_tmppath}/%{name}-%{version}-root
# macros for finding system files to update at install time (pci.ids, pcitable)
%define find() %(for f in %*; do if [ -e $f ]; then echo $f; break; fi; done)
%define _pciids   /usr/share/pci.ids        /usr/share/hwdata/pci.ids
%define _pcitable /usr/share/kudzu/pcitable /usr/share/hwdata/pcitable /dev/null
%define pciids    %find %{_pciids}
%define pcitable  %find %{_pcitable}
Requires: kernel, fileutils, findutils, gawk, bash

%description
This package contains the Linux driver for the
Intel(R) PRO/1000 Family of Server Adapters.

%prep
%setup

%build
mkdir -p %{buildroot}

KV=$(uname -r)
KA=$(uname -i)
KV_BASE=$(echo $KV | sed '{ s/hugemem//g; s/smp//g; s/enterprise//g; }' )

if [ -e /usr/src/kernels ] && [ $(echo $KV_BASE | grep "^2.6") ]; then
	if [ -e /etc/redhat-release ]; then
		KSP=$(ls /lib/modules | grep $KV_BASE)
		for K in $KSP ; do
			if [ $KA == "x86_64" ] && \
			   [ $(echo $K | grep hugemem) ]; then
				# Include path for x86_64 hugemem is broken
				# on RHEL4
				continue
			fi
			make -C src clean
			make -C src KSP=/lib/modules/$K/build \
				INSTALL_MOD_PATH=%{buildroot} \
				MANDIR=%{_mandir} \
				CFLAGS_EXTRA="$CFLAGS_EXTRA" install
		done
	else
		make -C src clean
		make -C src INSTALL_MOD_PATH=%{buildroot} \
			MANDIR=%{_mandir} install
	fi
else
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
		if echo $RHKL | grep BIGMEM
		then
			RHKL=$(echo $RHKL | sed 's/ENTERPRISE//')
		fi
		if echo $RHKL | grep HUGEMEM
		then
			RHKL=$(echo $RHKL | sed 's/BIGMEM//')
		fi
		for K in $RHKL ; do
			SwitchRHKernel $K "$RHKL"
			make -C src clean
			if [ $KA == "x86_64" ] ; then
				CFLAGS_EXTRA="$CFLAGS_EXTRA -D__MODULE_KERNEL_x86_64=0 -D__MODULE_KERNEL_ia32e=1"
			fi
			make -C src INSTALL_MOD_PATH=%{buildroot} \
				MANDIR=%{_mandir} CFLAGS_EXTRA="$CFLAGS_EXTRA" install
		done
	else
		make -C src clean
		make -C src INSTALL_MOD_PATH=%{buildroot} MANDIR=%{_mandir} install
	fi
fi

%install
# Append .new to driver name to avoid conflict with kernel RPM
cd %{buildroot}
find lib -name "e1000.*o" -exec mv {} {}.new \; \
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
FL="%{_docdir}/%{name}-%{version}/file.list
    %{_docdir}/%{name}/file.list"
FL=$(for d in $FL ; do if [ -e $d ]; then echo $d; break; fi;  done)

if [ -d /usr/local/lib/%{name} ]; then
	rm -rf /usr/local/lib/%{name}
fi

# Save old drivers (aka .o and .o.gz)
for k in $(sed 's/\/lib\/modules\/\([0-9a-zA-Z\.\-]*\).*/\1/' $FL) ; 
do
	d_drivers=/lib/modules/$k
	d_usr=/usr/local/lib/%{name}/$k
	mkdir -p $d_usr
	cd $d_drivers; find . -name %{name}.*o -exec cp --parents {} $d_usr \; -exec rm -f {} \;
	cd $d_drivers; find . -name %{name}_*.*o -exec cp --parents {} $d_usr \; -exec rm -f {} \;
	cd $d_drivers; find . -name %{name}.*o.gz -exec cp --parents {} $d_usr \; -exec rm -f {} \;
	cd $d_drivers; find . -name %{name}_*.*o.gz -exec cp --parents {} $d_usr \; -exec rm -f {} \;
done

# Add driver link
for f in $(sed 's/\.new$//' $FL) ; do
	ln -f $f.new $f 
done

# Check if kernel version rpm was built on IS the same as running kernel
BK_LIST=$(sed 's/\/lib\/modules\/\([0-9a-zA-Z\.\-]*\).*/\1/' $FL)
MATCH=no
for i in $BK_LIST
do
	if [ $(uname -r) == $i ] ; then
		MATCH=yes
		break
	fi
done
if [ $MATCH == no ] ; then
	echo -n "WARNING: Running kernel is $(uname -r).  "
	echo -n "RPM supports kernels (  "
	for i in $BK_LIST
	do
		echo -n "$i  "
	done
	echo ")"
fi

if [ -f /etc/redhat-release ] ; then
cat <<END > %{_docdir}/%{name}-%{version}/pci.updates
# updates for the system pci.ids file
#
# IMPORTANT!  Entries in this list must be sorted as they
#             would appear in the system pci.ids file.  Entries
#             are sorted by ven, dev, subven, subdev
#             (numerical order).
#
8086  Intel Corp.
	1000  82542 Gigabit Ethernet Controller
		1014 0119  Netfinity Gigabit Ethernet SX Adapter
		8086 1000  PRO/1000 Gigabit Server Adapter
	1001  82543GC Gigabit Ethernet Controller (Fiber)
		0e11 004a  NC6136 Gigabit Server Adapter
		8086 1003  PRO/1000 F Server Adapter
	1004  82543GC Gigabit Ethernet Controller (Copper)
		0e11 0049  NC7132 Gigabit Upgrade Module
		0e11 b1a4  NC7131 Gigabit Server Adapter
		8086 1004  PRO/1000 T Server Adapter
		8086 2004  PRO/1000 T Server Adapter
	1008  82544EI Gigabit Ethernet Controller (Copper)
		1028 011c  PRO/1000 XT Network Connection
		8086 1107  PRO/1000 XT Server Adapter
		8086 2107  PRO/1000 XT Server Adapter
		8086 2110  PRO/1000 XT Desktop Adapter
		8086 3108  PRO/1000 XT Network Connection
	1009  82544EI Gigabit Ethernet Controller (Fiber)
		8086 1109  PRO/1000 XF Server Adapter
		8086 2109  PRO/1000 XF Server Adapter
	100c  82544GC Gigabit Ethernet Controller (Copper)
		8086 1112  PRO/1000 T Desktop Adapter
		8086 2112  PRO/1000 T Desktop Adapter
	100d  82544GC Gigabit Ethernet Controller (LOM)
		1028 0123  PRO/1000 XT Network Connection
		8086 110d  82544GC Based Network Connection
	100e  82540EM Gigabit Ethernet Controller
		1014 0265  PRO/1000 MT Network Connection
		1014 0267  PRO/1000 MT Network Connection
		1014 026a  PRO/1000 MT Network Connection
		8086 001e  PRO/1000 MT Desktop Adapter
		8086 002e  PRO/1000 MT Desktop Adapter
	100f  82545EM Gigabit Ethernet Controller (Copper)
		1014 028e  PRO/1000 MT Network Connection
		8086 1000  PRO/1000 MT Network Connection
		8086 1001  PRO/1000 MT Server Adapter
	1010  82546EB Gigabit Ethernet Controller (Copper)
		8086 1011  PRO/1000 MT Dual Port Server Adapter
		8086 1012  PRO/1000 MT Dual Port Server Adapter
		8086 101a  PRO/1000 MT Dual Port Network Connection
	1011  82545EM Gigabit Ethernet Controller (Fiber)
		8086 1002  PRO/1000 MF Server Adapter
		8086 1003  PRO/1000 MF Server Adapter (LX)
	1012  82546EB Gigabit Ethernet Controller (Fiber)
		8086 1012  PRO/1000 MF Dual Port Server Adapter
	1013  82541EI Gigabit Ethernet Controller
		8086 1013  PRO/1000 MT Mobile Connection
	1014  82541ER Gigabit Ethernet Controller
		8086 1014  PRO/1000 MT Network Connection
		8086 0014  PRO/1000 MT Desktop Connection
	1015  82540EM Gigabit Ethernet Controller (LOM)
		8086 1015  PRO/1000 MT Mobile Connection
	1016  82540EP Gigabit Ethernet Controller
		1014 052c  PRO/1000 MT Mobile Connection
		1179 0001  PRO/1000 MT Mobile Connection
		8086 1016  PRO/1000 MT Mobile Connection
	1017  82540EP Gigabit Ethernet Controller
		8086 1017  PR0/1000 MT Desktop Connection
	1018  82541EI Gigabit Ethernet Controller
		8086 1018  PRO/1000 MT Mobile Connection
	1019  82547EI Gigabit Ethernet Controller
		8086 1019  PRO/1000 CT Desktop Connection
	101a  82547EI Gigabit Ethernet Controller
		8086 101a  PRO/1000 CT Mobile Connection
	101d  82546EB Gigabit Ethernet Controller
		8086 1000  PRO/1000 MT Quad Port Server Adapter
	101e  82540EP Gigabit Ethernet Controller
		1014 0549  PRO/1000 MT Mobile Connection
		8086 101e  PRO/1000 MT Mobile Connection
	1026  82545GM Gigabit Ethernet Controller
		8086 1000  PRO/1000 MT Server Connection
		8086 1001  PRO/1000 MT Server Adapter
		8086 1002  PRO/1000 MT Server Adapter
		8086 1026  PRO/1000 MT Server Connection
	1027  82545GM Gigabit Ethernet Controller
		8086 1001  PRO/1000 MF Server Adapter(LX)
		8086 1002  PRO/1000 MF Server Adapter(LX)
		8086 1003  PRO/1000 MF Server Adapter(LX)
		8086 1027  PRO/1000 MF Server Adapter
	1028  82545GM Gigabit Ethernet Controller
		8086 1028  PRO/1000 MB Server Connection
	105E  82571EB Gigabit Ethernet Controller
		8086 005E  PRO/1000 PT Dual Port Server Connection
		8086 105E  PRO/1000 PT Dual Port Network Connection
		8086 115E  PRO/1000 PT Dual Port Server Adapter
		8086 116E  PRO/1000 PT Dual Port Server Adapter
		8086 125E  PRO/1000 PT Dual Port Server Adapter
		8086 135E  PRO/1000 PT Dual Port Server Adapter
	105F  82571EB Gigabit Ethernet Controller
		8086 115F  PRO/1000 PF Dual Port Server Adapter
		8086 116F  PRO/1000 PF Dual Port Server Adapter
		8086 125F  PRO/1000 PF Dual Port Server Adapter
		8086 135F  PRO/1000 PF Dual Port Server Adapter
	1060  82571EB Gigabit Ethernet Controller
		8086 0060  PRO/1000 PB Dual Port Server Connection
		8086 1060  PRO/1000 PB Dual Port Server Connection
	1075  82547GI Gigabit Ethernet Controller
		8086 0075  PRO/1000 CT Network Connection
		8086 1075  PRO/1000 CT Network Connection
	1076  82541GI Gigabit Ethernet Controller
		8086 0076  PRO/1000 MT Network Connection
		8086 1076  PRO/1000 MT Network Connection
		8086 1176  PRO/1000 MT Desktop Adapter
		8086 1276  PRO/1000 MT Network Adapter
	1077  82541GI Gigabit Ethernet Controller
		1179 0001  PRO/1000 MT Mobile Connection
		8086 0077  PRO/1000 MT Mobile Connection
		8086 1077  PRO/1000 MT Mobile Connection
	1078  82541ER Gigabit Ethernet Controller
		8086 1078  82541ER-based Network Connection
	1079  82546GB Gigabit Ethernet Controller
		8086 0079  PRO/1000 MT Dual Port Network Connection
		8086 1079  PRO/1000 MT Dual Port Network Connection
		8086 1179  PRO/1000 MT Dual Port Server Adapter
		8086 117a  PRO/1000 MT Dual Port Server Adapter
	107a  82546GB Gigabit Ethernet Controller
		8086 107a  PRO/1000 MF Dual Port Server Adapter
		8086 127a  PRO/1000 MF Dual Port Server Adapter
	107b  82546GB Gigabit Ethernet Controller
		8086 007b  PRO/1000 MB Dual Port Server Connection
		8086 107b  PRO/1000 MB Dual Port Server Connection
	107c  82541PI Gigabit Ethernet Controller
		8086 1376  PRO/1000 GT Desktop Adapter
		8086 1476  PRO/1000 GT Desktop Adapter
	107d  82572EI Gigabit Ethernet Controller (Copper)
		8086 1082  PRO/1000 PT Server Adapter
		8086 1083  PRO/1000 PT Desktop Adapter
		8086 1092  PRO/1000 PT Server Adapter
		8086 1093  PRO/1000 PT Desktop Adapter
	107e  82572EI Gigabit Ethernet Controller (Fiber)
		8086 1084  PRO/1000 PF Server Adapter
		8086 1094  PRO/1000 PF Server Adapter
	107f  82572EI Gigabit Ethernet Controller
	108a  82546GB Gigabit Ethernet Controller
		8086 108a  PRO/1000 P Dual Port Server Adapter
		8086 118a  PRO/1000 P Dual Port Server Adapter
	108b  82573V Gigabit Ethernet Controller (Copper)
	108c  82573E Gigabit Ethernet Controller (Copper)
	109a  82573L Gigabit Ethernet Controller
		8086 109a  PRO/1000 PL Network Connection
END

#Yes, this really needs bash
bash -s %{pciids} \
	%{pcitable} \
	%{_docdir}/%{name}-%{version}/pci.updates \
	%{_docdir}/%{name}-%{version}/pci.ids.new \
	%{_docdir}/%{name}-%{version}/pcitable.new \
	%{name} \
<<"END"
#! /bin/bash
# $1 = system pci.ids file to update
# $2 = system pcitable file to update
# $3 = file with new entries in pci.ids file format
# $4 = pci.ids output file
# $5 = pcitable output file
# $6 = driver name for use in pcitable file

exec 3<$1
exec 4<$2
exec 5<$3
exec 6>$4
exec 7>$5
driver=$6
IFS=

# pattern matching strings
ID="[[:xdigit:]][[:xdigit:]][[:xdigit:]][[:xdigit:]]"
VEN="${ID}*"
DEV="	${ID}*"
SUB="		${ID}*"
TABLE_DEV="0x${ID}	0x${ID}	\"*"
TABLE_SUB="0x${ID}	0x${ID}	0x${ID}	0x${ID}	\"*"

line=
table_line=
ids_in=
table_in=
vendor=
device=
subdev=
subdev=
ven_str=
dev_str=
sub_str=

# force a sub-shell to fork with a new stdin
# this is needed if the shell is reading these instructions from stdin
while true
do
	# get the first line of each data file to jump start things
	exec 0<&3
	read -r ids_in
	exec 0<&4
	read -r table_in

	# outer loop reads lines from the updates file
	exec 0<&5
	while read -r line
	do
		# vendor entry
		if [[ $line == $VEN ]]
		then
			vendor=0x${line:0:4}
			ven_str=${line#${line:0:6}}
			# add entry to pci.ids
			exec 0<&3
			exec 1>&6
			while [[ $ids_in != $VEN ||
				 0x${ids_in:0:4} < $vendor ]]
			do
				echo "$ids_in"
				read -r ids_in
			done
			echo "$line"
			if [[ 0x${ids_in:0:4} == $vendor ]]
			then
				read -r ids_in
			fi

		# device entry
		elif [[ $line == $DEV ]]
		then
			device=0x${line:1:4}
			dev_str=${line#${line:0:7}}
			table_line="$vendor	$device	\"$driver\"	\"$ven_str|$dev_str\""
			# add entry to pci.ids
			exec 0<&3
			exec 1>&6
			while [[ $ids_in != $DEV ||
				 0x${ids_in:1:4} < $device ]]
			do
				if [[ $ids_in == $VEN ]]
				then
					break
				fi
				echo "$ids_in"
				read -r ids_in
			done
			echo "$line"
			if [[ 0x${ids_in:1:4} == $device ]]
			then
				read -r ids_in
			fi
			# add entry to pcitable
			exec 0<&4
			exec 1>&7
			while [[ $table_in != $TABLE_DEV ||
				 ${table_in:0:6} < $vendor ||
				 ( ${table_in:0:6} == $vendor &&
				   ${table_in:7:6} < $device ) ]]
			do
				echo "$table_in"
				read -r table_in
			done
			echo "$table_line"
			if [[ ${table_in:0:6} == $vendor &&
			      ${table_in:7:6} == $device ]]
			then
				read -r table_in
			fi

		# subsystem entry
		elif [[ $line == $SUB ]]
		then
			subven=0x${line:2:4}
			subdev=0x${line:7:4}
			sub_str=${line#${line:0:13}}
			table_line="$vendor	$device	$subven	$subdev	\"$driver\"	\"$ven_str|$sub_str\""
			# add entry to pci.ids
			exec 0<&3
			exec 1>&6
			while [[ $ids_in != $SUB ||
				 0x${ids_in:2:4} < $subven ||
				 ( 0x${ids_in:2:4} == $subven && 
				   0x${ids_in:7:4} < $subdev ) ]]
			do
				if [[ $ids_in == $VEN ||
				      $ids_in == $DEV ]]
				then
					break
				fi
				if [[ ! (${ids_in:2:4} == "1014" &&
					 ${ids_in:7:4} == "052C") ]]
				then
					echo "$ids_in"
				fi
				read -r ids_in
			done
			echo "$line"
			if [[ 0x${ids_in:2:4} == $subven  &&
			      0x${ids_in:7:4} == $subdev ]]
			then
				read -r ids_in
			fi
			# add entry to pcitable
			exec 0<&4
			exec 1>&7
			while [[ $table_in != $TABLE_SUB ||
				 ${table_in:14:6} < $subven ||
				 ( ${table_in:14:6} == $subven &&
				   ${table_in:21:6} < $subdev ) ]]
			do
				if [[ $table_in == $TABLE_DEV ]]
				then
					break
				fi
				if [[ ! (${table_in:14:6} == "0x1014" &&
					 ${table_in:21:6} == "0x052C") ]]
				then
					echo "$table_in"
				fi
				read -r table_in
			done
			echo "$table_line"
			if [[ ${table_in:14:6} == $subven &&
			      ${table_in:21:6} == $subdev ]]
			then
				read -r table_in
			fi
		fi

		exec 0<&5
	done

	# print the remainder of the original files
	exec 0<&3
	exec 1>&6
	echo "$ids_in"
	while read -r ids_in
	do
		echo "$ids_in"
	done

	exec 0>&4
	exec 1>&7
	echo "$table_in"
	while read -r table_in
	do
		echo "$table_in"
	done

	break
done <&5

exec 3<&-
exec 4<&-
exec 5<&-
exec 6>&-
exec 7>&-

END

mv -f %{_docdir}/%{name}-%{version}/pci.ids.new  %{pciids}
mv -f %{_docdir}/%{name}-%{version}/pcitable.new %{pcitable}
fi

uname -r | grep BOOT || /sbin/depmod -a > /dev/null 2>&1 || true

%preun
# If doing RPM un-install
if [ $1 -eq 0 ] ; then
	FL="%{_docdir}/%{name}-%{version}/file.list
    		%{_docdir}/%{name}/file.list"
	FL=$(for d in $FL ; do if [ -e $d ]; then echo $d; break; fi;  done)

	# Remove driver link
	for f in $(sed 's/\.new$//' $FL) ; do
		rm -f $f
	done

	# Restore old drivers
	if [ -d /usr/local/lib/%{name} ]; then
		cd /usr/local/lib/%{name}; find . -name '%{name}.*o*' -exec cp --parents {} /lib/modules/ \;
		cd /usr/local/lib/%{name}; find . -name '%{name}_*.*o*' -exec cp --parents {} /lib/modules/ \;
		rm -rf /usr/local/lib/%{name}
	fi
fi

%postun
uname -r | grep BOOT || /sbin/depmod -a > /dev/null 2>&1 || true


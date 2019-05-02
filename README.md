The Click Modular Router
========================
[![TravisCI status](https://travis-ci.org/kohler/click.svg?branch=master)](https://travis-ci.org/kohler/click)

Click is a modular router toolkit. To use it you'll need to know how to
compile and install the software, how to write router configurations, and how
to write new elements. Our [ACM Transactions on Computer Systems paper](http://dl.acm.org/citation.cfm?id=354874)
will give you a feeling for what Click can do. Using the optimization tools
under `CLICKDIR/tools`, you can get even better performance than that paper
describes.

Contents
--------

Subdirectory                  | Description
----------------------------- | -------------------------------
`CLICKDIR/apps`               | Click-related applications
`CLICKDIR/apps/clicky`        | GTK+ program for displaying configurations and interacting with drivers
`CLICKDIR/apps/csclient`      | Command-line program for interacting with drivers
`CLICKDIR/apps/ClickController` | Java program for interacting with drivers
`CLICKDIR/conf`               | example configuration files
`CLICKDIR/doc`                | documentation
`CLICKDIR/elements`           | element source code
`CLICKDIR/elements/analysis`  | …for trace analysis and manipulation
`CLICKDIR/elements/app`       | …for application-level protocols (e.g. FTP)
`CLICKDIR/elements/aqm`       | …for active queue management (e.g. RED)
`CLICKDIR/elements/ethernet`  | …for Ethernet
`CLICKDIR/elements/etherswitch` | …for an Ethernet switch
`CLICKDIR/elements/grid`      | …for the Grid mobile ad-hoc wireless network protocols
`CLICKDIR/elements/icmp`      | …for ICMP
`CLICKDIR/elements/ip`        | …for IPv4
`CLICKDIR/elements/ip6`       | …for IPv6
`CLICKDIR/elements/ipsec`     | …for IPsec
`CLICKDIR/elements/linuxmodule` | …for the Linux kernel driver
`CLICKDIR/elements/local`     | …for your own elements (empty)
`CLICKDIR/elements/ns`        | …for the NS network simulator driver
`CLICKDIR/elements/radio`     | …for communicating with wireless radios
`CLICKDIR/elements/standard`  | …for simple protocol-generic elements
`CLICKDIR/elements/tcpudp`    | …for TCP and UDP
`CLICKDIR/elements/test`      | …for regression tests
`CLICKDIR/elements/threads`   | …for thread management
`CLICKDIR/elements/userlevel` | …for the user-level driver
`CLICKDIR/elements/wifi`      | …for 802.11
`CLICKDIR/etc/samplepackage`  | sample source code for Click element package
`CLICKDIR/etc/samplellrpc`    | sample source code for reading Click LLRPCs
`CLICKDIR/etc/diagrams`       | files for drawing Click diagrams
`CLICKDIR/etc/libclick`       | files for standalone user-level Click library
`CLICKDIR/include/click`      | common header files
`CLICKDIR/include/clicknet`   | header files defining network headers
`CLICKDIR/lib`                | common non-element source code
`CLICKDIR/linuxmodule`        | Linux kernel module driver
`CLICKDIR/ns`                 | NS driver (integrates with the NS simulator)
`CLICKDIR/test`               | regression tests
`CLICKDIR/tools`              | Click tools
`CLICKDIR/tools/lib`          | …common code for tools
`CLICKDIR/tools/click-align`  | …enforces alignment for non-x86 machines
`CLICKDIR/tools/click-combine` | …merges routers into combined configuration
`CLICKDIR/tools/click-devirtualize` | …removes virtual functions from source
`CLICKDIR/tools/click-fastclassifier` | …specializes Classifiers into C++ code
`CLICKDIR/tools/click-mkmindriver` | …build environments for minimal drivers
`CLICKDIR/tools/click-install` | …installs configuration into kernel module
`CLICKDIR/tools/click-pretty` | …pretty-prints Click configuration as HTML
`CLICKDIR/tools/click-undead` | …removes dead code from configurations
`CLICKDIR/tools/click-xform`  | …pattern-based configuration optimizer
`CLICKDIR/tools/click2xml`    | …convert Click language <-> XML
`CLICKDIR/userlevel`          | user-level driver


Documentation
-------------

The `INSTALL.md` file in this directory contains installation instructions. User
documentation is in the `doc` subdirectory, which contains manual pages for
the Click language, the Linux kernel module, and several tools; it also has a
script that generates manual pages for many of the elements distributed in
this package. To install these manual pages so you can read them, follow the
`INSTALL.md` instructions, but `make install-man` instead of `make install`.


Running a Click Router
----------------------

Before playing with a Click router, you should get familiar with the Click
configuration language. You use this to tell Click how to process packets. The
language describes a graph of “elements,” or packet processing modules. See
the `doc/click.5` manual page for a detailed description, or check the `conf`
directory for some simple examples.

Click can be compiled as a user-level program or as a kernel module for Linux.
Either driver can receive and send packets; the kernel module directly
interacts with device drivers, while the user-level driver uses packet sockets
(on Linux) or the pcap library (everywhere else).

### User-Level Program

Run the user-level program by giving it the name of a configuration file:
`click CONFIGFILE`.

### Linux Kernel Module

See the `doc/click.o.8` manual page for a detailed description. To summarize,
install a configuration by running `click-install CONFIGFILE`. This will also
install the kernel module if necessary and report any errors to standard
error. (You must run `make install` before `click-install` will work.)

### NS-3 Simulator

See `INSTALL.md` for more information. Further information on NS-3 and Click is
available in [the NS-3 manual](http://www.nsnam.org/docs/models/html/click.html).

### NS-2 Simulator

See `INSTALL.md` for more information.  Once a Click-enabled version of NS-2 is
installed, the 'ns' command is able to run Click scripts as part of a normal
NS-2 simulation.

### DPDK

Click’s user-level driver supports DPDK. Before running in DPDK mode, the DPDK
must be set up properly as per the DPDK documentation. This mainly involves
setting up huge pages and binding some NIC to the DPDK userspace driver. E.g.,
to set up huge pages:

    echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages
    mkdir -p /mnt/huge
    mount -t hugetlbfs nodev /mnt/huge

On x86_64 you might achieve better performances with 1G huge pages, which must
be enabled through the kernel cmdline.

Intel NICs use entierly userspace drivers and needs to be bound to DPDK.
Eg., to bind eth0 to DPDK:

    modprobe uio_pci_generic
    dpdk/tools/dpdk_nic_bind.py --bind=uio_pci_generic eth0

Refer to the DPDK documentation for more details about huge pages and binding
devices, or use the DPDK helper located in `dpdk/usertools/setup.sh`.

Unlike most other DPDK applications, you have to pass DPDK EAL arguments
between `--dpdk` and `--`, then pass Click arguments. As the DPDK EAL will
handle thread management instead of Click, Click's `-j`/`--threads` argument
will be disabled when `--dpdk` is active. You should give at least the
following two EAL arguments for best practice. This is required with older
versions of DPDK, even if running on a single core:

* `-c COREMASK`: hexadecimal bitmask of cores to run on
* `-n NUM`: number of memory channels

If not provided, DPDK will use all available cores.

A sample command to run a click configuration on 4 cores on a computer with 4
memory channels and listen for control connections on TCP port 8080 would be:

    click --dpdk -c 0xf -n 4 -- -p 8080 configfile

If Click is launched without `--dpdk`, it will run in normal userlevel mode
without involving DPDK EAL, meaning that any DPDK element will not work.

### Configurations

Some sample configurations are included in the `conf` directory, including a
Perl script that generated the IP router configurations used in our TOCS paper
(`conf/make-ip-conf.pl`) and a set of patterns for the `click-xform` pattern
optimizer (`conf/ip.clickpat`).


Adding Your Own Elements
------------------------

Please see the FAQ in this directory to learn how to add elements to Click.


Copyright and License
---------------------

Most of Click is distributed under the Click license, a version of the MIT
License. See the `LICENSE` file for details. Each source file should identify
its license. Source files that do not identify a specific license are covered
by the Click license.

Parts of Click are distributed under different licenses. The specific licenses
are listed below.

* `drivers/e1000*`, `etc/linux-*-patch`, `linuxmodule/proclikefs.c`: These
  portions of the Click software are derived from the Linux kernel, and are
  thus distributed under the GNU General Public License, version 2. The GNU
  General Public License is available [via the Web](http://www.gnu.org/licenses/gpl.html) and
  in `etc/COPYING`.

* `include/click/bigint.hh`: This portion of the Click software derives from
  the GNU Multiple Precision Arithmetic Library, and is thus distributed under
  the GNU Lesser General Public License, version 3. This license is available
  [via the Web](http://www.gnu.org/licenses/lgpl.html) and in `etc/COPYING.lgpl`.

Element code that uses only Click’s interfaces will *not* be derived from the
Linux kernel. (For instance, those interfaces have multiple implementations,
including some that run at user level.) Thus, for element code that uses only
Click’s interfaces, the BSD-like Click license applies, not the GPL or the
LGPL.


Bugs, Questions, etc.
---------------------

We welcome bug reports, questions, comments, code, whatever you'd like to give
us. GitHub issues are the best way to stay in touch.

- The Click maintainers: [Eddie Kohler](http://www.read.seas.harvard.edu/~kohler/) and others

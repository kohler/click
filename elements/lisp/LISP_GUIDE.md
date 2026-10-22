# LISP in Click: user guide

This guide explains what LISP (the *Locator/ID Separation Protocol*) is,
how the LISP elements in `elements/lisp/` implement it, and how to build,
configure and observe them, including the *Follow Me Cloud* (FMCC)
extension for virtual-machine migration.

> **Status: experimental.** The elements implement the IPv4 parts of RFC 9300
> and RFC 9301 and decode correctly in Wireshark, and the regression tests in
> `test/lisp/` pass. They have not yet been tested against another LISP
> implementation over a real network.

---

## 1. LISP in five minutes

### The problem

An IP address is used for two things at once:

- **who** a host is (its identity, which TCP sessions and ACLs rely on);
- **where** it is attached (its location, which routing relies on).

When a host moves (a phone changes network, a VM migrates to another data
center), its location changes. So its address changes, and its sessions
break. Multi-homed sites also inject many small prefixes into the global
routing table.

### The idea

LISP gives each host **two** addresses:

| Name | Meaning | Visible to |
|---|---|---|
| **EID**: Endpoint Identifier | Identity of the host. Never changes when it moves. | Hosts, applications |
| **RLOC**: Routing Locator | Address of the LISP router the host sits behind. Changes when it moves. | The Internet core |

Hosts only use EIDs. At the edge of each site, a LISP router looks up
*EID -> RLOC* in a mapping system and tunnels the packet to the RLOC of the
destination site (*map-and-encap*).

```
   Site A (EIDs 10.0.1.0/24)                               Site B (EIDs 10.0.2.0/24)
 +-----------+      +-------+    Internet core     +-------+      +-----------+
 | host      |----->|  ITR  |=====================>|  ETR  |----->| host      |
 | 10.0.1.5  |      | RLOC  |  outer: RLOC->RLOC   | RLOC  |      | 10.0.2.7  |
 +-----------+      |192.0.2.1  inner: EID->EID    |192.0.2.2     +-----------+
                    +---+---+                      +---+---+
                        |  Map-Request        Map-Register |
                        v                                  v
                     +---------------------------------------+
                     |  Mapping system: Map-Resolver (MR)    |
                     |                  Map-Server   (MS)    |
                     |  10.0.2.0/24 -> 192.0.2.2             |
                     +---------------------------------------+
```

### Roles

| Role | Job |
|---|---|
| **ITR** (Ingress Tunnel Router) | Receives packets from local hosts, looks up the destination EID in its **map-cache**, and encapsulates the packet to the destination RLOC. On a miss, asks the mapping system. |
| **ETR** (Egress Tunnel Router) | Receives encapsulated packets, decapsulates them and delivers them to local hosts. Owns the authoritative mappings for its site and **registers** them with the MS. Answers Map-Requests for its EIDs. |
| **xTR** | A router that is both ITR and ETR (the usual case). |
| **MS** (Map-Server) | Stores the mappings registered by ETRs. |
| **MR** (Map-Resolver) | Receives Map-Requests from ITRs and forwards them to the right ETR (or answers for it). |
| **PITR / PETR** | Proxies between LISP sites and the non-LISP Internet (not implemented here). |

### Packets on the wire

LISP runs over UDP:

| UDP port | Used for |
|---|---|
| **4341** | Data plane: encapsulated user packets |
| **4342** | Control plane: Map-Request, Map-Reply, Map-Register, Map-Notify, … |

An encapsulated data packet looks like this (RFC 9300):

```
 +------------------+-----------+-------------------+------------------+---------+
 | outer IPv4       | UDP       | LISP header       | inner IPv4       | payload |
 | src = ITR RLOC   | dport 4341| 8 bytes           | src = EID A      |         |
 | dst = ETR RLOC   |           |                   | dst = EID B      |         |
 +------------------+-----------+-------------------+------------------+---------+

 LISP header:
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |N|L|E|V|I|R|K|K|            Nonce/Map-Version                  |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                 Instance ID/Locator-Status-Bits               |
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   N = nonce present     L = locator-status-bits valid
   E = echo-nonce request (only with N)   V = map-version present
   I = instance ID present (then 24-bit IID + 8 LSB bits)
```

Control messages (RFC 9301) all start with a 4-bit **type**:

| Type | Message | From -> To | Purpose |
|---|---|---|---|
| 1 | Map-Request | ITR -> MR (-> ETR) | "What is the RLOC for EID x?" |
| 1 + S bit | **SMR** (Solicit-Map-Request) | ETR -> ITR | "Your mapping for x is stale, ask again." |
| 2 | Map-Reply | ETR (or MS proxy) -> ITR | "x/len is at RLOC r, TTL t." |
| 3 | Map-Register | ETR -> MS | "I am authoritative for these prefixes." (authenticated, periodic) |
| 4 | Map-Notify | MS -> ETR | "Registration received." (when the M bit is set) |
| 5 | Map-Notify-Ack | ETR -> MS | Acknowledges a Map-Notify |
| 8 | Encapsulated Control Message | ITR -> MR | Carries Map-Requests to the MR |

### A first packet, step by step

```
host A        ITR              MR / MS             ETR             host B
  |  IP A->B   |                   |                 |                 |
  |----------->| map-cache miss    |                 |                 |
  |            | queue packet      |                 |                 |
  |            |-- Map-Request --->|                 |                 |
  |            |                   |-- forwarded --->|                 |
  |            |<---------------- Map-Reply (B/24 -> RLOC_B, TTL) -----|
  |            | fill cache, release queued packets  |                 |
  |            |===== LISP data (RLOC_A -> RLOC_B) ===>| decap          |
  |            |                   |                 |---- IP A->B --->|
```

Next packets to B hit the cache and are encapsulated immediately until the
mapping's TTL expires.

### Mobility and VM migration

When an EID moves from site 1 to site 2 (for example, a VM is migrated):

1. The ETR of site 2 adds the EID to its database and sends a
   **Map-Register** to the MS. The MS now maps the EID to RLOC₂.
2. The ETR of site 1 removes the EID and sends **SMRs** to every ITR that
   recently asked for it.
3. Those ITRs send a fresh Map-Request, get RLOC₂, and from then on tunnel
   directly to site 2. The EID, and therefore the VM's TCP sessions, never
   changed.

### Follow Me Cloud (FMCC): this work's extension

*Follow Me Cloud* (Taleb & Ksentini, 2013) keeps a mobile user's cloud
service close to the user by migrating the service's VM between data
centers as the user moves. This implementation adds an **FMCC
controller** to LISP:

1. The FMCC receives copies of Map-Registers, so it knows where every VM
   and mobile node is.
2. Its **FMCCManager** decides whether a VM should move (the design uses a Markov
   decision process; the code currently uses a fixed rule).
3. Its **FMCCMigrator** sends a **Map-Migrate** message (a new message type,
   not in the RFCs) to the xTR / hypervisor of the source data center.
4. The xTRs' **LISPMigrationUpdater** elements move the EID between their ETR databases,
   re-register with the MS and send SMRs, which is steps 1–3 of the
   mobility procedure above.

```
                       +-----------------------------+
     Map-Register copy |  FMCC                       |
   +------------------>|  FMCCDatabase (VM table)    |
   |                   |  FMCCManager  (decision)    |
   |                   |  FMCCMigrator (Map-Migrate) |----+
   |                   +-----------------------------+    | Map-Migrate (VM, new DC)
   |                                                      v
 +-+--------+   Map-Register   +-------+            +----------+
 | xTR DC1  |----------------->| MS/MR |            | xTR DC1  | LISPMigrationUpdater: remove VM, SMR ITRs
 +----------+                  +-------+            | xTR DC2  | LISPMigrationUpdater: add VM, Map-Register
                                                    +----------+
```

---

## 2. How the elements map to LISP

| LISP function | Element | Source files |
|---|---|---|
| Wire formats, constants | — | `include/clicknet/lisp.h` |
| Shared code: IP/UDP builder, prefix tables, HMAC | — | `lispcommon.{cc,hh}` |
| Encapsulation | `LISPEncap` | `lispencap.{cc,hh}` |
| Decapsulation | `LISPDecap` | `lispdecap.{cc,hh}` |
| Packet classification | `LISPClassifierxTR`, `LISPClassifierMS`, `LISPClassifierFMCC` | `lispclassifier.{cc,hh}` |
| ITR map-cache | `LISPMapCache` | `itrcache.{cc,hh}` |
| ITR: lookup, Map-Request, Map-Reply, SMR | `LISPMapRequester` | `rqstsender1.{cc,hh}` |
| ETR database | `LISPEtrDatabase` | `etrbase.{cc,hh}` |
| ETR: answer Map-Requests | `LISPMapReplier` | `rqstresolver.{cc,hh}` |
| ETR: Map-Register / Map-Notify | `LISPMapRegistrar` | `register.{cc,hh}` |
| ETR: apply a migration, send SMRs | `LISPMigrationUpdater` | `updater.{cc,hh}` |
| "Hypervisor": count / deliver to VMs | `LISPVMCounter` | `transmitter.{cc,hh}` |
| MS/MR database | `LISPMappingDB` | `mapbase.{cc,hh}` |
| Map-Server | `LISPMapServer` | `mapserver.{cc,hh}` |
| Map-Resolver | `LISPMapResolver` | `mapresolver.{cc,hh}` |
| FMCC database | `FMCCDatabase` | `fmccbase.{cc,hh}` |
| FMCC decision | `FMCCManager` | `manager.{cc,hh}` |
| FMCC Map-Migrate sender | `FMCCMigrator` | `migrator.{cc,hh}` |

"Database" elements (map-cache, ETR database, mapping database, FMCC
database) have no ports. Other elements receive them as a configuration
argument, the same way `ARPQuerier` or `IPRewriter` tables are shared in
Click.

### Inside an xTR

```
                         +--> [0] plain IP from local hosts -------> LISPMapRequester[0] --[0]--> LISPEncap --> to network
 FromDevice -> Classifier|                                                 |    \--[1]--> Map-Request (or ECM) --> to MR
   (Ethernet) -> Strip(14) -> CheckIPHeader -> LISPClassifierxTR           \---[2]--> negative mapping: forward natively
                         +--> [1] Map-Request  --------------------> LISPMapReplier (database) --> Map-Reply --> to ITR
                         +--> [2] Map-Reply    --------------------> LISPMapRequester[1]  (fills the map-cache, releases packets)
                         +--> [3] Map-Notify   --------------------> LISPMapRegistrar (database) --periodic--> Map-Register --> to MS
                         +--> [4] Map-Migrate  --------------------> LISPMigrationUpdater --> SMRs to the ITRs
                         +--> [5] LISP data    --------------------> LISPDecap --> LISPVMCounter / to VMs
                         +--> [6] errors       --------------------> Discard
                         +--> [7] SMR          --------------------> LISPMapRequester[2]  (refreshes the mapping)
```

---

## 3. Building

```sh
cd click
./configure --disable-linuxmodule --enable-lisp     # userlevel driver
make -j"$(nproc)"
make check                                          # includes test/lisp/
```

- `--enable-lisp` works because `configure` turns every directory in
  `elements/` into an element group. `--enable-all-elements` includes it too.
- Only the LISP tests: `make share/click/elementmap.xml`, then
  `test/clicktest -p bin -p test CLICKPATH="$PWD:" test/lisp`.
  `LISPWire-01` also needs `tshark`, and is skipped without it.
- Man pages for every element: `make install-man`, or
  `perl doc/click-elem2man -d DIR elements/lisp/*.hh`.
- The group also builds with clang, and with `--enable-user-multithread`.
  The Linux kernel module has not been tested with the current code.

---

## 4. Quick start: run the demo without a network

`conf/lisp/xtr1_xtr2_fmcc.click` puts **every role in one Click process**:
two xTRs (RLOCs `192.168.122.239` and `192.168.122.153`), a Map-Server and
the FMCC, wired element to element. At start-up the xTRs register, the FMCC
sees the registrations and migrates VM `192.168.122.161` from xTR1 to xTR2.

Replace the real interface with dummy elements and stop after 2.5 s:

```sh
cd click
sed -e 's/FromDevice(eth0, PROMISC true)/Idle/' \
    -e 's/ToDevice(eth0)/Discard/' \
    -e 's/^OUT :: Queue(10000) -> TD;/OUT :: Queue(10000) -> Unqueue -> TD;/' \
    conf/lisp/xtr1_xtr2_fmcc.click > /tmp/lisp-demo.click
echo 'DriverManager(wait 2.5s, stop);' >> /tmp/lisp-demo.click

userlevel/click /tmp/lisp-demo.click -h FMCCB.table -h MB.table -h EB1.base -h EB2.base -h migrator1.migrations
```

Expected output (abridged):

```
FMCCB.table:
3 . VM:192.168.122.161,  OLD_Destination :  192.168.122.239, ..., NEW_Destination : 192.168.122.153, Migrated : true,  size  8
MB.table:
192.168.122.161 mask 32 version  6  ttl  600  auth bit falseRLOC192.168.122.153
...
migrator1.migrations:
1
```

`EB1.base` no longer lists `192.168.122.161`; `EB2.base` does. The
`expensive Packet::push` lines are harmless: those packets were built with
little headroom.

---

## 5. Element reference

The full reference for each element is in its man page (see §3), generated
from the comments in the `.hh` files. Summary:

| Element | Arguments | Ports | Main handlers |
|---|---|---|---|
| `LISPEncap` | `PROTO, SRC, DST`; `NONCE`, `ECHO_NONCE`, `LSB`, `IID`, `UDP_CHECKSUM`, IP keywords | 1 / 1 | `count` |
| `LISPDecap` | `[SRC]` (ignored) | 1 / 1-2 (1: rejected) | `count`, `drops` |
| `LISPClassifierxTR` | `VERBOSE` | 1 / 8 (see §2) | `requests`, `registers` |
| `LISPClassifierMS` | `VERBOSE` | 1 / 4: other, Map-Request (plain or ECM), Map-Register, Map-Migrate | `requests`, `registers` |
| `LISPClassifierFMCC` | `VERBOSE` | 1 / 4: data, Map-Register, Map-Notify, other | `requests`, `registers` |
| `LISPMapCache` | `RQST_TIMEOUT` (jiffies), `CAPACITY`, `ENTRY_CAPACITY` | — | `cache`, `entries`, `queued`, `drops` |
| `LISPMapRequester` | `CACHE, PROTO, SRC, DST`; `MASK_LEN`, `AUTHORITATIVE`, `MAP_DATA`, `PITR`, `SMR_INVOKED`, `ECM` | 3 / 3: data, Map-Request, native | `stats`, `table`, `smrs`, `native_forwarded` |
| `LISPEtrDatabase` | `RLOC [, EID x4]`; `EID prefix` (repeatable), `TTL` (minutes) | — | `base`, `entries`; write `add`, `remove` |
| `LISPMapReplier` | `BASE, PROTO, SRC, DST` | 1 / 1 | `requests`, `replies` |
| `LISPMapRegistrar` | `BASE, PROTO, SRC, DST`; `KEY`, `INTERVAL`, `RETRY` (s) | 1 / 1 | `registers`, `notifies_valid`; write `register` |
| `LISPMigrationUpdater` | `RLOC, BASE, TRANSMITTER, REGISTER`; `KEY` | 1 / 1 | `vms_in`, `vms_out`, `smrs`, `auth_failures` |
| `LISPVMCounter` | `[VM x4]`; `VM` (repeatable), `USE_MAC` | 1 / 0-1 | `table` |
| `LISPMappingDB` | `REG_TIMEOUT` (s), `ENTRY_CAPACITY` | — | `table`, `entries`, `etrs`, `expired` |
| `LISPMapServer` | `MBASE, PROTO, SRC, DST`; `SITE "PREFIX KEY"` (repeatable) | 1 / 1 | `registers`, `notifies`, `auth_failures` |
| `LISPMapResolver` | `MBASE, PROTO, SRC, DST` | 1 / 1 | `requests`, `forwarded`, `negative`, `ecm` |
| `FMCCDatabase` | `ENTRY_CAPACITY` | — | `table`, `entries` |
| `FMCCManager` | `DC_SRC, DC_DST, VM, FMCCBASE, MIGRATOR`; `AUTO` | 1 / 0-1 | `migrations`; write `migrate` |
| `FMCCMigrator` | `FMCCBASE, PROTO, SRC, DST`; `KEY` | 1 / 1 | `migrations`, `notifies` |

Common to the senders: `PROTO, SRC, DST` describe the IP header of the
packets they send. `PROTO` is ignored (LISP is always UDP), `DST` may be
`DST_ANNO`, and `TTL`, `TOS`, `DSCP`, `ECT`, `CE`, `DF` work as in `IPEncap`.
Handlers also exist under their 2014 names (for example `m_rgst count`).

---

## 6. Writing a configuration

The configurations below parse and run with the current elements; their
packets have been checked with Wireshark. They have not yet been run against
other LISP routers on a real network.

### One xTR on one interface

RLOC `192.0.2.1`, site EIDs `10.0.1.0/24`, a Map-Server / Map-Resolver at
`192.0.2.10`, registrations signed with a shared key.

```
// ---------------- interface ----------------
fd  :: FromDevice(eth0);
out :: Queue(1000) -> ToDevice(eth0);
c   :: Classifier(12/0806 20/0001,   // ARP request
                  12/0806 20/0002,   // ARP reply
                  12/0800,           // IPv4
                  -);
arpr :: ARPResponder(192.0.2.1 eth0);
arpq :: ARPQuerier(192.0.2.1, eth0);
fd -> c;
c[0] -> arpr -> out;
c[1] -> [1]arpq -> out;
c[3] -> Discard;
arpq -> out;

// ---------------- LISP ----------------
lc  :: LISPClassifierxTR;
c[2] -> Strip(14) -> CheckIPHeader -> lc;

// ITR
ic  :: LISPMapCache(CAPACITY 1024);
itr :: LISPMapRequester(ic, UDP, 192.0.2.1, 192.0.2.10, MASK_LEN 32, ECM true);
enc :: LISPEncap(UDP, 192.0.2.1, DST_ANNO);
lc[0] -> [0]itr;
lc[2] -> [1]itr;
lc[7] -> [2]itr;
itr[0] -> enc -> arpq;          // data, encapsulated to the RLOC
itr[1] -> arpq;                 // Map-Requests (in ECMs) to the MR
itr[2] -> arpq;                 // destinations with a negative mapping: not encapsulated

// ETR
eb  :: LISPEtrDatabase(192.0.2.1, EID 10.0.1.0/24);
rr  :: LISPMapReplier(eb, UDP, 192.0.2.1, 192.0.2.10);
reg :: LISPMapRegistrar(eb, UDP, 192.0.2.1, 192.0.2.10, KEY "site-A-secret");
tr  :: LISPVMCounter(VM 10.0.1.10, VM 10.0.1.11);
upd :: LISPMigrationUpdater(192.0.2.1, eb, tr, reg);
lc[1] -> rr  -> arpq;           // Map-Replies
lc[3] -> reg -> arpq;           // Map-Notify in, Map-Register out
lc[4] -> upd -> arpq;           // Map-Migrate in, SMRs out
lc[5] -> LISPDecap -> tr -> Discard;   // replace Discard with the path to the hosts
lc[6] -> Discard;
```

### Map-Server / Map-Resolver node

```
fd  :: FromDevice(eth0);
out :: Queue(1000) -> ToDevice(eth0);
c   :: Classifier(12/0806 20/0001, 12/0806 20/0002, 12/0800, -);
arpr :: ARPResponder(192.0.2.10 eth0);
arpq :: ARPQuerier(192.0.2.10, eth0);
fd -> c;  c[0] -> arpr -> out;  c[1] -> [1]arpq -> out;  c[3] -> Discard;  arpq -> out;

mb  :: LISPMappingDB(REG_TIMEOUT 180);
ms  :: LISPMapServer(mb, UDP, 192.0.2.10, 192.0.2.10, SITE "10.0.1.0/24 site-A-secret");
mr  :: LISPMapResolver(mb, UDP, 192.0.2.10, 192.0.2.10);
lcm :: LISPClassifierMS;

c[2] -> Strip(14) -> CheckIPHeader -> lcm;
lcm[0] -> Discard;
lcm[1] -> mr -> arpq;           // Map-Requests forwarded to the ETR, or Negative Map-Replies
lcm[2] -> ms -> arpq;           // Map-Registers in, Map-Notify out
lcm[3] -> Discard;
```

An FMCC node is wired the same way with `LISPClassifierFMCC`: output 1
(Map-Register) goes to `FMCCManager`, output 2 (Map-Notify) goes to
`FMCCMigrator`, and `FMCCMigrator`'s output goes to `arpq`. Give
`FMCCMigrator` and the xTRs' `LISPMigrationUpdater` the same `KEY`.
`conf/lisp/xtr1_xtr2_fmcc.click` is the single-process version of all three
roles.

### Test topology

A small lab: four network namespaces or VMs on one bridge.

```
  ns "siteA" (10.0.1.0/24) -- xTR-A (192.0.2.1)
  ns "siteB" (10.0.2.0/24) -- xTR-B (192.0.2.2)       all RLOCs on 192.0.2.0/24
  MS/MR (192.0.2.10)
  FMCC  (192.0.2.20)
```

Run one `click` per node and watch the wire with
`tshark -i br0 -d udp.port==4342,lisp -d udp.port==4341,lisp-data`.

---

## 7. Observing a running router

- **At exit:** `click conf.click -h itr.stats -h ic.cache` prints the handlers
  when the router stops (pair it with `DriverManager(wait 10s, stop)`).
- **While running:** add `ControlSocket(TCP, 7777);` to the configuration,
  then:

  ```sh
  $ nc localhost 7777
  READ eb.base
  READ itr.stats
  WRITE mgr.migrate VM 10.0.2.7 FROM 192.0.2.2 TO 192.0.2.3
  ```

- **One line per classified packet:** `LISPClassifierxTR(VERBOSE true)`.
- **Packet traces:** `Print(label, MAXLENGTH 96)`, `IPPrint(label)`, or
  `ToDump(file.pcap, ENCAP IP)` and Wireshark.
- **Validity checks:** `CheckIPHeader(VERBOSE true) -> CheckUDPHeader(VERBOSE true)`.

---

## 8. Known limitations

- IPv4 only: no IPv6 EIDs or RLOCs.
- Not implemented: PITR/PETR, RLOC-probing, LISP-SEC, NAT traversal,
  map-versioning (the V bit is sent with versions 0), multicast.
- An ETR site has one RLOC (its own). The ITR and the Map-Server handle
  locator sets of up to 5 locators received from others.
- The FMCC decision is a fixed rule (`AUTO`) or an external order through
  the `migrate` handler. The Markov-decision policy of the report is not
  implemented.
- Map-Migrate is a local extension (RFC 8113 type 15, provisional
  sub-type), not a standard message.
- Not yet tested on the Linux kernel module, or against other LISP routers
  on a real network.

---

## 9. References

- RFC 9299: *An Architectural Introduction to the Locator/ID Separation Protocol (LISP)*, 2022.
- RFC 9300: *The Locator/ID Separation Protocol (LISP)* (data plane), 2022. Obsoletes RFC 6830.
- RFC 9301: *LISP Control Plane* (Map-Request/Reply/Register/Notify, authentication), 2022. Obsoletes RFC 6833.
- RFC 8113: *LISP Shared Extension Message & IANA Registry for Packet Type Allocations*, 2017.
- T. Taleb, A. Ksentini, *Follow Me Cloud: Interworking Federated Clouds and Distributed Mobile Networks*, IEEE Network, 2013.
- Open Overlay Router (OOR), an open-source LISP implementation useful for interoperability tests: <https://github.com/OpenOverlayRouter/oor>.
- Click documentation: `doc/click.5` (language), `doc/elementdoc.7` (element docs), `etc/samplepackage/` (out-of-tree packages).

# LISP elements for Click

The `lisp` element group implements the **Locator/ID Separation Protocol**
(LISP, RFC 9300 / RFC 9301, IPv4) as Click elements:

- **xTR** (ITR + ETR): encapsulation and decapsulation, the map-cache with
  longest-prefix match and locator sets, Map-Request (plain or in an
  Encapsulated Control Message), Map-Reply, Negative Map-Reply,
  Map-Register / Map-Notify with HMAC-SHA-256-128 authentication, and
  Solicit-Map-Request.
- **Map-Server / Map-Resolver**: the mapping database with site
  authorization and registration expiry, Map-Notify, and request forwarding.
- **FMCC** (*Follow Me Cloud* controller): an extension that migrates a
  virtual machine between data centers and updates the LISP mappings so its
  traffic follows it.

It was written by Franck Messaoudi in 2014 on Click 2.0.1 and updated to
the current Click tree and RFCs.

> **Status: experimental.** The elements build with gcc and clang, their
> packets decode correctly in Wireshark, and the tests in `test/lisp/` and
> Click's `make check` pass. Not supported yet: IPv6 EIDs and RLOCs,
> PITR/PETR, RLOC-probing, LISP-SEC, and the Linux kernel module (the
> elements are userlevel-tested only).

## Documents in this folder

| File | For | Content |
|---|---|---|
| `README.md` | everyone | This page: what the group is, where the files are, how to build and test |
| [LISP_GUIDE.md](LISP_GUIDE.md) | users | LISP explained, how the elements map to it, element reference, quick-start demo, example configurations |

---

## Where the files are

The group follows the layout of the other element groups: like
`elements/ipsec/` and `elements/wifi/`, it is off by default; its
wire-format header is in `include/clicknet/`, the example in `conf/lisp/`
and the tests in `test/lisp/`.

| Path | Content |
|---|---|
| `include/clicknet/lisp.h` | Wire formats and constants (plain C, byte-exact, network byte order) |
| `elements/lisp/lispcommon.{cc,hh}` | Shared code: IP/UDP header builder, prefix tables, locator choice, SHA-256 / HMAC, record helpers, locking |
| `elements/lisp/lispencap.*`, `lispdecap.*` | Data plane |
| `elements/lisp/lispclassifier.*` | `LISPClassifierxTR`, `LISPClassifierMS`, `LISPClassifierFMCC` |
| `elements/lisp/itrcache.*`, `rqstsender1.*` | ITR: `LISPMapCache`, `LISPMapRequester` |
| `elements/lisp/etrbase.*`, `rqstresolver.*`, `register.*`, `updater.*`, `transmitter.*` | ETR: `LISPEtrDatabase`, `LISPMapReplier`, `LISPMapRegistrar`, `LISPMigrationUpdater`, `LISPVMCounter` |
| `elements/lisp/mapbase.*`, `mapserver.*`, `mapresolver.*` | `LISPMappingDB`, `LISPMapServer`, `LISPMapResolver` |
| `elements/lisp/fmccbase.*`, `manager.*`, `migrator.*` | FMCC: `FMCCDatabase`, `FMCCManager`, `FMCCMigrator` |
| `conf/lisp/xtr1_xtr2_fmcc.click` | The 2014 demo: two xTRs, an MS and the FMCC in one process |
| `test/lisp/*.clicktest` | 11 regression tests, run by `make check` |

The elements and their source files are listed in
[LISP_GUIDE.md §2](LISP_GUIDE.md#2-how-the-elements-map-to-lisp).

### Build-system change

`configure.in` declares the group next to the other `ELEMENTS_ARG_ENABLE`
lines:

```m4
ELEMENTS_ARG_ENABLE(lisp, [include LISP (Locator/ID Separation Protocol) elements], NO)
```

`NO` keeps the group off by default; `--enable-lisp` or the CI's
`--enable-all-elements` turns it on. No `Makefile` changes are needed.

---

## Building and testing

```sh
cd click
./configure --disable-linuxmodule --enable-lisp
make -j"$(nproc)"
make check                      # all Click tests, including test/lisp/
```

Then follow the [quick start](LISP_GUIDE.md#4-quick-start-run-the-demo-without-a-network),
which runs two xTRs, an MS and the FMCC in one process and migrates a VM.

Tested on 2026-10-21 (Ubuntu 22.04): gcc 12.3 and clang 14
(`--enable-all-elements -std=gnu++11`), userlevel driver, single-threaded
and `--enable-user-multithread`. No compiler warnings in the LISP files;
11/11 LISP tests and 183/183 runnable `make check` tests pass.

---

## Documentation as code

All documentation of the code lives in the source and is generated from it:

| What | Where it is written | How to generate it |
|---|---|---|
| Element reference (arguments, ports, handlers) | `=c` / `=s` / `=d` / `=h` comment above each element class | `make install-man`, or `perl doc/click-elem2man -d DIR elements/lisp/*.hh` (one man page per element) |
| API reference (wire formats, shared helpers, internal classes) | Doxygen comments (`/** */`, `///<`) in `include/clicknet/lisp.h` and `elements/lisp/*.hh` | `doxygen elements/lisp/Doxyfile` from the top of the tree, then open `doc/lisp-api/html/index.html` |
| User guide | [LISP_GUIDE.md](LISP_GUIDE.md) | included as a page in the Doxygen output |

Both generators run without warnings. When you change an element's
arguments, ports or handlers, update its `=c` comment in the same commit.

## Coding style

The code follows Click's own conventions: 4-space indentation with 8-column
tabs, the return type of a function definition on its own line, private
members `_snake_case`, plain struct fields lowercase, classes `CamelCase`,
enum constants `UPPER_CASE`, wire-format structs `click_lisp_*`. The
[`.clang-format`](.clang-format) file in this folder reproduces it:

```sh
clang-format -i --style=file:elements/lisp/.clang-format elements/lisp/*.cc elements/lisp/*.hh include/clicknet/lisp.h
```

Keep `class_name()` on one line: `click-buildtool` reads it with a regular
expression and rejects the element otherwise.

## License

Every file carries a copyright notice (Copyright (c) 2014-2026 Franck
Messaoudi) and the summary of the Click license, as the other Click files
do. The Click license, in the top-level `LICENSE` file, is an MIT license
with one extra clause: the copyright holders' names may not be used in
advertising without their permission. Anyone may use, copy, modify,
merge, publish, distribute, sublicense and sell the code, provided the
notice is kept.

---

## Authors

- Franck Messaoudi: original implementation (2014) and maintenance.

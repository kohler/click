// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * netmapinfo.{cc,hh} -- library for interfacing with netmap
 * Eddie Kohler, Luigi Rizzo
 *
 * Copyright (c) 2012 Eddie Kohler
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/glue.hh>
#include "netmapinfo.hh"
#if HAVE_NET_NETMAP_H
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <click/sync.hh>
#include <unistd.h>
#include <fcntl.h>
CLICK_DECLS

static Spinlock netmap_memory_lock;
static void *netmap_memory = MAP_FAILED;
static size_t netmap_memory_size;
static uint32_t netmap_memory_users;

unsigned char *NetmapInfo::buffers;

int
NetmapInfo::ring::open(const String &ifname,
		       bool always_error, ErrorHandler *errh)
{
    ErrorHandler *initial_errh = always_error ? errh : ErrorHandler::silent_handler();

    int fd = ::open("/dev/netmap", O_RDWR);
    if (fd < 0) {
	initial_errh->error("/dev/netmap: %s", strerror(errno));
	return -1;
    }

    struct nmreq req;
    memset(&req, 0, sizeof(req));
    strncpy(req.nr_name, ifname.c_str(), sizeof(req.nr_name));
    req.nr_ringid = 0;
#if NETMAP_API
    req.nr_version = NETMAP_API;
#endif
    int r;
    if ((r = ioctl(fd, NIOCGINFO, &req))) {
	initial_errh->error("netmap %s: %s", ifname.c_str(), strerror(errno));
    error:
	close(fd);
	return -1;
    }
    size_t memsize = req.nr_memsize;

    if ((r = ioctl(fd, NIOCREGIF, &req))) {
	errh->error("netmap register %s: %s", ifname.c_str(), strerror(errno));
	goto error;
    }

    netmap_memory_lock.acquire();
    if (netmap_memory == MAP_FAILED) {
	netmap_memory_size = memsize;
	netmap_memory = mmap(0, netmap_memory_size, PROT_WRITE | PROT_READ,
			     MAP_SHARED, fd, 0);
	if (netmap_memory == MAP_FAILED) {
	    errh->error("netmap allocate %s: %s", ifname.c_str(), strerror(errno));
	    netmap_memory_lock.release();
	    goto error;
	}
    }
    mem = (char *) netmap_memory;
    ++netmap_memory_users;
    netmap_memory_lock.release();

    nifp = NETMAP_IF(mem, req.nr_offset);
    ring_begin = 0;
    ring_end = req.nr_numrings;

    // XXX timestamp off
    for (unsigned i = ring_begin; i != ring_end; ++i)
	NETMAP_RXRING(nifp, i)->flags = NR_TIMESTAMP;
    return fd;
}

void
NetmapInfo::ring::close(int fd)
{
    netmap_memory_lock.acquire();
    if (--netmap_memory_users <= 0 && netmap_memory != MAP_FAILED) {
	munmap(netmap_memory, netmap_memory_size);
	netmap_memory = MAP_FAILED;
    }
    netmap_memory_lock.release();
    ioctl(fd, NIOCUNREGIF, (struct nmreq *) 0);
    ::close(fd);
}

CLICK_ENDDECLS
#endif
ELEMENT_PROVIDES(NetmapInfo)

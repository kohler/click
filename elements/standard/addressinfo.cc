// -*- c-basic-offset: 4; related-file-name: "../../include/click/standard/addressinfo.hh" -*-
/*
 * addressinfo.{cc,hh} -- element stores address information
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
 * Copyright (c) 2004 The Regents of the University of California
 * Copyright (c) 2011 Meraki, Inc.
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
#include <click/standard/addressinfo.hh>
#include <click/nameinfo.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <clicknet/ether.h>
#include <click/etheraddress.hh>
#if CLICK_NS
# include <click/master.hh>
#endif
#if CLICK_USERLEVEL
# include <unistd.h>
# include <time.h>
# if HAVE_IFADDRS_H
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <net/if.h>
#  if HAVE_NET_IF_TYPES_H
#   include <net/if_types.h>
#  endif
#  if HAVE_NET_IF_DL_H
#   include <net/if_dl.h>
#  endif
#  if HAVE_NETPACKET_PACKET_H
#   include <netpacket/packet.h>
#  endif
#  include <ifaddrs.h>
# elif defined(__linux__)
#  include <net/if.h>
#  include <sys/ioctl.h>
#  include <net/if_arp.h>
#  include <click/userutils.hh>
# endif
#elif CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <linux/netdevice.h>
# include <linux/rtnetlink.h>
# include <linux/if_arp.h>
# include <linux/inetdevice.h>
# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
#  include <net/net_namespace.h>
# endif
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
#if CLICK_BSDMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <sys/socket.h>
# include <net/if.h>
# include <net/if_dl.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
CLICK_DECLS

AddressInfo::AddressInfo()
{
}

AddressInfo::~AddressInfo()
{
}

int
AddressInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
    enum { t_eth = 1, t_ip4 = 2, t_ip4net = 4, t_ip6 = 8, t_ip6net = 16 };
    int before = errh->nerrors();

    for (int i = 0; i < conf.size(); i++) {
	Vector<String> parts;
	cp_spacevec(conf[i], parts);
	if (parts.size() == 0)
	    // allow empty arguments
	    continue;
	if (parts.size() < 2)
	    errh->error("expected %<NAME [ADDRS]%>");
	int types = 0;

	for (int j = 1; j < parts.size(); j++) {
	    struct in_addr ip4[2];
	    unsigned char ether[6];
# if HAVE_IP6
	    struct {
		unsigned char c[sizeof(click_in6_addr)];
		int p;
	    } ip6;
# endif

	    int my_types = 0;
	    if (EtherAddressArg().parse(parts[j], ether))
		my_types |= t_eth;
	    if (IPAddressArg().parse(parts[j], ip4[0]))
		my_types |= t_ip4;
	    else if (IPPrefixArg().parse(parts[j], ip4[0], ip4[1])) {
		my_types |= t_ip4net;
		if (ip4[0].s_addr & ~ip4[1].s_addr)
		    my_types |= t_ip4;
	    }
#if HAVE_IP6
	    if (cp_ip6_address(parts[j], ip6.c))
		my_types |= t_ip6;
	    else if (cp_ip6_prefix(parts[j], ip6.c, &ip6.p, false)) {
		my_types |= t_ip6net;
		if (IP6Address(ip6.c) & IP6Address::make_inverted_prefix(ip6.p))
		    my_types |= t_ip6;
	    }
#endif

	    bool one_type = (my_types & (my_types - 1)) == 0;
	    if ((my_types & t_eth) && (one_type || !(types & t_eth)))
		NameInfo::define(NameInfo::T_ETHERNET_ADDR, this, parts[0], ether, 6);
	    if ((my_types & t_ip4) && (one_type || !(types & t_ip4)))
		NameInfo::define(NameInfo::T_IP_ADDR, this, parts[0], &ip4[0], 4);
	    if ((my_types & t_ip4net) && (one_type || !(types & t_ip4net)))
		NameInfo::define(NameInfo::T_IP_PREFIX, this, parts[0], &ip4[0], 8);
#if HAVE_IP6
	    if ((my_types & t_ip6) && (one_type || !(types & t_ip6)))
		NameInfo::define(NameInfo::T_IP6_ADDR, this, parts[0], ip6.c, 16);
	    if ((my_types & t_ip6net) && (one_type || !(types & t_ip6net)))
		NameInfo::define(NameInfo::T_IP6_PREFIX, this, parts[0], &ip6, 16 + sizeof(int));
#endif

	    types |= my_types;
	    if (!my_types)
		errh->error("\"%s\" %<%s%> is not a recognizable address", parts[0].c_str(), parts[j].c_str());
	}
    }

    return (errh->nerrors() == before ? 0 : -1);
}


#if CLICK_USERLEVEL && !CLICK_NS && (HAVE_IFADDRS_H || defined(__linux__))
static void
create_deviceinfo(Vector<String> &deviceinfo)
{
# if HAVE_IFADDRS_H
    // Read network device information from getifaddrs().
    struct ifaddrs *ifap;
    if (getifaddrs(&ifap) >= 0) {
	for (struct ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
	    if (!ifa->ifa_addr)
		continue;

	    if (ifa->ifa_addr->sa_family == AF_INET) {
		struct sockaddr_in *sin = (struct sockaddr_in *) ifa->ifa_addr;
		deviceinfo.push_back(ifa->ifa_name);
		deviceinfo.push_back(String('i') + String((char *) &sin->sin_addr, sizeof(struct in_addr)));
		if (ifa->ifa_netmask) {
		    struct sockaddr_in *sinm = (struct sockaddr_in *) ifa->ifa_netmask;
		    deviceinfo.push_back(ifa->ifa_name);
		    deviceinfo.push_back(String('I') + String((char *) &sin->sin_addr, sizeof(struct in_addr)) + String((char *) &sinm->sin_addr, sizeof(struct in_addr)));
		}
	    }

#  if defined(AF_PACKET) && HAVE_NETPACKET_PACKET_H
	    if (ifa->ifa_addr->sa_family == AF_PACKET) {
		struct sockaddr_ll *sll = (struct sockaddr_ll *) ifa->ifa_addr;
		if ((sll->sll_hatype == ARPHRD_ETHER || sll->sll_hatype == ARPHRD_80211) && sll->sll_halen == sizeof(EtherAddress)) {
		    deviceinfo.push_back(ifa->ifa_name);
		    deviceinfo.push_back(String('e') + String((char *) sll->sll_addr, sizeof(EtherAddress)));
		}
	    }
#  endif

#  if defined(AF_LINK) && HAVE_NET_IF_DL_H
	    if (ifa->ifa_addr->sa_family == AF_LINK) {
		struct sockaddr_dl *sdl = (struct sockaddr_dl *) ifa->ifa_addr;
		if (sdl->sdl_type == IFT_ETHER && sdl->sdl_alen == sizeof(EtherAddress)) {
		    deviceinfo.push_back(ifa->ifa_name);
		    deviceinfo.push_back(String('e') + String((char *) LLADDR(sdl), sizeof(EtherAddress)));
		}
	    }
#  endif
	}

	freeifaddrs(ifap);
    }

# elif defined(__linux__)
    // Read network device information from /proc/net/dev and ioctls.
    int query_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (query_fd < 0)
	return;

    String f = file_string("/proc/net/dev");
    const char *begin = f.begin(), *end = f.end(), *nl;
    struct ifreq ifr;
    for (; begin < end; begin = nl + 1) {
	const char *colon = find(begin, end, ':');
	nl = find(begin, end, '\n');
	if (colon <= begin || colon >= nl)
	    continue;

	const char *word = colon;
	while (word > begin && !isspace((unsigned char) word[-1]))
	    --word;
	if ((size_t) (colon - word) >= sizeof(ifr.ifr_name))
	    continue;

	// based on patch from Jose Vasconcellos <jvasco@bellatlantic.net>
	String dev_name = f.substring(word, colon);
	strcpy(ifr.ifr_name, dev_name.c_str());
	if (ioctl(query_fd, SIOCGIFHWADDR, &ifr) >= 0
	    && (ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER
		|| ifr.ifr_hwaddr.sa_family == ARPHRD_80211)) {
	    deviceinfo.push_back(dev_name);
	    deviceinfo.push_back(String('e') + String(ifr.ifr_hwaddr.sa_data, sizeof(EtherAddress)));
	}
	char x[8];
	if (ioctl(query_fd, SIOCGIFADDR, &ifr) >= 0
	    && ifr.ifr_addr.sa_family == AF_INET) {
	    struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;
	    deviceinfo.push_back(dev_name);
	    memcpy(x, &sin->sin_addr, sizeof(struct in_addr));
	    deviceinfo.push_back(String('i') + String(x, sizeof(struct in_addr)));
	    if (ioctl(query_fd, SIOCGIFNETMASK, &ifr) >= 0
		&& ifr.ifr_addr.sa_family == AF_INET) {
		deviceinfo.push_back(dev_name);
		memcpy(x + sizeof(struct in_addr), &sin->sin_addr, sizeof(struct in_addr));
		deviceinfo.push_back(String('I') + String(x, 2 * sizeof(struct in_addr)));
	    }
	}
    }

    close(query_fd);
# endif
}
#endif

bool
AddressInfo::query_netdevice(const String &s, unsigned char *store,
			     int type, int len, const Element *context)
    // type: should be 'e' (Ethernet), 'i' (ipv4), 'I' (ipv4 prefix)
{
    (void) s, (void) store, (void) type, (void) len, (void) context;

#if CLICK_USERLEVEL && !CLICK_NS && (HAVE_IFADDRS_H || defined(__linux__))

    // 5 Mar 2004 - Don't call ioctl for every attempt to look up an Ethernet
    // device name, because this causes the kernel to try to load weird kernel
    // modules.
    static time_t read_time = 0;
    static Vector<String> deviceinfo;

    // XXX magic time constant
    if (!read_time || read_time + 30 < time(0)) {
	deviceinfo.clear();
	create_deviceinfo(deviceinfo);
	read_time = time(0);
    }

# if 0 /* debugging */
    for (int i = 0; i < deviceinfo.size(); i += 2) {
	fprintf(stderr, "%s %c:", deviceinfo[i].c_str(), deviceinfo[i+1][0]);
	for (const char *x = deviceinfo[i+1].begin() + 1; x != deviceinfo[i+1].end(); ++x)
	    fprintf(stderr, "%02x", (unsigned char) *x);
	fprintf(stderr, "\n");
    }
# endif

    for (int i = 0; i < deviceinfo.size(); i += 2)
	if (deviceinfo[i] == s && deviceinfo[i+1][0] == type) {
	    memcpy(store, deviceinfo[i+1].data() + 1, len);
	    return true;
	}

#elif CLICK_LINUXMODULE

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24)
    net_device *dev = dev_get_by_name(&init_net, s.c_str());
# else
    net_device *dev = dev_get_by_name(s.c_str());
# endif
    bool found = false;
    if (dev && type == 'e'
	&& (dev->type == ARPHRD_ETHER || dev->type == ARPHRD_80211)) {
	memcpy(store, dev->dev_addr, 6);
	found = true;
    } else if (dev && (type == 'i' || type == 'I')) {
	if (in_device *in_dev = in_dev_get(dev)) {
	    for_primary_ifa(in_dev) {
		memcpy(store, &ifa->ifa_local, 4);
		if (type == 'I')
		    memcpy(store + 4, &ifa->ifa_mask, 4);
		found = true;
		break;
	    }
	    endfor_ifa(in_dev);
	    in_dev_put(in_dev);
	}
    }
    dev_put(dev);
    return found;

#elif CLICK_BSDMODULE
    struct ifnet *ifp;
    bool found = false;
    ifp = ifunit(s.c_str());
    if (ifp && type == 'e') {
	struct sockaddr_dl *sdl;
	sdl = (struct sockaddr_dl *)ifp->if_addr->ifa_addr;
	memcpy(store, LLADDR(sdl), 6 /*sdl->sdl_alen*/);
	found = true;
    } else if (ifp && (type == 'i' || type == 'I')) {
	struct ifaddr *ifa;
	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
	    struct sockaddr_in *sin;
	    sin = satosin(ifa->ifa_addr);
	    // use first IP address
	    if (sin != NULL && sin->sin_family == AF_INET) {
		memcpy(store, (uint8_t *)(&(sin->sin_addr)),
		       4 /*sizeof(sin->sin_addr)*/);
		if (type == 'I') {
		    sin = satosin(ifa->ifa_netmask);
		    memcpy(store + 4, (uint8_t *)(&(sin->sin_addr)), 4);
		}
		found = true;
		break;
	    }
	}
	IF_ADDR_UNLOCK(ifp);
    }
    return found;

#elif CLICK_NS

    if (context && type == 'i') {
	char tmp[255];
	int r = simclick_sim_command(e->router()->master()->simnode(), SIMCLICK_IPADDR_FROM_NAME, s.c_str(), tmp, 255);
	if (r >= 0 && tmp[0] && IPAddressArg().parse(tmp, *reinterpret_cast<IPAddress *>(store)))
	    return true;
    } else if (context && type == 'e') {
	char tmp[255];
	int r = simclick_sim_command(e->router()->master()->simnode(), SIMCLICK_MACADDR_FROM_NAME, s.c_str(), tmp, 255);
	if (r >= 0 && tmp[0] && EtherAddressArg().parse(tmp, store))
	    return true;
    }

#endif

    return false;
}


bool
AddressInfo::query_ip(String s, unsigned char *store, const Element *context)
{
    int colon = s.find_right(':');
    if (colon >= 0) {
	String typestr = s.substring(colon).lower();
	s = s.substring(0, colon);
	union {
	    uint32_t addr[2];
	    unsigned char x[8];
	} u;
	if (typestr.equals(":ip", 3) || typestr.equals(":ip4", 4))
	    /* do nothing */;
	else if (typestr.equals(":bcast", 6)) {
	    if (query_ip_prefix(s, &u.x[0], &u.x[4], context)) {
		u.addr[0] |= ~u.addr[1];
		memcpy(store, u.x, 4);
		return true;
	    } else
		return false;
	} else if (typestr.equals(":gw", 3)) {
	    if (query_ip_prefix(s, &u.x[0], &u.x[4], context)) {
		u.addr[0] = (u.addr[0] & u.addr[1]) | htonl(1);
		memcpy(store, u.x, 4);
		return true;
	    } else
		return false;
	} else
	    return false;
    }

    return NameInfo::query(NameInfo::T_IP_ADDR, context, s, store, 4)
	|| query_netdevice(s, store, 'i', 4, context);
}

bool
AddressInfo::query_ip_prefix(String s, unsigned char *store,
			     unsigned char *mask_store, const Element *context)
{
    int colon = s.find_right(':');
    if (colon >= 0) {
	String typestr = s.substring(colon).lower();
	s = s.substring(0, colon);
	if (!(typestr.equals(":net", 4) || typestr.equals(":ipnet", 6)
	      || typestr.equals(":ip4net", 7)))
	    return false;
    }

    uint8_t data[8];
    if (NameInfo::query(NameInfo::T_IP_PREFIX, context, s, &data[0], 8)
	|| query_netdevice(s, data, 'I', 8, context)) {
	memcpy(store, &data[0], 4);
	memcpy(mask_store, &data[4], 4);
	return true;
    } else
	return false;
}


#ifdef HAVE_IP6

bool
AddressInfo::query_ip6(String s, unsigned char *store, const Element *e)
{
    int colon = s.find_right(':');
    if (colon >= 0 && s.substring(colon).lower() != ":ip6")
	return false;
    else if (colon >= 0)
	s = s.substring(0, colon);

    return NameInfo::query(NameInfo::T_IP6_ADDR, e, s, store, 16);
}

bool
AddressInfo::query_ip6_prefix(String s, unsigned char *store,
			      int *bits_store, const Element *context)
{
    int colon = s.find_right(':');
    if (colon >= 0 && s.substring(colon).lower() != ":ip6net")
	return false;
    else if (colon >= 0)
	s = s.substring(0, colon);

    struct {
	unsigned char c[16];
	int p;
    } data;
    if (NameInfo::query(NameInfo::T_IP6_PREFIX, context, s, &data, sizeof(data))) {
	memcpy(store, data.c, 16);
	*bits_store = data.p;
	return true;
    }

    return false;
}

#endif /* HAVE_IP6 */


bool
AddressInfo::query_ethernet(String s, unsigned char *store, const Element *context)
{
    int colon = s.find_right(':');
    if (colon >= 0 && s.substring(colon).lower() != ":eth"
	&& s.substring(colon).lower() != ":ethernet")
	return false;
    else if (colon >= 0)
	s = s.substring(0, colon);

    return NameInfo::query(NameInfo::T_ETHERNET_ADDR, context, s, store, 6)
	|| query_netdevice(s, store, 'e', 6, context);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AddressInfo)
ELEMENT_HEADER(<click/standard/addressinfo.hh>)

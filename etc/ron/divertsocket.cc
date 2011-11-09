/*
 * fromdevice.{cc,hh} -- element diverts IP packets into Click using divert sockets
 * Alexander Yip
 *
 * Copyright (c) 2001 Massachusetts Institute of Technology
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
#include "divertsocket.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/glue.hh>
#include <unistd.h>
#include <fcntl.h>

# include <net/if.h>

#if defined(__FreeBSD__ )
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <sys/types.h>

#elif defined(__linux__)
# include <features.h>
# include <net/if_packet.h>
# include <netinet/ip.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <netinet/udp.h>
# include <sys/param.h>

# include <linux/types.h>
# include <linux/icmp.h>

# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
#  include <net/ethernet.h>
# else
#  include <linux/if_packet.h>
#  include <linux/if_ether.h>
# endif
#endif

#include <clicknet/ip.h>

DivertSocket::DivertSocket()
{
  _fd = -1;
}


DivertSocket::~DivertSocket()
{
  uninitialize();
}

int DivertSocket::parse_ports(const String &param, ErrorHandler *errh,
			      int32_t *portl, int32_t *porth) {
  int dash;
  *portl =  *porth = 0;;
  dash = param.find_left('-');

  if (dash < 0)
    dash = param.length();

  if (!cp_integer(param.substring(0,dash), portl)){
    //errh->error("1 bad port in rule spec");
    return -1;
  }

  if (dash < param.length()) {
    if (!cp_integer(param.substring(dash+1), porth)) {
      //errh->error("2 bad port in rule spec");
      return -1;
    }
  } else
    *porth = *portl;

  if (*portl > *porth || *portl < 0 || *porth > 0xFFFF) {
    errh->error("port(s) %d-%d out of range in rule spec", portl, porth);
    return -1;
  }
  return 0;
}

int
DivertSocket::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  int confindex = 5;
  _have_sport = _have_dport = false;
  _setup_fw = true;
#if 0
  for(int i=0; i < conf.size(); i++){
    click_chatter("  %s\n", ((String)conf[i]).c_str());
  }
#endif

  if (conf.size() == 1) {
    if (cp_va_parse(conf[0], this, errh, cpUnsigned, "divertport",
		    &_divertport, cpEnd) < 0)
      return -1;
    _setup_fw = false;
    return 0;
  }


  if (conf.size() < confindex+1) {
    errh->error("not enough parameters for DivertSocket");
    return -1;
  }
  if (conf.size() > confindex+4) {
    errh->error("too many parameters for DivertSocket");
    return -1;
  }
  /*
  click_chatter("Hello 1 this = %x\n", (void *)this);
  click_chatter("Hello 2 this = %x\n", (void *)this);
  click_chatter("conf[0] = %s\n", conf[0].c_str());
  */

  // parse devicename
  if (cp_va_parse(conf[0], this, errh, cpString, "device", &_device, cpEnd) < 0)
    return -1;
  // parse divert port number
  if (cp_va_parse(conf[1], this, errh, cpUnsigned, "divertport", &_divertport, cpEnd) < 0)
    return -1;
  // parse rule number
  if (cp_va_parse(conf[2], this, errh, cpUnsigned, "rulenumber", &_rulenumber, cpEnd) < 0)
    return -1;


  // parse protocol & src addr/mask
  if ((cp_va_parse(conf[3], this, errh, cpByte, "protocol", &_protocol, cpEnd) < 0) ||
      (!cp_ip_prefix(conf[4], &_saddr, &_smask, true, this))) {
    errh->error("invalid src addr/mask");
    return -1;
  }
  _saddr &= _smask;


  if ((_protocol != IP_PROTO_UDP && _protocol != IP_PROTO_TCP) && (conf.size() > 7)) {
    errh->error("too many parameters for non TCP/UDP rule");
    return -1;
  }

  // parse src ports
  if (_protocol == IP_PROTO_UDP || _protocol == IP_PROTO_TCP) {
    if (parse_ports(conf[5], errh, &_sportl, &_sporth) < 0)
      _have_sport = false;
    else {
      _have_sport = true;
      confindex++;
    }
  } else if (parse_ports(conf[5], errh, &_sportl, &_sporth) >= 0) {
    errh->error("ports not required for non TCP/UDP rules");
    return -1;
  }

  //printf("1 confindex = %d (%s)\n", confindex, conf[confindex].c_str());
  // parse dst addr/mask
  if (!cp_ip_prefix(conf[confindex], &_daddr, &_dmask, true, this )){
    errh->error("invalid dst addr/mask");
    return -1;
  }


  _daddr &= _dmask;
  confindex++;


  // parse dst ports
  if (confindex < conf.size()) {
    //printf("2 confindex = %d (%s)\n", confindex, conf[confindex].c_str());
    if (_protocol == IP_PROTO_UDP || _protocol == IP_PROTO_TCP) {
      if (parse_ports(conf[confindex], errh, &_dportl, &_dporth) < 0)
	_have_dport = false;
      else {
	_have_dport = true;
	confindex++;
      }
    } else if (parse_ports(conf[confindex], errh, &_dportl, &_dporth) >= 0) {
      errh->error("ports not required for non TCP/UDP rules");
      return -1;
    }
  }

  // parse in/out
  if (confindex < conf.size() ) {
    if (cp_va_parse(conf[confindex], this, errh, cpString, "in/out", &_inout, cpEnd) < 0)
      return -1;
    if ( (_inout != "") && (_inout != "in") && (_inout != "out") ) {
      errh->error("illegal direction specifier: '%s'", _inout.c_str());
      return -1;
    }
  }


  return 0;
}

int
DivertSocket::setup_firewall(ErrorHandler *errh) {

#if defined(__FreeBSD__)
  char tmp[512];
  char sport[32], dport[32], prot[8];
#elif defined(__linux__)
  char *fw_policy="DIVERT";
  char fw_chain[32];
#endif


  // Setup firewall to divert sockets
#if defined(__FreeBSD__)
  if (_protocol == 0)
    sprintf(prot, "ip");
  else
    sprintf(prot, "%d", _protocol);

  if (_have_sport) {
    if (_sportl == _sporth)
      sprintf(sport, "%d", _sportl);
    else
      sprintf(sport, "%d-%d", _sportl, _sporth);
  } else {
    sport[0]=0;
  }

  if (_have_dport) {
    if (_dportl == _dporth)
      sprintf(dport, "%d", _dportl);
    else
      sprintf(dport, "%d-%d", _dportl, _dporth);
  } else {
    dport[0]=0;
  }

  sprintf(tmp, "/sbin/ipfw add %u divert %u %s from %s:%s %s to %s:%s %s %s via %s",
	  _rulenumber, _divertport,
	  prot, _saddr.s().c_str(), _smask.s().c_str(), sport,
	  _daddr.s().c_str(), _dmask.s().c_str(), dport, _inout.c_str(), _device.c_str() );
  printf("%s\n", tmp);

  if (system(tmp) != 0) {
    close (_fd);
    errh->error("ipfw failed");
    return -1;
  }
#elif defined(__linux__)

  /* fill in the rule first */
  bzero(&fw, sizeof (struct ip_fw));
  fw.fw_proto= _protocol;
  fw.fw_redirpt=htons(bindPort.sin_port);
  //fw.fw_redirpt=bindPort.sin_port;

  if (_have_sport) {
    fw.fw_spts[0]=_sportl;
    fw.fw_spts[1]=_sporth;
  } else {
    fw.fw_spts[0]=0;
    fw.fw_spts[1]=0xffff;
  }
  if (_have_dport) {
    fw.fw_dpts[0]=_dportl;
    fw.fw_dpts[1]=_dporth;
  } else {
    fw.fw_dpts[0]=0;
    fw.fw_dpts[1]=0xffff;
  }

  fw.fw_src.s_addr = _saddr.in_addr().s_addr;
  fw.fw_smsk.s_addr= _smask.in_addr().s_addr;
  fw.fw_dst.s_addr = _daddr.in_addr().s_addr;
  fw.fw_dmsk.s_addr= _dmask.in_addr().s_addr;

  fw.fw_outputsize=0xffff;
  strcpy(fw.fw_vianame, _device.c_str() );

  /* fill in the fwuser structure */
  ipfu.ipfw=fw;
  memcpy(ipfu.label, fw_policy, strlen(fw_policy));


  /* fill in the fwnew structure */
  ipfc.fwn_rule=ipfu;
  ipfc.fwn_rulenum = _rulenumber;


  /* open a socket */
  if ((fw_sock=socket(AF_INET, SOCK_RAW, IPPROTO_RAW))==-1) {
    errh->error("could not create raw socket for firewall setup");
    return -1;
  }


  if (_inout == "in") {
    strcpy(fw_chain, "input");
    memcpy(ipfc.fwn_label, fw_chain, strlen(fw_chain));

  } else if (_inout == "out") {
    strcpy(fw_chain, "output");
    memcpy(ipfc.fwn_label, fw_chain, strlen(fw_chain));

  } else {
    memcpy(&ipfc2, &ipfc, sizeof(ipfc));
    strcpy(fw_chain, "input");
    memcpy(ipfc.fwn_label, fw_chain, strlen(fw_chain));
    strcpy(fw_chain, "output");
    memcpy(ipfc2.fwn_label, fw_chain, strlen(fw_chain));

    /* write a rule into it */
    if (setsockopt(fw_sock, IPPROTO_IP, IP_FW_INSERT, &ipfc2, sizeof(ipfc2))==-1) {
      errh->error("could not set output firewall rule: %s", strerror(errno));
      return -1;
    }
  }

  /* write a rule into it */
  if (setsockopt(fw_sock, IPPROTO_IP, IP_FW_INSERT, &ipfc, sizeof(ipfc))==-1) {
    errh->error("could not set firewall rule: %s",strerror(errno));
    return -1;
  }

#else
  close(_fd);
  errh->error("This platform is not yet supported by DivertSocket");
  return -1;


#endif
  return 0;
}

int
DivertSocket::initialize(ErrorHandler *errh)
{
  int ret, n;
  struct sockaddr_in bindPort; //, sin;



#if 0
  printf("Device  : \t%s\n", _device.c_str());
  printf("DIV port: \t%u\n", _divertport);
  printf("Rule Num: \t%u\n", _rulenumber);
  printf("Protocol: \t%d\n", _protocol);
  printf("src/mask: \t%s / %s\n", _saddr.s().c_str(), _smask.s().c_str());
  printf("dst/mask: \t%s / %s\n", _daddr.s().c_str(), _dmask.s().c_str());
  printf("sport   : \t%u - %u\n", _sportl, _sporth);
  printf("dport   : \t%u - %u\n", _dportl, _dporth);
  printf("in/out  : \t%s\n", _inout.c_str());
#endif

  // Setup divert socket
  _fd = socket(AF_INET, SOCK_RAW, IPPROTO_DIVERT);
  if (_fd == -1) {
    errh->error("DivertSocket(socket): %s", strerror(errno));
    return -1;
  }
  bindPort.sin_family=AF_INET;
  bindPort.sin_port=htons(_divertport);
  bindPort.sin_addr.s_addr=0;

  // set REUSE option
  n = 1;
  if (setsockopt (_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&n, sizeof (n)) < 0) {
    errh->error("could not set REUSEADDR");
    close (_fd);
    return -1;
  }

  // bind to port
  ret=bind(_fd, (struct sockaddr *)&bindPort, sizeof(struct sockaddr_in));

  if (ret != 0) {
    close(_fd);
    errh->error("DivertSocket(bind): %s", strerror(errno));
    return -1;
  }

  fcntl(_fd, F_SETFL, O_NONBLOCK);

  // setup firewall
  if (_setup_fw && (ret = setup_firewall(errh) < 0))
    return ret;

  add_select(_fd, SELECT_READ);
  return 0;
}


void
DivertSocket::uninitialize()
{

  if (_fd >= 0) {

    if (_setup_fw) {
#if defined(__FreeBSD__)
      char tmp[64];
      sprintf(tmp, "/sbin/ipfw delete %u", _rulenumber);
      system(tmp);

#elif defined(__linux__)
      struct ip_fwdelnum ipfwd;

      ipfwd.fwd_rulenum = ipfc.fwn_rulenum;
      strcpy(ipfwd.fwd_label, ipfc.fwn_label);


      if (setsockopt(fw_sock, IPPROTO_IP, IP_FW_DELETE_NUM, &ipfwd, sizeof(ipfwd))==-1) {
	fprintf(stderr, "could not remove firewall rule");
      }

      if (_inout == "") {
	ipfwd.fwd_rulenum = ipfc2.fwn_rulenum;
	strcpy(ipfwd.fwd_label, ipfc2.fwn_label);

	if (setsockopt(fw_sock, IPPROTO_IP, IP_FW_DELETE_NUM, &ipfwd, sizeof(ipfwd))==-1) {
	  fprintf(stderr, "could not remove output firewall rule");
	}
      }
      close(fw_sock);

#else

#endif

    }

    //fprintf(stderr, "closing _fd\n");
    close (_fd);
    remove_select(_fd, SELECT_READ);
    _fd = -1;
  }
}


void
DivertSocket::selected(int fd, int)
{
  struct sockaddr_in sa;
  socklen_t fromlen;

  WritablePacket *p;
  int len;

  if (fd != _fd)
    return;

  fromlen = sizeof(sa);
  p  = Packet::make(2, 0, 2046, 0); // YIPAL bufsize
  len = recvfrom(_fd, p->data(), p->length(), 0, (sockaddr *)&sa, &fromlen);

  if (len > 0) {

    // set the timestamp
    p->timestamp_anno() = Timestamp::now();
    p->change_headroom_and_length(2, len);
    output(0).push(p);

  } else {
    p->kill();
    if (len <= 0 && errno != EAGAIN)
      click_chatter("DivertSocket: recvfrom: %s", strerror(errno));
  }
}


void
DivertSocket::send_packet(Packet *p)
{
  int n;
  struct sockaddr_in sa;

  sa.sin_len = sizeof(sa);
  sa.sin_family = AF_INET;
  sa.sin_port = htons(_divertport);
  memcpy(&sa.sin_addr.s_addr, p->data() + 16, 4);
  //printf("address: 0x%x\n", sa.sin_addr.s_addr);

  n = sendto(_fd, p->data(), p->length() , 0,
	     (sockaddr *)&sa, sizeof(sa));
  //click_chatter("  %i bytes reinjected.", n);

  if (n < 0)
    click_chatter("DivertSocket: %s", strerror(errno));
  p->kill();
}

void
DivertSocket::push(int, Packet *p)
{
  assert(p->length() >= 20);
  send_packet(p);
}


ELEMENT_REQUIRES(userlevel)
EXPORT_ELEMENT(DivertSocket)



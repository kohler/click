/*
 * ftpportmapper.{cc,hh} -- IPMapper for FTP PORT commands
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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
#include "ftpportmapper.hh"
#include <click/click_ip.h>
#include <click/click_tcp.h>
#include <click/router.hh>
#include <click/elemfilter.hh>
#include <click/confparse.hh>
#include <click/error.hh>

FTPPortMapper::FTPPortMapper()
  : Element(1, 1), _pattern(0)
{
  MOD_INC_USE_COUNT;
}

FTPPortMapper::~FTPPortMapper()
{
  MOD_DEC_USE_COUNT;
}

int
FTPPortMapper::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() != 3)
    return errh->error("wrong number of arguments; expected `FTPPortMapper(element, pattern)'");

  // get control packet rewriter
  Element *e = cp_element(conf[0], this, errh);
  if (!e)
    return -1;
  _control_rewriter = (TCPRewriter *)e->cast("TCPRewriter");
  if (!_control_rewriter)
    return errh->error("first argument must be a TCPRewriter-like element");

  // get data packet rewriter
  e = cp_element(conf[1], this, errh);
  if (!e)
    return -1;
  _data_rewriter = (IPRw *)e->cast("IPRw");
  if (!_data_rewriter)
    return errh->error("second argument must be an IPRewriter-like element");

  _pattern = 0;
  if (IPRw::Pattern::parse_with_ports(conf[2], &_pattern, &_forward_port,
				      &_reverse_port, this, errh) < 0)
    return -1;
  _pattern->use();
  if (_data_rewriter->notify_pattern(_pattern, errh) < 0)
    return -1;
  
  if (_forward_port >= _data_rewriter->noutputs()
      || _reverse_port >= _data_rewriter->noutputs())
    return errh->error("port out of range for `%s'", _data_rewriter->declaration().cc());
  else
    return 0;
}

int
FTPPortMapper::initialize(ErrorHandler *errh)
{
  // make sure that _control_rewriter is downstream
  CastElementFilter filter("TCPRewriter");
  Vector<Element *> downstream;
  router()->downstream_elements(this, 0, &filter, downstream);
  filter.filter(downstream);
  for (int i = 0; i < downstream.size(); i++)
    if (downstream[i] == _control_rewriter)
      goto found_control_rewriter;
  errh->warning("control packet rewriter `%s' is not downstream", _control_rewriter->declaration().cc());

 found_control_rewriter:
  return 0;
}

void
FTPPortMapper::cleanup(CleanupStage)
{
  if (_pattern)
    _pattern->unuse();
}

Packet *
FTPPortMapper::simple_action(Packet *p)
{
  const click_ip *iph = p->ip_header();
  assert(iph->ip_p == IP_PROTO_TCP);
  const click_tcp *tcph = p->tcp_header();
  unsigned data_offset = p->transport_header_offset() + (tcph->th_off<<2);
  const unsigned char *data = p->data() + data_offset;
  unsigned len = p->length() - data_offset;

  if (len < 4
      || (data[0] != 'P' && data[0] != 'p')
      || (data[1] != 'O' && data[1] != 'o')
      || (data[2] != 'R' && data[2] != 'r')
      || (data[3] != 'T' && data[3] != 't')
      || data[4] != ' ')
    return p;
  
  // parse the PORT command
  unsigned pos = 5;
  while (pos < len && data[pos] == ' ')
    pos++;
  unsigned port_arg_offset = pos;

  // followed by 6 decimal numbers separated by commas
  unsigned nums[6];
  nums[0] = nums[1] = nums[2] = nums[3] = nums[4] = nums[5] = 0;
  int which_num = 0;

  while (pos < len && which_num < 6) {
    if (data[pos] >= '0' && data[pos] <= '9')
      nums[which_num] = (nums[which_num] * 10) + data[pos] - '0';
    else if (data[pos] == ',')
      which_num++;
    else
      break;
    pos++;
  }

  // check that the command was complete and the numbers are ok
  if (which_num != 5 || pos >= len || (data[pos] != '\r' && data[pos] != '\n'))
    return p;
  for (int i = 0; i < 6; i++)
    if (nums[i] >= 256)
      return p;

  // OK; create the IP address and port number
  IPAddress src_data_addr(htonl((nums[0]<<24) | (nums[1]<<16) | (nums[2]<<8) | nums[3]));
  unsigned src_data_port = htons((nums[4]<<8) | nums[5]);

  // add mapping from specified port to destination
  unsigned dst_data_port = htons(ntohs(tcph->th_dport) - 1);
  IPFlowID flow(src_data_addr, src_data_port,
		IPAddress(iph->ip_dst), dst_data_port);

  // check for existing mapping
  IPRw::Mapping *forward = _data_rewriter->get_mapping(IP_PROTO_TCP, flow);
  if (!forward) {
    // create new mapping
    forward = _data_rewriter->apply_pattern(_pattern, IP_PROTO_TCP, flow,
					    _forward_port, _reverse_port);
    if (!forward)
      return p;
  }

  // rewrite PORT command to reflect mapping
  IPFlowID new_flow = forward->flow_id();
  unsigned new_saddr = ntohl(new_flow.saddr().addr());
  unsigned new_sport = ntohs(new_flow.sport());
  char buf[30];
  unsigned buflen;
  buflen = sprintf(buf, "%d,%d,%d,%d,%d,%d", (new_saddr>>24)&255, (new_saddr>>16)&255,
	  (new_saddr>>8)&255, new_saddr&255, (new_sport>>8)&255, new_sport&255);
  //click_chatter("%s", buf);

  WritablePacket *wp;
  unsigned port_arg_len = pos - port_arg_offset;
  if (port_arg_len < buflen) {
    wp = p->put(buflen - port_arg_len);
  } else {
    wp = p->uniqueify();
    wp->take(port_arg_len - buflen);
  }
  memmove(wp->data() + data_offset + port_arg_offset + buflen,
	  wp->data() + data_offset + port_arg_offset + port_arg_len,
	  len - pos);
  memcpy(wp->data() + data_offset + port_arg_offset,
	 buf,
	 buflen);

  // set IP length field, incrementally update IP checksum according to RFC1624
  // new_sum = ~(~old_sum + ~old_halfword + new_halfword)
  click_ip *wp_iph = wp->ip_header();
  unsigned short old_ip_hw = ((unsigned short *)wp_iph)[1];
  wp_iph->ip_len = htons(wp->length() - wp->ip_header_offset());
  unsigned short new_ip_hw = ((unsigned short *)wp_iph)[1];
  unsigned ip_sum =
    (~wp_iph->ip_sum & 0xFFFF) + (~old_ip_hw & 0xFFFF) + new_ip_hw;
  while (ip_sum >> 16)		// XXX necessary?
    ip_sum = (ip_sum & 0xFFFF) + (ip_sum >> 16);
  wp_iph->ip_sum = ~ip_sum;

  // set TCP checksum
  // XXX should check old TCP checksum first!!!
  click_tcp *wp_tcph = wp->tcp_header();

  // update sequence numbers in old mapping
  IPFlowID p_flow(p);
  if (TCPRewriter::TCPMapping *p_mapping = _control_rewriter->get_mapping(IP_PROTO_TCP, p_flow)) {
    tcp_seq_t interesting_seqno = ntohl(wp_tcph->th_seq) + len;
    p_mapping->update_seqno_delta(interesting_seqno, buflen - port_arg_len);
  }

  wp_tcph->th_sum = 0;
  unsigned wp_tcp_len = wp->length() - wp->transport_header_offset();
  unsigned csum = ~click_in_cksum((unsigned char *)wp_tcph, wp_tcp_len) & 0xFFFF;
#ifdef CLICK_LINUXMODULE
  csum = csum_tcpudp_magic(wp_iph->ip_src.s_addr, wp_iph->ip_dst.s_addr,
			   wp_tcp_len, IP_PROTO_TCP, csum);
#else
  {
    unsigned short *words = (unsigned short *)&wp_iph->ip_src;
    csum += words[0];
    csum += words[1];
    csum += words[2];
    csum += words[3];
    csum += htons(IP_PROTO_TCP);
    csum += htons(wp_tcp_len);
    while (csum >> 16)
      csum = (csum & 0xFFFF) + (csum >> 16);
    csum = ~csum & 0xFFFF;
  }
#endif
  wp_tcph->th_sum = csum;
  
  return wp;
}

ELEMENT_REQUIRES(TCPRewriter)
EXPORT_ELEMENT(FTPPortMapper)

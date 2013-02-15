/*
 * ftpportmapper.{cc,hh} -- IPMapper for FTP PORT commands
 * Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2009-2010 Meraki, Inc.
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
#include <clicknet/ip.h>
#include <clicknet/tcp.h>
#include <click/router.hh>
#include <click/routervisitor.hh>
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS

FTPPortMapper::FTPPortMapper()
{
}

FTPPortMapper::~FTPPortMapper()
{
}

int
FTPPortMapper::configure(Vector<String> &conf, ErrorHandler *errh)
{
    TCPRewriter *new_control_rewriter;
    IPRewriterBase *new_data_rewriter;
    int new_data_rewriter_input;

    if (Args(conf, this, errh)
	.read_mp("CONTROL_REWRITER", ElementCastArg("TCPRewriter"), new_control_rewriter)
	.read_mp("DATA_REWRITER", ElementCastArg("IPRewriterBase"), new_data_rewriter)
	.read_mp("DATA_REWRITER_INPUT", new_data_rewriter_input)
	.complete() < 0)
	return -1;

    if (new_data_rewriter_input < 0
	|| new_data_rewriter_input >= new_data_rewriter->ninputs())
	return errh->error("DATA_REWRITER_INPUT out of range");
    _control_rewriter = new_control_rewriter;
    _data_rewriter = new_data_rewriter;
    _data_rewriter_input = new_data_rewriter_input;
    return 0;
}

int
FTPPortMapper::initialize(ErrorHandler *errh)
{
    // make sure that _control_rewriter is downstream
    ElementCastTracker filter(router(), "TCPRewriter");
    router()->visit_downstream(this, 0, &filter);
    if (!filter.contains(_control_rewriter))
	errh->warning("control packet rewriter %<%s%> is not downstream", _control_rewriter->declaration().c_str());
    return 0;
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

  if (len < 5
      || (data[0] != 'P' && data[0] != 'p')
      || (data[1] != 'O' && data[1] != 'o')
      || (data[2] != 'R' && data[2] != 'r')
      || (data[3] != 'T' && data[3] != 't')
      || data[4] != ' ')
    return p;

  IPFlowID control_flow(p);
  IPRewriterEntry *p_mapping = _control_rewriter->get_entry(IP_PROTO_TCP, control_flow, -1);
  if (!p_mapping)
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

  // find or create mapping
  IPRewriterEntry *forward = _data_rewriter->get_entry(IP_PROTO_TCP, flow, _data_rewriter_input);
  if (!forward)
      return p;

  // rewrite PORT command to reflect mapping
  IPFlowID new_flow = forward->rewritten_flowid();
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
  tcp_seq_t interesting_seqno = ntohl(wp_tcph->th_seq) + len;
  TCPRewriter::TCPFlow *p_flow = static_cast<TCPRewriter::TCPFlow *>(p_mapping->flow());
  p_flow->update_seqno_delta(p_mapping->direction(), interesting_seqno,
                             buflen - port_arg_len);
  // assume the annotation from the control rewriter also applies to the
  // data
  forward->flow()->set_reply_anno(p_flow->reply_anno());

  wp_tcph->th_sum = 0;
  unsigned wp_tcp_len = wp->length() - wp->transport_header_offset();
  unsigned csum = click_in_cksum((unsigned char *)wp_tcph, wp_tcp_len);
  wp_tcph->th_sum = click_in_cksum_pseudohdr(csum, wp_iph, wp_tcp_len);

  return wp;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(TCPRewriter)
EXPORT_ELEMENT(FTPPortMapper)

/*
 * sourceipmapper.{cc,hh} -- source IP mapper (using consistent hashing)
 * Max Krohn, Eddie Kohler
 *
 * Copyright (c) 2005-2009 Max Krohn
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
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#if CLICK_BSDMODULE
# include <sys/limits.h>
#endif
#include "elements/ip/iprwpattern.hh"
#include "siphmapper.hh"
CLICK_DECLS

SourceIPHashMapper::SourceIPHashMapper()
  : _hasher (NULL)
{
}

SourceIPHashMapper::~SourceIPHashMapper()
{
}

void *
SourceIPHashMapper::cast(const char *name)
{
  if (name && strcmp("SourceIPHashMapper", name) == 0)
    return (Element *)this;
  else if (name && strcmp("IPMapper", name) == 0)
    return (IPMapper *)this;
  else
    return 0;
}

int
SourceIPHashMapper::parse_server(const String &conf, IPRewriterInput *input,
			      int *id_store, Element *e,
			      ErrorHandler *errh)
{
  Vector<String> words;
  cp_spacevec(conf, words);
  int32_t id;

  if (words.size () <= 1
      || !IntArg().parse(words[words.size() - 1], id)
      || id < 0)
    return errh->error("bad server ID in pattern spec");
  words.resize(words.size() - 1);
  *id_store = id;
  return IPRewriterPattern::parse_with_ports(cp_unspacevec(words), input,
					     e, errh) ? 0 : -1;
}

int
SourceIPHashMapper::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size () == 0)
    return errh->error ("no hash seed given");
  else if (conf.size() == 1)
    return errh->error("no patterns given");
  else if (conf.size() == 2)
    errh->warning("only one pattern given");

  int nnodes;
  int32_t seed;
  Vector<String> params;
  cp_spacevec (conf[0], params);
  if (params.size () != 2)
    return errh->error("requires 2 config params: numnodes seed");
  if (!IntArg().parse(params[0], nnodes) || nnodes <= 0)
    return errh->error("number of nodes must be an integer");
  if (!IntArg().parse(params[1], seed))
    return errh->error("hash seed must be an integer");

  int idp = 0;
  unsigned short *ids = new unsigned short[conf.size ()];

    for (int i = 1; i < conf.size(); i++) {
	IPRewriterInput is;
	is.kind = IPRewriterInput::i_pattern;
	int id;
	if (parse_server(conf[i], &is, &id, this, errh) >= 0) {
	    is.u.pattern->use();
	    _is.push_back(is);
	    ids[idp++] = id;
	}
    }

  if (_hasher)
    delete (_hasher);
  _hasher = new chash_t<int> (idp, ids, nnodes, seed);

  delete [] ids;
  return errh->nerrors() ? -1 : 0;
}

void
SourceIPHashMapper::cleanup(CleanupStage)
{
    for (int i = 0; i < _is.size(); i++)
	_is[i].u.pattern->unuse();
    delete _hasher;
}

void
SourceIPHashMapper::notify_rewriter(IPRewriterBase *user,
				    IPRewriterInput *input, ErrorHandler *errh)
{
    for (int i = 0; i < _is.size(); i++) {
	if (_is[i].foutput >= user->noutputs()
	    || _is[i].routput >= input->reply_element->noutputs())
	    errh->error("output port out of range in %s pattern %d", declaration().c_str(), i);
    }
}

int
SourceIPHashMapper::rewrite_flowid(IPRewriterInput *input,
				   const IPFlowID &flowid,
				   IPFlowID &rewritten_flowid,
				   Packet *p, int mapid)
{
    const struct in_addr ipsrc = flowid.saddr();
    unsigned int tmp, t2;
    memcpy (&tmp, &ipsrc, sizeof (tmp));
    t2 = tmp & 0xff;

    // make the lower bits have some more impact so that adjacent
    // IPs can be hashed to different servers.
    // note that this really isn't necessary for i386 alignment...
    tmp *= ((t2 << 24) | 0x1);
    tmp = tmp % INT_MAX;

    int v = _hasher->hash2ind (tmp);
    // debug code
    click_chatter ("%p -> %d", (void *)tmp, v);
    _is[v].reply_element = input->reply_element;
    input->foutput = _is[v].foutput;
    input->routput = _is[v].routput;
    return _is[v].rewrite_flowid(flowid, rewritten_flowid, p, mapid);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRewriterBase)
EXPORT_ELEMENT(SourceIPHashMapper)

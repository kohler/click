/*
 * sourceipmapper.{cc,hh} -- source IP mapper (using consistent hashing)
 *
 * $Id: siphmapper.cc,v 1.4 2004/10/05 18:49:56 eddietwo Exp $
 *
 */

#include <click/config.h>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#if CLICK_USERLEVEL
# include <limits.h>
#else 
#if CLICK_BSDMODULE
# include <machine/limits.h>
#endif
#endif
#include "siphmapper.hh"
CLICK_DECLS

SourceIPHashMapper::SourceIPHashMapper()
  : _hasher (NULL)
{
  MOD_INC_USE_COUNT;
}

SourceIPHashMapper::~SourceIPHashMapper()
{
  MOD_DEC_USE_COUNT;
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
SourceIPHashMapper::parse_server (const String &conf, IPRw::Pattern **pstore,
			      int *fport_store, int *rport_store,
			      int *id_store, Element *e, 
			      ErrorHandler *errh)
{
  Vector<String> words;
  cp_spacevec(conf, words);
  int32_t id;

  if (words.size () <= 1
      || !cp_integer(words[words.size() - 1], &id)
      || id < 0)
    return errh->error("bad server ID in pattern spec");
  words.resize(words.size() - 1);
  *id_store = id;
  return IPRw::Pattern::parse_with_ports (cp_unspacevec(words), pstore, 
					  fport_store,
					  rport_store, e, errh);
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

  int before = errh->nerrors();

  int nnodes;
  int32_t seed;
  Vector<String> params;
  cp_spacevec (conf[0], params);
  if (params.size () != 2)
    return errh->error("requires 2 config params: numnodes seed");
  if (!cp_integer(params[0], &nnodes) || nnodes <= 0)
    return errh->error("number of nodes must be an integer");
  if (!cp_integer(params[1], &seed))
    return errh->error("hash seed must be an integer");
  
  int idp = 0;
  unsigned short *ids = new unsigned short[conf.size ()];
  
  for (int i = 1; i < conf.size(); i++) {
    IPRw::Pattern *p;
    int f, r, id;
    if (parse_server (conf[i], &p, &f, &r, &id, this, errh) >= 0) {
      p->use();
      _patterns.push_back(p);
      _forward_outputs.push_back(f);
      _reverse_outputs.push_back(r);
      ids[idp++] = id;
    }
  }

  if (_hasher) 
    delete (_hasher);
  _hasher = new chash_t<int> (idp, ids, nnodes, seed);

  delete [] ids;
  return (errh->nerrors() == before ? 0 : -1);
}

void
SourceIPHashMapper::cleanup(CleanupStage)
{
  for (int i = 0; i < _patterns.size(); i++)
    _patterns[i]->unuse();
  delete _hasher;
}

void
SourceIPHashMapper::notify_rewriter(IPRw *rw, ErrorHandler *errh)
{
  int no = rw->noutputs();
  for (int i = 0; i < _patterns.size(); i++) {
    if (_forward_outputs[i] >= no || _reverse_outputs[i] >= no)
      errh->error("port in `%s' out of range for `%s'", declaration().cc(), rw->declaration().cc());
    rw->notify_pattern(_patterns[i], errh);
  }
}

IPRw::Mapping *
SourceIPHashMapper::get_map(IPRw *rw, int ip_p, const IPFlowID &flow, 
			    Packet *p)
{
  const click_ip *iph = p->ip_header();
  const struct in_addr ipsrc = iph->ip_src;
  unsigned int tmp, t2;
  memcpy (&tmp, &ipsrc, sizeof (tmp));
  t2 = tmp & 0xff;

  // make the lower bits have some more impact so that adjacent
  // IPs can be hashed to different servers.
  // note that this really isn't necessary for i386 alignment...
  tmp *= ((t2 << 24) | 0x1);
  tmp = tmp % INT_MAX;
  
  int v = _hasher->hash2ind (tmp);
  IPRw::Pattern *pat = _patterns[v];
  int fport = _forward_outputs[v];
  int rport = _reverse_outputs[v];

  // debug code
  click_chatter ("%p -> %d", (void *)tmp, v);

  return (rw->apply_pattern(pat, ip_p, flow, fport, rport));
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPRw)
EXPORT_ELEMENT(SourceIPHashMapper)

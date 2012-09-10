#ifndef CLICK_SOURCEIPMAPPER_HH
#define CLICK_SOURCEIPMAPPER_HH
#include <click/element.hh>
#include <click/ipflowid.hh>
#include <clicknet/ip.h>
#include "elements/ip/iprewriterbase.hh"
#include <click/glue.hh>
CLICK_DECLS

/*
 * =c
 * SourceIPHashMapper(NNODES SEED, PATTERN1, ..., PATTERNn)
 * =s nat
 * Source IP Hash mapper for IPRewriter(n)
 * =d
 *
 * Works in tandem with IPRewriter to provide source IP-based rewriting.
 * This is useful, for example, in load-balancing applications. Implements the
 * IPMapper interface.
 *
 * Like RoundRobinIPMapper, but also uses consistent hashing to map
 * map elements by source IP to the same node in the cluster, even
 * if nodes are added or removed.
 *
 *
 * =a IPRewriter, TCPRewriter, IPRewriterPatterns, RoundRobinIPMapper
 */

//
// JV-tree.  A dirt-simple binary tree for use in the consistent
// hash table.  Is balanced, but static.
//
template<class C, class K, K C::*key> int
jvcomp (const void *a, const void *b, void *)
{
  const C *ca = static_cast<const C *> (a);
  const C *cb = static_cast<const C *> (b);
  return int (ca->*key - cb->*key);
}

template<class C, class K, K C::*key>
class jvtree_t
{
public:
  jvtree_t (int sz, C *arr)
    : num (sz), tree (new C[sz])
  {
    click_qsort ((void *)arr, sz, sizeof (C), jvcomp<C,K,key>);

    int n = next2pow (sz);
    int h = n - 1;

    // f is the "first odd" that we start to map in our translation
    // scheme.
    int f = 2*((sz+1)/2) - 2*((h - sz)/ 2) + 1;

    int s,d;
    d = n;
    s = d >> 1;

    int to = 0;
    while (s > 0 && to < sz) {
      for (int x = s; x < n && to < sz; x += d) {
	int from = (x < f) ? x : (x + f - 1)/2;
	tree[to++] = arr[--from];
      }
      d = s;
      s = s >> 1;
    }
  }

  // should really be static, but if we keep it here, then everything
  // fits into the .h file
  int next2pow (int i)
  {
    int tmp = i;
    int p = 1;
    while ((tmp = (tmp >> 1)) > 0) p++;
    int r = 1;
    for (int i = 0; i < p; i++) r = r << 1;
    return r;
  }

  ~jvtree_t () { delete [] tree; }


  C *search (K k) const
  {
    int i = 0;
    C *ret = NULL;
    C *curr;
    K tkv;
    while (i < num) {
      curr = tree + i;
      tkv = curr->*key;
      if (k == tkv) {
	return curr;
      } else if (k > tkv) {
	i = 2*i + 2;
      } else {
	i = 2*i + 1;
	ret = curr;
      }
    }
    return ret ? ret : tree;
  }
private:
  int num;
  C *tree;
};

template<class K>
class chash_node_t {
public:
  chash_node_t () {}
  chash_node_t (K k, unsigned short v) : key (k), val (v), index (0) {}
  K key;
  unsigned short val;
  unsigned short index;
};

//
// consistent hash table, based on JV tree.
//
template<class K>
class chash_t {
public:
  chash_t (size_t ns, unsigned short *ids, size_t nn, int seed = 0x1)
    : num_servers (ns), num_nodes (nn)
  {
    click_srandom(seed);

    int max_servers = -1;
    for (size_t i = 0; i < num_servers; i++)
      if (ids[i] > max_servers)
	max_servers = ids[i];
    max_servers++;

    // temporary map for boolean server lookup
    char *servmap = new char[max_servers];
    memset (servmap, 0, max_servers);
    for (size_t i = 0; i < num_servers; i++)
      servmap[ids[i]] = 1;

    int n = num_servers * num_nodes;
    chash_node_t<K> *in = new chash_node_t<K> [n];
    int p = 0;
    unsigned short index = -1;
    for (unsigned short i = 0; i < max_servers; i++) {
      bool inc = false;
      for (unsigned int j = 0; j < num_nodes; j++) {
	int tmp = click_random(); // XXX: assumes randoms # from 0 to INT_MAX
	if (servmap[i]) {
	  if (!inc) {
	    inc = true;
	    index ++;
	  }
	  K ktmp = static_cast<K> (tmp);
	  in[p].key = ktmp;
	  in[p].val = i;
	  in[p++].index = index;
	}
      }
    }
    tree = new jvtree_t<chash_node_t<K>, K, &chash_node_t<K>::key> (n, in);

    delete [] in;
    delete [] servmap;
  }

  unsigned short hash (K k) const { return tree->search (k)->val; }
  unsigned short hash2ind (K k) const { return tree->search (k)->index; }

  ~chash_t ()
  {
    delete tree;
  }

private:
  size_t num_servers, num_nodes;
  jvtree_t<chash_node_t<K>, K, &chash_node_t<K>::key> *tree;
};


class SourceIPHashMapper : public Element, public IPMapper { public:

  SourceIPHashMapper() CLICK_COLD;
  ~SourceIPHashMapper() CLICK_COLD;

  const char *class_name() const	{ return "SourceIPHashMapper"; }
  void *cast(const char *);

  int configure_phase() const		{ return IPRewriterBase::CONFIGURE_PHASE_MAPPER;}
  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  void cleanup(CleanupStage) CLICK_COLD;

    void notify_rewriter(IPRewriterBase *user, IPRewriterInput *input,
			 ErrorHandler *errh);
    int rewrite_flowid(IPRewriterInput *input,
		       const IPFlowID &flowid, IPFlowID &rewritten_flowid,
		       Packet *p, int mapid);

protected:
    int parse_server(const String &conf, IPRewriterInput *input,
		     int *id_store, Element *e, ErrorHandler *errh);

 private:

    Vector<IPRewriterInput> _is;
    chash_t<int> *_hasher;
};

CLICK_ENDDECLS
#endif

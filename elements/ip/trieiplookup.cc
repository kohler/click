// -*- c-basic-offset: 4 -*-
#include <click/config.h>
#include <click/ipaddress.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#include <click/error.hh>
#include "trieiplookup.hh"
CLICK_DECLS

static int
prefix_order_compar(const void *athunk, const void *bthunk)
{
    const TrieIPLookup::Prefix* a = (const TrieIPLookup::Prefix*) athunk;
    const TrieIPLookup::Prefix* b = (const TrieIPLookup::Prefix*) bthunk;

    if (a->addr.addr() == b->addr.addr())
        return a->mask.mask_to_prefix_len() - b->mask.mask_to_prefix_len();
    else if (ntohl(a->addr.addr()) > ntohl(b->addr.addr()))
        return 1;
    else
        return -1;
}

TrieIPLookup::TrieIPLookup()
{
    MOD_INC_USE_COUNT;
    add_input();

    // initialize _default_rope
    _default_rope.push_back(0);
    _default_rope.push_back(1);
    _default_rope.push_back(2);
    _default_rope.push_back(4);
    _default_rope.push_back(8);
    _default_rope.push_back(16);
}

TrieIPLookup::~TrieIPLookup()
{
    MOD_DEC_USE_COUNT;
}

void TrieIPLookup::notify_noutputs(int n)
{
    set_noutputs(n);
}

int
TrieIPLookup::initialize(ErrorHandler *)
{
    return 0;
}

int
TrieIPLookup::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int before = errh->nerrors();
    for (int i = 0; i < conf.size(); i++) {
        Vector<String> words;
        cp_spacevec(conf[i], words);

        IPAddress dst, mask, gw;
        int32_t port;
        bool ok = false;
        if ((words.size() == 2 || words.size() == 3)
            && cp_ip_prefix(words[0], &dst, &mask, true, this)
            && cp_integer(words.back(), &port)) {
            if (words.size() == 3)
                ok = cp_ip_address(words[1], &gw, this);
            else
                ok = true;
        }

        if (ok && port >= 0) {
            add_to_route_vector(dst, mask, gw, port, errh);
        } else
            errh->error("argument %d should be `DADDR/MASK [GATEWAY] OUTPUT'", i+1);
    }

    if (errh->nerrors() == before) {
        build_main();
        return 0;
    } else
        return -1;
}

inline int
TrieIPLookup::add_to_route_vector(IPAddress addr, IPAddress mask, IPAddress gw, int output, ErrorHandler *errh)
{
    if (output < 0 || output >= noutputs())
        return errh->error("port number out of range");

    addr &= mask;
    _route_vector.push_back(Prefix(addr, mask, gw, output));

    return 0;
}

inline void
TrieIPLookup::build_init()
{
    // sort _route_vector
    click_qsort(&_route_vector[0], _route_vector.size(), sizeof(Prefix), prefix_order_compar);

    // get rid of duplicates in _route_vector
    int n_last = 0;
    int n_current = 1;
    while (n_current < _route_vector.size()) {
        if (_route_vector[n_last].addr != _route_vector[n_current].addr ||
            _route_vector[n_last].mask != _route_vector[n_current].mask) {
            n_last++;
            if (n_last < n_current)
                _route_vector[n_last] = _route_vector[n_current];
        }
        n_current++;
    }

    for (int i = 0; i < _route_vector.size() - n_last - 1; i++) {
        _route_vector.pop_back();
    }

    int n_increment = 0;
    // if there's no root node, add it
    if (_route_vector[0].mask.mask_to_prefix_len() != 0) {
        trie_vector.push_back(TrieNode(0, 0, 0, -1, true, n_increment++));
    }

    // fill trie_vector
    for (int i = 0; i < _route_vector.size(); i++) {
        const Prefix prefix = _route_vector[i];
        trie_vector.push_back(TrieNode(prefix.addr, prefix.mask, prefix.gw,
                                       prefix.output, true, i+n_increment));
    }
}

inline int
TrieIPLookup::build_exists_middle(const TrieNode& parent, const TrieNode& child)
{
    // no need for middle child
    if (-1 == parent.left_child) {
        assert(-1 == parent.right_child);
        return 0;
    }

    // sibling to with common prefix with
    const TrieNode &sibling = -1 != parent.right_child ?
        trie_vector[parent.right_child] : trie_vector[parent.left_child];

    // just making sure
    assert(parent.mask.mask_to_prefix_len() < child.mask.mask_to_prefix_len());
    assert(parent.mask.mask_to_prefix_len() < sibling.mask.mask_to_prefix_len());
    assert((child.addr & parent.mask) == parent.addr);
    assert((sibling.addr & parent.mask) == parent.addr);

    int lower_bound = parent.mask.mask_to_prefix_len() + 1;
    int upper_bound = child.mask.mask_to_prefix_len();

    for (int i = lower_bound; i <= upper_bound; i++) {
        IPAddress newmask = IPAddress::make_prefix(i);
        if ((newmask & child.addr) != (newmask & sibling.addr)) {
            if (i == lower_bound) {
                assert(-1 == parent.right_child);
                return 0;
            } else {
                return i - 1;
            }
        }
    }
    assert(false);
}

// returns whether there exists a middle
inline void
TrieIPLookup::build_middle(int prefix_length, const TrieNode& parent, const TrieNode& child)
{
    assert(-1 != parent.left_child);
    const TrieNode &sibling = -1 != parent.right_child ?
        trie_vector[parent.right_child] : trie_vector[parent.left_child];

    // create middle node
    IPAddress mask = IPAddress::make_prefix(prefix_length);
    TrieNode middle(child.addr & mask, mask, parent.gw, parent.output, false, trie_vector.size());
    middle.left_child = sibling.index;
    middle.right_child = child.index;
    middle.parent = parent.index;
    trie_vector.push_back(middle);

    // make all child/parent connections
    if (-1 != parent.right_child) {
        trie_vector[parent.index].right_child = middle.index;
    } else {
        trie_vector[parent.index].left_child = middle.index;
    }

    trie_vector[sibling.index].parent = middle.index;
    trie_vector[child.index].parent = middle.index;

    // just making sure
    check_trie_node(middle);
    check_trie_node(trie_vector[middle.left_child]);
    check_trie_node(trie_vector[middle.right_child]);
}


inline void
TrieIPLookup::build_trie()
{
    if (trie_vector.size() == 0)
        return;

    TrieNode cn = trie_vector[0];

    // take care of the rest
    int trie_vector_size = trie_vector.size();
    for (int i = 1; i < trie_vector_size; i++) {

        check_trie(trie_vector[0]);

        TrieNode tn = trie_vector[i];

        while ((tn.mask.mask_to_prefix_len() <= cn.mask.mask_to_prefix_len()) ||
               ((tn.addr & cn.mask) != cn.addr)) {
            // while the above condition is true, tn should not be a child of cn
            cn = trie_vector[cn.parent];
        }

        // make sure tn should be a child of cn
        assert((tn.addr & cn.mask) == cn.addr);

        // should we create a fake node?
        int prefix_length = build_exists_middle(cn, tn);
        if (prefix_length) {
            check_trie_node(trie_vector[cn.left_child]);
            build_middle(prefix_length, cn, tn);
        } else {
            assert(-1 == cn.right_child);
            if (-1 != cn.left_child) {
                trie_vector[cn.index].right_child = tn.index;
            } else {
                trie_vector[cn.index].left_child = tn.index;
            }
            trie_vector[tn.index].parent = cn.index;
            check_trie_node(trie_vector[tn.index]);
        }
        cn = trie_vector[tn.index];
    }
}

void
TrieIPLookup::build_main()
{
    struct timeval tv;
    click_gettimeofday(&tv);
    click_chatter("starting to init data structure: %d\n", tv.tv_sec);

    // build initial data structure
    trie_vector.clear();
    build_init();
    check_init();

    click_gettimeofday(&tv);
    click_chatter("starting to build trie: %d\n", tv.tv_sec);

    // build trie
    build_trie();
    check_trie(trie_vector[0]);

    click_gettimeofday(&tv);
    click_chatter("starting to set children lengths: %d\n", tv.tv_sec);

    // set children lengths
    build_children_lengths(0);
    check_lengths(trie_vector[0]);

    click_gettimeofday(&tv);
    click_chatter("starting to rebuild hash: %d\n", tv.tv_sec);

    // rebuild hash
    for (int i = 0; i <= 32; i++) {
        _lengths_array[i].clear();
    }
    build_hash(0);

    click_gettimeofday(&tv);
    click_chatter("done initializaton: %d\n", tv.tv_sec);

    trie_vector.clear();
}

inline void
TrieIPLookup::build_hash(int index)
{
    if (-1 == index) return;

    TrieNode &tn = trie_vector[index];

    // maybe add this node to the hashmap
    build_hash_node(tn, _default_rope, 32);

    // call this method recursively on all the children
    build_hash(tn.left_child);
    build_hash(tn.right_child);
}

inline void
TrieIPLookup::build_hash_node(TrieNode &tn, const Rope& rope, int upper_bound_inclusive)
{
    int index;
    int length;
    LengthHash* lh;
    Marker* p_marker;

    if (rope.size() == 0 && !tn.is_real)
        return;
    
    // get the first index from the rope
    assert(rope.size() > 0);
    index = rope.back();

    // get length of the bitmask
    length = tn.mask.mask_to_prefix_len();

    if (length < index) {
        // our final place is somewhere above here, so follow the rope
        Rope new_rope(rope);
        new_rope.pop_back();
        build_hash_node(tn, new_rope, index-1);
    } else {
        IPAddress current_mask = IPAddress::make_prefix(index);

        // get the hashmap corresponding to that index
        lh = &_lengths_array[index];

        // look for a marker/prefix
        p_marker = lh->findp(tn.addr & current_mask);

        if (NULL != p_marker) {
            // assert this isn't our final resting place
            assert(index != length);

            // go to the new trie and continue searching
            build_hash_node(tn, p_marker->rope, upper_bound_inclusive);
        } else {
            // we are the first to visit this level, so create marker and insert it
            Marker new_marker;

            build_hash_marker(new_marker, tn, length, index, upper_bound_inclusive);
            lh->insert(tn.addr & current_mask, new_marker);

            // continue
            if (length > index) {
                build_hash_node(tn, new_marker.rope, upper_bound_inclusive);
            }
        }
    }
}

inline void
TrieIPLookup::build_hash_marker(Marker &new_marker, TrieNode tn, int n_prefix_length,
                                int n_array_index, int upper_bound_inclusive)
{
    // set the rope for our new marker
    Vector<int> lengths_vec;
    if (n_prefix_length != n_array_index) lengths_vec.push_back(n_prefix_length);
    int i;
    for (i = 0; i < 32; i++) {
        if (tn.children_lengths & (1 << i)) {
            if (n_prefix_length + i + 1 <= upper_bound_inclusive)
                lengths_vec.push_back(n_prefix_length + i + 1);
        }
    }
    
    int positions[6];
    int end = 0;
    
    i = lengths_vec.size();
    while (i > 0) {
        i /= 2;
        positions[end] = lengths_vec[i];
        end++;
    }
    
    for (i = 0; i < end; i++) {
        new_marker.rope.push_back(positions[end - i - 1]);
    }

    // set gw & output
    while(n_array_index < tn.mask.mask_to_prefix_len()) {
        tn = trie_vector[tn.parent];
    }
    new_marker.gw = tn.gw;
    new_marker.output = tn.output;
}

inline void
TrieIPLookup::build_children_lengths(int index)
{
    // initialize it to 0
    TrieNode &tn = trie_vector[index];
    tn.children_lengths = 0;

    // loop through children and get their lengths
    if (-1 != tn.left_child) {
        build_children_lengths(tn.left_child);
        TrieNode &left_child = trie_vector[tn.left_child];
	
        // update my variable
        int distance = left_child.mask.mask_to_prefix_len() - tn.mask.mask_to_prefix_len();
        tn.children_lengths |= (left_child.children_lengths << distance);

        // if this child is real, then add it also
        if (left_child.is_real) {
            tn.children_lengths |= (1 << (distance-1));
        }
    }

    if (-1 != tn.right_child) {
        build_children_lengths(tn.right_child);
        TrieNode &right_child = trie_vector[tn.right_child];
	
        // update my variable
        int distance = right_child.mask.mask_to_prefix_len() - tn.mask.mask_to_prefix_len();
        tn.children_lengths |= (right_child.children_lengths << distance);

        // if this child is real, then add it also
        if (right_child.is_real) {
            tn.children_lengths |= (1 << (distance-1));
        }
    }
}

void
TrieIPLookup::add_handlers() {
}


int
TrieIPLookup::add_route(IPAddress addr, IPAddress mask, IPAddress gw,
                        int output, ErrorHandler *errh)
{
    int before = errh->nerrors();
    add_to_route_vector(addr, mask, gw, output, errh);
    if (errh->nerrors() == before) {
        build_main();
        return 0;
    } else
        return -1;
}

int
TrieIPLookup::remove_route(IPAddress, IPAddress, IPAddress, int, ErrorHandler *)
{
    return 0;
}

int
TrieIPLookup::lookup_route(IPAddress a, IPAddress &gw) const
{
    int output = -1;
    Rope rope = _default_rope;
    while (rope.size() > 0) {
        int index = rope.back();
        IPAddress current_mask = IPAddress::make_prefix(index);
        Marker* pmarker = _lengths_array[index].findp(a & current_mask);

        if (0 == pmarker) {
            // we didn't find a marker at this level
            rope.pop_back();
        } else {
            // we found a marker, so update everything
            output = pmarker->output;
            gw = pmarker->gw;
            rope = pmarker->rope;
        }
    }
    return output;
}

String
TrieIPLookup::dump_routes() const
{
    return NULL;
}

void
TrieIPLookup::print_route_vector() const
{
    click_chatter("\n");
    for (int i = 0; i < _route_vector.size(); i++) {
        click_chatter("%s\n", _route_vector[i].unparse().cc());
    }
    click_chatter("\n");
}

void
TrieIPLookup::print_lengthhash(const LengthHash& lengthhash) const
{
    LengthHash::iterator it = lengthhash.begin();
    for (int i = 0; i < lengthhash.size(); i++) {
        click_chatter("%s : %s", it.key().unparse().cc(), it.value().unparse().cc());
    }
}

void
TrieIPLookup::print_trie(const TrieNode& tn) const
{
    click_chatter("%s", tn.unparse().cc());
}

void
TrieIPLookup::check_init()
{
    assert(_route_vector.size() >= trie_vector.size());
    if (!trie_vector.size()) return;

    // elements should be in order and there should be no duplicates
    TrieNode last_node = trie_vector[0];

    for (int i = 1; i < trie_vector.size(); i++) {
        assert(last_node < trie_vector[i]);
        last_node = trie_vector[i];
    }

    // these should be the same elements as in _route_vector
    for (int i = 0; i < trie_vector.size(); i++) {
        TrieNode tn = trie_vector[i];
        bool ok = false;
        for (int i = 0; i < _route_vector.size(); i++) {
            Prefix prefix = _route_vector[i];
            if ((prefix.addr == tn.addr) && (prefix.mask == tn.mask) &&
                (prefix.gw == tn.gw) && (prefix.output == tn.output)) {
                ok = true;
                break;
            }
        }
        assert(ok);
    }

    for (int i = 0; i < trie_vector.size(); i++) {
        TrieNode tn = trie_vector[i];
        assert((tn.mask & tn.addr) == tn.addr);
    }
}

void
TrieIPLookup::check_trie_node(const TrieNode& node)
{
    assert(node.mask == (IPAddress::make_prefix(node.mask.mask_to_prefix_len())));
    assert ((node.mask & node.addr) == node.addr);

    if (node.mask.mask_to_prefix_len() == 0) {
        assert(-1 == node.parent);
        assert(0 == node.index);
    } else {
        assert(-1 != node.parent);
        assert(0 < node.index);
        TrieNode &parent = trie_vector[node.parent];
        assert(parent.mask.mask_to_prefix_len() < node.mask.mask_to_prefix_len());
        assert((node.addr & parent.mask) == parent.addr);
    }
    if (node.mask.mask_to_prefix_len() && node.is_real)
        assert(node.output != -1);
}

void
TrieIPLookup::check_trie(const TrieNode& root)
{
    check_trie_node(root);

    if (-1 != root.left_child) {
        check_trie(trie_vector[root.left_child]);
    }

    if (-1 != root.right_child) {
        check_trie(trie_vector[root.right_child]);
    }
}

void
TrieIPLookup::check_lengths(const TrieNode& root) {
    uint32_t children_lengths = 0;
    if (-1 != root.left_child) {
        TrieNode &left_child = trie_vector[root.left_child];
        int distance = left_child.mask.mask_to_prefix_len() - root.mask.mask_to_prefix_len();
        children_lengths |= (left_child.children_lengths << distance);

        if (left_child.is_real) {
            children_lengths |= (1 << (distance - 1));
        }

        check_lengths(left_child);
    }

    if (-1 != root.right_child) {
        TrieNode &right_child = trie_vector[root.right_child];
        int distance = right_child.mask.mask_to_prefix_len() - root.mask.mask_to_prefix_len();
        children_lengths |= (right_child.children_lengths << distance);

        if (right_child.is_real) {
            children_lengths |= (1 << (distance - 1));
        }

        check_lengths(right_child);
    }

    assert (children_lengths == root.children_lengths);
}


#include<click/hashmap.cc>
#include<click/vector.cc>

ELEMENT_REQUIRES(IPRouteTable)
EXPORT_ELEMENT(TrieIPLookup)

CLICK_ENDDECLS

#ifndef CLICK_AGGCOUNTER_HH
#define CLICK_AGGCOUNTER_HH
#include <click/element.hh>

/*
=c

AggregateCounter

tcpdpriv(1) */

class AggregateCounter : public Element { public:
  
    AggregateCounter();
    ~AggregateCounter();
  
    const char *class_name() const	{ return "AggregateCounter"; }
    const char *processing() const	{ return AGNOSTIC; }
    AggregateCounter *clone() const	{ return new AggregateCounter; }

    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();
    void add_handlers();

    Packet *simple_action(Packet *);

    int write_file(String, bool, ErrorHandler *) const;
    
  private:

    struct Node {
	uint32_t aggregate;
	uint32_t count;
	Node *child[2];
    };

    bool _bytes;
    
    Node *_root;
    Node *_free;
    Vector<Node *> _blocks;

    Node *new_node();
    Node *new_node_block();
    void free_node(Node *);

    Node *make_peer(uint32_t, Node *);
    Node *find_node(uint32_t);

    static uint32_t write_nodes(Node *, FILE *, bool, uint32_t *, int &, int, ErrorHandler *);
    static int write_file_handler(const String &, Element *, void *, ErrorHandler *);
    
};

inline AggregateCounter::Node *
AggregateCounter::new_node()
{
    if (_free) {
	Node *n = _free;
	_free = n->child[0];
	return n;
    } else
	return new_node_block();
}

inline void
AggregateCounter::free_node(Node *n)
{
    n->child[0] = _free;
    _free = n;
}

#endif

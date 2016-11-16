#ifndef CLICK_CLASSIFICATION_HH
#define CLICK_CLASSIFICATION_HH 1
#define CLICK_CLASSIFICATION_WORDWISE_DOMINATOR_FASTPRED 1
#include <click/packet.hh>
#include <click/vector.hh>
CLICK_DECLS
class ErrorHandler;
namespace Classification {

enum Jumps {
    j_never = -2147483647,	// Output means "drop packet."
    j_failure,			// Parse-time output for fail branch.
    j_success			// Parse-time output for success branch.
};

enum Combiner {
    c_and,
    c_or,
    c_ternary
};

enum {
    offset_max = 0x7FFFFFFF
};

namespace Wordwise {

class DominatorOptimizer;


struct Insn {
    uint16_t offset;
    uint8_t padding;
    uint8_t short_output;
    union {
	unsigned char c[4];
	uint32_t u;
    } mask;
    union {
	unsigned char c[4];
	uint32_t u;
    } value;
    int32_t j[2];

    enum {
	width = 4
    };

    Insn(int offset_, uint32_t value_, uint32_t mask_,
	 int32_t failure_ = j_failure, int32_t success_ = j_success,
	 bool short_output_ = false)
	: offset(offset_), padding(0), short_output(short_output_) {
	mask.u = mask_;
	value.u = value_ & mask_;
	j[0] = failure_;
	j[1] = success_;
    }

    int32_t yes() const			{ return j[1]; }
    int32_t &yes()			{ return j[1]; }
    int32_t no() const			{ return j[0]; }
    int32_t &no()			{ return j[0]; }

    unsigned required_length() const {
	if (mask.u == 0)
	    return 0;
	else if (mask.c[3])
	    return offset + 4;
	else if (mask.c[2])
	    return offset + 3;
	else if (mask.c[1])
	    return offset + 2;
	else
	    return offset + 1;
    }

    /** @brief Test whether a packet that matches *this must match @a x.
     * @param known_length The number of packet bytes known to definitively
     *   exist when this instruction is executed. */
    bool implies(const Insn &x, unsigned known_length) const;
    /** @brief Test whether a packet that does not match *this must match @a x.
     * @param known_length The number of packet bytes known to definitively
     *   exist when this instruction is executed.
     *
     * This happens when either @a x matches everything, or @a x and *this
     * both match against the same single bit, and they have different
     * values. */
    bool not_implies(const Insn &x, unsigned known_length) const;
    /** @brief Test whether a packet that matches *this must not match @a x.
     * @param known_length The number of packet bytes known to definitively
     *   exist when this instruction is executed. */
    bool implies_not(const Insn &x, unsigned known_length) const;
    /** @brief Test whether a packet that does not match *this must not match
     * @a x.
     * @param known_length The number of packet bytes known to definitively
     *   exist when this instruction is executed. */
    bool not_implies_not(const Insn &x, unsigned known_length) const;
    /** @brief Test whether this instruction and @a x are compatible.
     * @param consider_short If false, don't consider short packets.
     *
     * Instructions are compatible if every bit pattern that matches one
     * instruction could be extended into a bit pattern that matches both
     * instructions. */
    bool compatible(const Insn &x, bool consider_short) const;
    /** @brief Test whether this instruction and @a x form a pair whose
     *   combined effect is that of an instruction with a less specific
     *   mask.
     *
     * @a x is expected to be reachable from *this by the no branch. */
    bool generalizable_or_pair(const Insn &x) const;

    /** @brief Test whether this instruction is flippable.
     *
     * Flipping an instruction swaps its "yes" and "no" branches and the value
     * of its test.  Only single-bit tests are flippable. */
    bool flippable() const;
    /** @brief Flip this instruction.
     * @pre flippable() */
    void flip();

    String unparse() const;

    static int compare(const Insn &a, const Insn &b) {
	return memcmp(&a, &b, 12);
    }

  private:

    inline bool implies_short_ok(bool direction, const Insn &x, bool next_direction, unsigned known_length) const {
	// Common cases.
	if (short_output != direction || offset + 4 <= (int) known_length)
	    return true;
	else
	    return hard_implies_short_ok(direction, x, next_direction, known_length);
    }

    bool hard_implies_short_ok(bool direction, const Insn &x, bool next_direction, unsigned known_length) const;

};

StringAccum &operator<<(StringAccum &sa, const Insn &insn);


class Program { public:

    Program(unsigned align_offset = 0)
	: _output_everything(-j_never), _safe_length((unsigned) -1),
	  _align_offset(align_offset) {
    }

    unsigned align_offset() const {
	return _align_offset;
    }
    int output_everything() const {
	return _output_everything;
    }
    unsigned safe_length() const {
	return _safe_length;
    }

    int ninsn() const {
	return _insn.size();
    }
    const Insn &insn(int i) const {
	return _insn[i];
    }
    const Insn *begin() const {
	return _insn.begin();
    }
    const Insn *end() const {
	return _insn.end();
    }

    Insn &back() {
	return _insn.back();
    }

    void add_insn(Vector<int> &tree, int offset, uint32_t value, uint32_t mask);
    void add_raw_insn(Insn new_insn);

    Vector<int> init_subtree() const;
    void start_subtree(Vector<int> &tree) const;
    /** @brief Negate the meaning of the last subtree, so that packets that
     * match the last subtree test will jump to "failure" and packets that
     * don't match will jump to "success".
     * @param flip_short If true, then also flip whether short packets
     *   match. */
    void negate_subtree(Vector<int> &tree, bool flip_short = false);
    void finish_subtree(Vector<int> &tree, Combiner op = c_and,
			int success = j_success, int failure = j_failure);

    void combine_compatible_states();
    void remove_unused_states();
    void unaligned_optimize();
    void count_inbranches(Vector<int> &inbranches) const;
    void bubble_sort_and_exprs(const int *offset_map_begin, const int *offset_map_end, int last_offset);
    void optimize(const int *offset_map_begin, const int *offset_map_end, int last_offset);

    void warn_unused_outputs(int noutputs, ErrorHandler *errh) const;

    void offset_insn_tree(int step_offset);
    void redirect_unfinished_insn_tree(int new_target);

    int match(const Packet *p);

    String unparse() const;

  private:

    Vector<Insn> _insn;
    int _output_everything;
    unsigned _safe_length;
    unsigned _align_offset;

    void redirect_subtree(int first, int next, int success, int failure);

    int length_checked_match(const Packet *p);
    static inline int map_offset(int offset, const int *begin, const int *end);
    static int hard_map_offset(int offset, const int *begin, const int *end);

    friend class DominatorOptimizer;

};


class CompressedProgram { public:

    CompressedProgram()
	: _output_everything(-j_never), _safe_length((unsigned) -1),
	  _align_offset(0) {
    }

    unsigned align_offset() const {
	return _align_offset;
    }
    int output_everything() const {
	return _output_everything;
    }
    unsigned safe_length() const {
	return _safe_length;
    }

    const uint32_t *begin() const {
	return _zprog.begin();
    }
    const uint32_t *end() const {
	return _zprog.end();
    }

    void compile(const Program &prog, bool perform_binary_search,
		 unsigned min_binary_search);

    void warn_unused_outputs(int noutputs, ErrorHandler *errh) const;

    String unparse() const;

  private:

    Vector<uint32_t> _zprog;
    int _output_everything;
    unsigned _safe_length;
    unsigned _align_offset;

};


class DominatorOptimizer { public:

    DominatorOptimizer(Program *p);

    static int brno(int state, bool br)		{ return (state << 1) + br; }
    static int stateno(int brno)		{ return brno >> 1; }
    static bool br_yes(int brno)		{ return brno & 1; }

    bool br_implies(int brno, int state) const {
	assert(state > 0);
	int from_state = stateno(brno);
	unsigned kl = _known_length[from_state];
	if (br_yes(brno))
	    return insn(from_state).implies(insn(state), kl);
	else
	    return insn(from_state).not_implies(insn(state), kl);
    }

    bool br_implies_not(int brno, int state) const {
	assert(state > 0);
	int from_state = stateno(brno);
	unsigned kl = _known_length[from_state];
	if (br_yes(brno))
	    return insn(from_state).implies_not(insn(state), kl);
	else
	    return insn(from_state).not_implies_not(insn(state), kl);
    }

    void run(int state);

    void print();

  private:

    Program *_p;
    Vector<int> _known_length;
    Vector<int> _insn_id;
    Vector<int> _dom;
    Vector<int> _dom_start;
    Vector<int> _domlist_start;
#if CLICK_CLASSIFICATION_WORDWISE_DOMINATOR_FASTPRED
    mutable Vector<int> _pred_first;	// indexed by state (insn id)
    mutable Vector<int> _pred_next;	// indexed by branch
    mutable Vector<int> _pred_prev;	// indexed by branch
#endif

    enum { MAX_DOMLIST = 4 };

    Insn &insn(int state) const {
	return _p->_insn[state];
    }
    int ninsn() const {
	return _p->_insn.size();
    }

    static void intersect_lists(const Vector<int> &, const Vector<int> &, const Vector<int> &, int pos1, int pos2, Vector<int> &);
    static int last_common_state_in_lists(const Vector<int> &, const Vector<int> &, const Vector<int> &);
    void find_predecessors(int state, Vector<int> &) const;
    int dom_shift_branch(int brno, int to_state, int dom, int dom_end, Vector<int> *collector);
    void shift_branch(int state, bool branch);
    void calculate_dom(int state);

    inline void set_branch(int from_state, bool branch, int to_state) {
	Insn &in = insn(from_state);
#if CLICK_CLASSIFICATION_WORDWISE_DOMINATOR_FASTPRED
	int br = brno(from_state, branch);
	if (in.j[branch] > 0) {
	    if (_pred_prev[br] >= 0)
		_pred_next[_pred_prev[br]] = _pred_next[br];
	    else
		_pred_first[in.j[branch]] = _pred_next[br];
	    if (_pred_next[br] >= 0)
		_pred_prev[_pred_next[br]] = _pred_prev[br];
	}
	if (to_state > 0) {
	    int prev = -1, *pnext = &_pred_first[to_state];
	    while (*pnext >= 0 && *pnext < br) {
		prev = *pnext;
		pnext = &_pred_next[*pnext];
	    }
	    _pred_prev[br] = prev;
	    _pred_next[br] = *pnext;
	    *pnext = br;
	    if (_pred_next[br] >= 0)
		_pred_prev[_pred_next[br]] = br;
	}
#endif
	in.j[branch] = to_state;
    }

};


inline int
Program::match(const Packet *p)
{
    if (_output_everything >= 0)
	return _output_everything;
    else if (p->length() < _safe_length)
	// common case never checks packet length
	return length_checked_match(p);

    const unsigned char *packet_data = p->data() - _align_offset;
    int pos = 0;
    Insn *ex = &_insn[0];     // avoid bounds checking

    do {
	uint32_t data = *((const uint32_t *)(packet_data + ex[pos].offset));
	data &= ex[pos].mask.u;
	pos = ex[pos].j[data == ex[pos].value.u];
    } while (pos > 0);

    return -pos;
}

}}
CLICK_ENDDECLS
#endif

#ifndef CLICK_CLASSIFIER_HH
#define CLICK_CLASSIFIER_HH
#include <click/element.hh>

/*
 * =c
 * Classifier(pattern1, ..., patternN)
 * =s classification
 * classifies packets by contents
 * =d
 * Classifies packets. The Classifier has N outputs, each associated with the
 * corresponding pattern from the configuration string.
 * A pattern is a set of clauses, where each clause is either "offset/value"
 * or "offset/value%mask". A pattern matches if the packet has the indicated
 * value at each offset.
 *
 * The clauses in each pattern are separated
 * by spaces. A clause consists of the offset, "/", the value, and (optionally)
 * "%" and a mask. The offset is in decimal. The value and mask are in hex.
 * The length of the value is implied by the number of hex digits, which must
 * be even. "?" is also allowed as a "hex digit"; it means "don't care about
 * the value of this nibble".
 *
 * If present, the mask must have the same number of hex digits as the value.
 * The matcher will only check bits that are 1 in the mask.
 *
 * A clause may be preceded by "!", in which case the clause must NOT match
 * the packet.
 *
 * As a special case, a pattern consisting of "-" matches every packet.
 *
 * The patterns are scanned in order, and the packet is sent to the output
 * corresponding to the first matching pattern. Thus more specific patterns
 * should come before less specific ones. You will get a warning if no packet
 * could ever match a pattern. Usually, this is because an earlier pattern is
 * more general, or because your pattern is contradictory (`12/0806 12/0800').
 *
 * =n
 *
 * The IPClassifier and IPFilter elements have a friendlier syntax if you are
 * classifying IP packets.
 *
 * =e
 * For example,
 *
 *   Classifier(12/0806 20/0001,
 *              12/0806 20/0002,
 *              12/0800,
 *              -);
 *
 * creates an element with four outputs intended to process
 * Ethernet packets.
 * ARP requests are sent to output 0, ARP replies are sent to
 * output 1, IP packets to output 2, and all others to output 3.
 *
 * =h program read-only
 * Returns a human-readable definition of the program the Classifier element
 * is using to classify packets. At each step in the program, four bytes
 * of packet data are ANDed with a mask and compared against four bytes of
 * classifier pattern.
 *
 * The Classifier patterns above compile into the following program:
 *
 *   0  12/08060000%ffff0000  yes->step 1  no->step 3
 *   1  20/00010000%ffff0000  yes->[0]  no->step 2
 *   2  20/00020000%ffff0000  yes->[1]  no->[3]
 *   3  12/08000000%ffff0000  yes->[2]  no->[3]
 *   safe length 22
 *   alignment offset 0
 *
 * =a IPClassifier, IPFilter */

class Classifier : public Element { public:

  class Expr;
  
  Classifier();
  ~Classifier();
  
  const char *class_name() const		{ return "Classifier"; }
  const char *processing() const		{ return PUSH; }
  
  Classifier *clone() const;
  int configure(Vector<String> &, ErrorHandler *);
  void add_handlers();

  // creating Exprs
  enum { NEVER = -2147483647, FAILURE, SUCCESS };
  void add_expr(Vector<int> &, const Expr &);
  void add_expr(Vector<int> &, int offset, uint32_t value, uint32_t mask);
  void init_expr_subtree(Vector<int> &);
  void start_expr_subtree(Vector<int> &);
  void negate_expr_subtree(Vector<int> &);
  void finish_expr_subtree(Vector<int> &, bool is_and, int success = SUCCESS, int failure = FAILURE);
  
  void push(int port, Packet *);
  
  struct Expr {
    int offset;
    union {
      unsigned char c[4];
      uint32_t u;
    } mask;
    union {
      unsigned char c[4];
      uint32_t u;
    } value;
    int yes;
    int no;
    bool implies(const Expr &) const;
    bool implies_not(const Expr &) const;
    bool not_implies(const Expr &) const;
    bool not_implies_not(const Expr &) const;
    bool compatible(const Expr &) const;
    bool flippable() const;
    void flip();
    String s() const;
  };

 protected:
  
  Vector<Expr> _exprs;
  int _output_everything;
  unsigned _safe_length;
  unsigned _align_offset;

  void combine_compatible_states();
  bool remove_unused_states();
  //int count_occurrences(const Expr &, int state, bool first) const;
  //bool remove_duplicate_states();
  void unaligned_optimize();
  void optimize_exprs(ErrorHandler *);
  
  static String program_string(Element *, void *);
  
  void length_checked_push(Packet *);

 private:

  class DominatorOptimizer { public:

    DominatorOptimizer(Classifier *c);

    static int brno(int state, bool br)		{ return (state << 1) + br; }
    static int stateno(int brno)		{ return brno >> 1; }
    static bool br(int brno)			{ return brno & 1; }

    bool br_implies(int brno, int state) const;
    bool br_implies_not(int brno, int state) const;

    void run(int);
    void print();

   private:

    Classifier *_c;
    Vector<int> _dom;
    Vector<int> _dom_start;
    Vector<int> _domlist_start;

    static const int MAX_DOMLIST = 4;
    
    Classifier::Expr &expr(int state) const;
    int nexprs() const;

    static void intersect_lists(const Vector<int> &, const Vector<int> &, const Vector<int> &, int pos1, int pos2, Vector<int> &);
    static int last_common_state_in_lists(const Vector<int> &, const Vector<int> &, const Vector<int> &);
    void find_predecessors(int state, Vector<int> &) const;
    int dom_shift_branch(int brno, int to_state, int dom, int dom_end, Vector<int> *collector);
    void shift_branch(int brno);
    void calculate_dom(int state);
    
  };
  
  void bubble_sort_and_exprs();
  //bool check_path_iterative(Vector<int> &, int interested, int eventual) const;
  //bool check_path(const Vector<int> &path, Vector<int> &, int ei, int interested, int eventual, bool first, bool yet) const;
  //int check_path(int, bool) const;
  //void drift_expr(int);

  friend class DominatorOptimizer;
  
};

#endif

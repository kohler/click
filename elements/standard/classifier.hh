#ifndef CLASSIFIER_HH
#define CLASSIFIER_HH
#include "element.hh"

/*
 * =c
 * Classifier(pattern1, ..., patternN)
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
 * The patterns are scanned in order, and the packet is sent
 * to the output corresponding to the first matching pattern.
 * Thus more specific patterns should come before less
 * specific ones.
 *
 * =e
 * For example,
 *
 * = Classifier(12/0806 20/0001,
 * =            12/0806 20/0002,
 * =            12/0800,
 * =            -);
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
 * = 0  12/08060000%ffff0000  yes->step 1  no->step 3
 * = 1  20/00010000%ffff0000  yes->[0]  no->step 2
 * = 2  20/00020000%ffff0000  yes->[1]  no->[3]
 * = 3  12/08000000%ffff0000  yes->[2]  no->[3]
 * = safe length 22
 * = alignment offset 0
 */

class Classifier : public Element {
  
  struct Expr {
    int offset;
    union {
      unsigned char c[4];
      unsigned u;
    } mask;
    union {
      unsigned char c[4];
      unsigned u;
    } value;
    int yes;
    int no;
  };
  
  struct Spread {
    int _length;
    unsigned *_urelevant;
    unsigned *_uvalue;
    Spread();
    Spread(const Spread &);
    Spread &operator=(const Spread &);
    ~Spread();
    int grow(int);
    int add(const Expr &);
    bool conflicts(const Expr &) const;
    bool alw_implies_match(const Expr &) const;
    bool nev_implies_no_match(const Expr &) const;
    void alw_combine(const Spread &);
    void nev_combine(const Spread &alw, const Spread &nev,
		     const Spread &cur_alw);
  };
  
  Vector<Expr> _exprs;
  int _output_everything;
  unsigned _safe_length;
  unsigned _align_offset;

  int drift_one_edge(const Spread &, const Spread &, int) const;
  void handle_vertex(int, Vector<Spread *> &, Vector<Spread *> &,
		     Vector<int> &);
  void drift_edges();
  void unaligned_optimize();
  void remove_unused_states();
  void optimize_exprs(ErrorHandler *);
  
  static String program_string(Element *, void *);
  
  void length_checked_push(Packet *);
  
 public:
  
  Classifier();
  ~Classifier();
  
  const char *class_name() const		{ return "Classifier"; }
  Processing default_processing() const		{ return PUSH; }
  
  Classifier *clone() const;
  int configure(const String &, ErrorHandler *);
  void add_handlers(HandlerRegistry *);
  
  void push(int port, Packet *);
  
};

#endif

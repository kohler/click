#ifndef CLICK_FASTCLASSIFIER_HH
#define CLICK_FASTCLASSIFIER_HH
#include <click/vector.hh>
#include <click/string.hh>
class ElementClassT;

struct Classifier_Insn {
  int yes;
  int no;
  int offset;
  union {
    unsigned char c[4];
    unsigned u;
  } mask;
  union {
    unsigned char c[4];
    unsigned u;
  } value;
};

struct Classifier_Program {
  int safe_length;
  int output_everything;
  int align_offset;
  int noutputs;
  Vector<Classifier_Insn> program;
  int type;
  ElementClassT *eclass;
  Vector<String> handler_names;
  Vector<String> handler_values;
  const String &handler_value(const String &name) const;
};

bool operator==(const Classifier_Insn &, const Classifier_Insn &);
bool operator!=(const Classifier_Insn &, const Classifier_Insn &);

bool operator==(const Classifier_Program &, const Classifier_Program &);
bool operator!=(const Classifier_Program &, const Classifier_Program &);

int add_classifier_type(const String &name, int guaranteed_packet_length,
	void (*checked_body)(const Classifier_Program &, StringAccum &),
	void (*unchecked_body)(const Classifier_Program &, StringAccum &),
	void (*push_body)(const Classifier_Program &, StringAccum &));
void add_interesting_handler(const String &name);

#endif

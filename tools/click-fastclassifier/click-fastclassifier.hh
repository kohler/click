#ifndef CLICK_FASTCLASSIFIER_HH
#define CLICK_FASTCLASSIFIER_HH
#include <click/vector.hh>
#include <click/string.hh>
class ElementClassT;

struct Classifier_Insn {
    int offset;
    bool short_output;
    int j[2];
    union {
	unsigned char c[4];
	uint32_t u;
    } mask;
    union {
	unsigned char c[4];
	uint32_t u;
    } value;

    int required_length() const {
	if (!mask.u)
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

    static void write_branch(int branch, const String &label_prefix,
			     StringAccum &sa);
    void write_state(int state, bool check_length, bool take_short,
		     const String &data, const String &label_prefix,
		     StringAccum &sa) const;
};

struct Classifier_Program {
    int safe_length;
    int unsafe_length_output_everything;
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

int add_classifier_type(const String &name,
	void (*match_body)(const Classifier_Program &, StringAccum &sa),
	void (*more)(const Classifier_Program &, const String &type_name, StringAccum &header_sa, StringAccum &source_sa));
void add_interesting_handler(const String &name);

#endif

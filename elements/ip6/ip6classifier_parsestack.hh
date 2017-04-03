#ifndef CLICK_IP6CLASSIFIER_PARSESTACK_HH
#define CLICK_IP6CLASSIFIER_PARSESTACK_HH
#include <click/config.h>
#include <click/vector.hh>
#include <click/error.hh>
#include "ip6classifier_tokens.hh"
CLICK_DECLS

namespace ip6classification {
/* for documentation see ip6classifier_parsestack.cc */
int create_negated_node(ASTNode *original_node, ASTNode &negated_node);

class ParseStack {
public:
    int push_on_stack_and_possibly_evaluate(Token *token, ErrorHandler *errh);
    int get_AST(AST &ast, ErrorHandler *errh);
private:
    int evaluate_end_of_line_version(ErrorHandler *errh);
    int evaluate_parenthesis_version(ErrorHandler *errh);
    int evaluate_common_part(int first_token_location, int last_token_location, bool is_called_by_parenthesis_version, ErrorHandler *errh);
    AST ast;
    Vector<Token*> stack;   // a vector simulating a stack
};
};

CLICK_ENDDECLS
#endif /* CLICK_IP6CLASSIFIER_PARSESTACK_HH */

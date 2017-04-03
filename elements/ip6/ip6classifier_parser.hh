#ifndef CLICK_IP6CLASSIFIER_PARSER_HH
#define CLICK_IP6CLASSIFIER_PARSER_HH
#include <click/config.h>
#include <click/vector.hh>
#include <click/string.hh>
#include <click/error.hh>
#include "ip6classifier_tokens.hh"
#include "ip6classifier_tokens_ip6.hh"
#include "ip6classifier_tokens_icmp.hh"
#include "ip6classifier_tokens_tcp.hh"
#include "ip6classifier_parsestack.hh"
CLICK_DECLS

namespace ip6classification {

/*
 * @brief This class represents a Parser that is used to parse a to be parsed list of tokens, and transform it into an AST.
 * This specific Parser is the Parser associated with the IPFilter class.
 * For more information on how the ASTs might look like, go the the parser function AST parse().
 */
class Parser {
public:
    Parser(Vector<Token*> to_be_processed_tokens);
    ~Parser();
    int parse(AST& ast, ErrorHandler *errh);
private:
    Vector<Token*> to_be_processed_tokens;
    ParseStack parse_stack;
};

}
CLICK_ENDDECLS
#endif /* CLICK_IP6CLASSIFIER_PARSER_HH */

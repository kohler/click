#ifndef CLICK_IP6CLASSIFIER_LEXER_HH
#define CLICK_IP6CLASSIFIER_LEXER_HH

#include <click/config.h>
#include <click/vector.hh>
#include <click/string.hh>
#include <click/error.hh>
#include "ip6classifier_tokens.hh"
#include "ip6classifier_tokens_ip6.hh"
#include "ip6classifier_tokens_icmp.hh"
#include "ip6classifier_tokens_tcp.hh"
#include "ip6classifier_tokens_tcp_udp.hh"
#include "ip6classifier_factories.hh"
CLICK_DECLS

namespace ip6classification {
/* for documentation see ip6filterlexer.cc */
int is_word_an_operator(String word, Operator& an_operator);

/*
 * @brief This class represents a Lexer that is used to lex a to be lexed string and split it up into a list of tokens tokens.
 * This specific Lexer is the Lexer associated with the IPFilter class.
 * For more information on how the tokens might look like, go the the lex function Vector<String> lex().
 */
class Lexer {
public:
    Lexer(String to_be_lexed_string);
    ~Lexer();
    int lex(Vector<Token*>& tokens, ErrorHandler *errh);
private:
    int skip_blanks(int i);
    int read_word(int i, String& read_word);
    void skip_blanks_and_read_word(int& i, String& word_to_be_read, const String error); 
    String to_be_lexed_string;
};

};

CLICK_ENDDECLS
#endif /* CLICK_IP6CLASSIFIER_LEXER_HH */

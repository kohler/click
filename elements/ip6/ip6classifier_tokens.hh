#ifndef CLICK_IP6CLASSIFIER_TOKENS_HH
#define CLICK_IP6CLASSIFIER_TOKENS_HH

#include <click/config.h>
#include <clicknet/ip.h>
#include <clicknet/ip6.h>
#include <clicknet/ether.h>
#include "ip6classifier_AST.hh"
#include "ip6classifier_operator.hh"
CLICK_DECLS

namespace ip6classification {
/*
 * @brief This class represents a Token.
 * This specific Lexer is the Lexer associated with the IPFilter class.
 * For more information on how the tokens might look like, go the the lex function Vector<String> lex().
 */
class Token : public ASTNode {
public:
    virtual ~Token() {}
    virtual void print_name() = 0;
    virtual void print() {
        ASTNode::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) = 0;
};

/*
 * @brief A CombinerToken is are Tokens such as and, or.
 * CombinerTokens are used to connected other Tokens with each other. They are used to build up complicated 
 * filter expressions, like "host 10.5.5.7 and dst host 10.5.2.3 or host 12.2.7.9".
 * Oftentimes they also get preceeded by a parenthesistoken, to make the order of operations clear.
 * An example, "10.5.5.7 and dst host 10.5.2.2 (or 12.2.7.9)"
 */
class CombinerToken : public Token {
public:
    virtual ~CombinerToken() {}
    virtual void print_name() = 0;
    virtual void print() {
        Token::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) = 0;
};

/*
 * @brief A specific type of Combiner token, this Combiner token brings two Primitives together and returns true if they both are true.
 */
class AndCombinerToken : public CombinerToken {
public:
    AndCombinerToken() {}
    virtual ~AndCombinerToken() {}
    virtual void print_name() {
        click_chatter("AndCombinerToken");
    }
    virtual void print() {
        click_chatter("We encountered an AndCombinerToken");
        CombinerToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        return left_child->check_whether_packet_matches(packet) && right_child->check_whether_packet_matches(packet);
    }
};

/*
 * @brief A specific type of Combiner token, this Combiner token brings two Primitives together and returns true if one of both are true.
 */
class OrCombinerToken : public CombinerToken {
public:
    OrCombinerToken() {}
    virtual ~OrCombinerToken() { }
    virtual void print_name() {
        click_chatter("OrCombinerToken");
    }
    virtual void print() {
        click_chatter("We encountered an OrCombinerToken");
        CombinerToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        return left_child->check_whether_packet_matches(packet) || right_child->check_whether_packet_matches(packet);
    }
};

/*
 * @brief A token denoting a left parenthesis, this shouldn't contain an check_whether_packet_matches(Packet* packet) function
 */
class LeftParenthesisToken : public Token {
public:
    LeftParenthesisToken(bool is_preceded_by_not_keyword) {
        this->is_preceded_by_not_keyword = is_preceded_by_not_keyword;
    }    
    virtual ~LeftParenthesisToken() {}

    virtual void print_name() {
        click_chatter("LeftParenthesisToken");
    }
    virtual void print() {
        click_chatter("We encountered a LeftParenthesisToken");
        Token::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        (void) packet;  // too show that packet is used
        return true;
    }

    bool is_preceded_by_not_keyword;
};

/*
 * @brief A token denoting a right parenthesis, this shoudln't contain an check_whether_packet_matches(Packet* packet) function
 */
class RightParenthesisToken : public Token {
public:
    RightParenthesisToken() {}
    virtual ~RightParenthesisToken() {}
    virtual void print_name() {
        click_chatter("RightParenthesisToken");
    }
    virtual void print() {
        click_chatter("We encountered a RightParenthesisToken");
        Token::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        (void) packet;  // to show that packet is used
        return true;
    }
};
/*
 * @brief A Token representing a tcpdumplike syntax primitive.
 * In tcpdumplike syntax filter expressions, the filter expression is built up out of multiple primitives, combined by ands, 
 * and ors. An example, in the filter expression "host 10.2.5.9 or dst host 10.9.5.7 and src port 2000", the primitives were
 * "host 10.2.5.9", "dst host 10.9.5.7" and "src port 2000". And as we can see, they were combined with each other by "and"
 * and "or" symbols.
 *
 * Next to that, each primitive token can also contain an operator (==, !=, >=, >, <, <=) right after his keyword and right
 * before the data. Examples are "host >= 12.5.7.9" and "src port < 1500". Here the keywords were "host" in the first example
 * and "src port" in the second example. The data were respectivelly "12.5.7.9" and "1500". When no operator was given (as
 * in the first series of examples) the "==" operator is assumed. So writing "host 10.2.5.9" is the same as writing 
 * "host == 10.2.5.9".
 */
class PrimitiveToken : public Token {
public:
    PrimitiveToken(bool is_preceded_by_not_keyword, Operator an_operator) : is_preceded_by_not_keyword(is_preceded_by_not_keyword), an_operator(an_operator) { }
    virtual ~PrimitiveToken() {}
    
    /*
     * @brief Clones the node but inverts the not keyword seen value.
     * If the "not keyword" was seen, the clone will indicate that the keyword was not seen. If the "not keyword" was not seen, the clone will indicate that the keyword was seen.
     * @return A clone of the node but with the not keyword seen value inverted.
     */
    virtual PrimitiveToken* clone_and_invert_not_keyword_seen() = 0;

    virtual void print_name() = 0;
    virtual void print() {
        Token::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) = 0;
    
    const bool is_preceded_by_not_keyword;
    
    const Operator an_operator;   // When no token was given by the user, the EQUALITY operator is assumed and assigned internally (namely here).
    
    /*
     * @brief This function inverses the given expression if this Primitive was preceded by the not keyword
     * This function is called before returning from a check_whether_a_packet_matches(Packet *packet) method. 
     * @param expression An expression that possibly needs to be reversed
     * @return Leaves the expresssion unaltered when nothing when no not keyword was seen or gives back the reverse expression
     */     
    inline bool take_inverse_on_not(bool expression) {  // TODO unused at the moment
        if (is_preceded_by_not_keyword) {
            return !expression;
        } else {
            return expression;
        }
    }
};

/*
 * @brief A special PrimitiveToken that always returns true.
 * To use this token, just write 'true'. This token can be used in a filterexpression as in "src host 10.5.6.9 or true", which is an expression that will always match
 */
class TruePrimitiveToken : public PrimitiveToken {
public:
    TruePrimitiveToken(bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) { }
    virtual ~TruePrimitiveToken() { }
    virtual TruePrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new TruePrimitiveToken(!this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("TruePrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered a TruePrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        (void) packet;  // Remove the warning
        return take_inverse_on_not(true);
    }   
};

/*
 * @brief A special PrimitiveToken that always returns false.
 * To use this token, just write 'false'. This token can be used in a filterexpression as in "src host 10.5.6.9 or false", which is an expression that will always fail to match
 */
class FalsePrimitiveToken : public PrimitiveToken {
public:
    FalsePrimitiveToken(bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) { }
    virtual ~FalsePrimitiveToken() { }
    virtual FalsePrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new FalsePrimitiveToken(!this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("FalsePrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered a FalsePrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        (void) packet;  // Remove the warning
        return take_inverse_on_not(false);
    }   
};

class EndOfLineToken : public Token {
public:
    EndOfLineToken() { }
    virtual ~EndOfLineToken() {}
    
    virtual void print_name() {
        click_chatter("EndOfLineToken");
    }
    virtual void print() {
        click_chatter("We encountered an EndOfLineToken");
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        (void) packet;  // to show that the packed is used
        return true;
    }
};
};

CLICK_ENDDECLS
#endif /* CLICK_IP6CLASSIFIER_TOKENS_HH */

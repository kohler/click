#ifndef IP6CLASSIFIER_TOKENS_TCP
#define IP6CLASSIFIER_TOKENS_TCP

#include "ip6classifier_tokens.hh"
#include "ip6classifier_operator.hh"
#include <clicknet/tcp.h>
#include "ip6helpers.hh"

CLICK_DECLS

namespace ip6classification {

enum TCPOptionName {
    SYN,
    FIN,
    ACK,
    RST,
    PSH,
    URG
};

/*
 * @brief A Token representing an TCP option name, a Primitive that does not hold an Opertator.
 * Whenever we see in our text something of the form "tcp opt" followed by a TCP option. The TCP Options are 'syn', 'fin', 'ack', 'rst', 'psh' and 'urg'.
 * Examples of TCPOptionNamePrimitiveTokens are "tcp opt fin" and "tcp opt ack".
 */
class TCPOptionNamePrimitiveToken : public PrimitiveToken {
public:
    TCPOptionNamePrimitiveToken(TCPOptionName tcp_option_name, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), tcp_option_name(tcp_option_name) { }
    virtual ~TCPOptionNamePrimitiveToken() {}
    
    virtual TCPOptionNamePrimitiveToken* clone_and_invert_not_keyword_seen() {
        if (this->is_preceded_by_not_keyword) {
            return new TCPOptionNamePrimitiveToken(this->tcp_option_name, false, this->an_operator); // not keyword seen is now inverted to false
        } else {
            return new TCPOptionNamePrimitiveToken(this->tcp_option_name, true, this->an_operator);  // not keyword seen is now inverted to true
        }
    }
    
    virtual bool check_whether_packet_matches(Packet *packet) {
        try {
            if (ip6::get_higher_layer_protocol(packet) == 6) {  // 6 means IT IS a TCP packet            
                click_tcp* tcp_header_of_this_packet = (click_tcp*) packet->tcp_header();
                  
                switch (tcp_option_name) {
                    case SYN:
                        return ((tcp_header_of_this_packet->th_flags & TH_SYN) >> 1);
                    case FIN:
                        return (tcp_header_of_this_packet->th_flags & TH_FIN);
                    
                    case ACK:
                        return ((tcp_header_of_this_packet->th_flags & TH_ACK) >> 4);     // ALWAYS ADD HOW MUCH YOU NEED TO SHIFT TO THE RIGHT TO GET BIT INTO GOOD SPOT
                                                                                        // In this particular case move 4 to the right
                    case RST:
                        return ((tcp_header_of_this_packet->th_flags & TH_RST) >> 2);
                        
                    case PSH:
                        return ((tcp_header_of_this_packet->th_flags & TH_PUSH ) >> 3);
                        
                    default:   // It is an URG
                        return ((tcp_header_of_this_packet->th_flags & TH_URG) >> 5);
                }
            } else {
                return false;   // It is NOT A TCP packet           
            }
        } catch (String error) {
            click_chatter(error.c_str());   // report the error that occured in the terminal
            return false;
        }
    }
    
    virtual void print_name() {
        click_chatter("TCPOptionNamePrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an TCPOptionNamePrimitiveToken");
        PrimitiveToken::print();
    }
    
private:
    const TCPOptionName tcp_option_name;
};

class TCPReceiveWindowLengthPrimitiveToken : public PrimitiveToken {
public:
    TCPReceiveWindowLengthPrimitiveToken(uint16_t window_length, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), window_length(window_length) {}
    virtual ~TCPReceiveWindowLengthPrimitiveToken() { }
    
    virtual TCPReceiveWindowLengthPrimitiveToken* clone_and_invert_not_keyword_seen() {
        if (this->is_preceded_by_not_keyword) {
            return new TCPReceiveWindowLengthPrimitiveToken(this->window_length, false, this->an_operator); // not keyword seen is now inverted to false
        } else {
            return new TCPReceiveWindowLengthPrimitiveToken(this->window_length, true, this->an_operator);  // not keyword seen is now inverted to true
        }
    }

    virtual bool check_whether_packet_matches(Packet *packet) {
        try {
            if (ip6::get_higher_layer_protocol(packet) == 6) {  // 6 means IT IS a TCP packet
                click_tcp* tcp_header_of_this_packet = (click_tcp*) packet->tcp_header();
                
                switch (an_operator) {
                    case EQUALITY:
                        return take_inverse_on_not(tcp_header_of_this_packet->th_win == htons(window_length)); // normally we simply give back the answer of the equality but when the not
                                                                                                        // keyword was seen we give back the inverse of this
                    case INEQUALITY:
                        return take_inverse_on_not(tcp_header_of_this_packet->th_win != htons(window_length));
                    
                    case GREATER_THAN:
                        return take_inverse_on_not(tcp_header_of_this_packet->th_win > htons(window_length));
                        
                    case LESS_THAN:
                        return take_inverse_on_not(tcp_header_of_this_packet->th_win < htons(window_length));
                        
                    case GREATER_OR_EQUAL_THAN:
                        return take_inverse_on_not(tcp_header_of_this_packet->th_win >= htons(window_length));
                        
                    default:   // It is an LESS_OR_EQUAL_THAN
                        return take_inverse_on_not(tcp_header_of_this_packet->th_win <= htons(window_length));
                }
            } else {
                return false;   // It is NOT A TCP packet
            }
        } catch (String error) {
            click_chatter(error.c_str());   // report the error that occured in the terminal
            return false;
        }
    }

    virtual void print_name() {
        click_chatter("TCPReceiveWindowLengthPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an TCPReceiveWindowLengthPrimitiveToken");
        PrimitiveToken::print();
    }
    
private:
    uint16_t window_length;
};

};

CLICK_ENDDECLS

#endif /* IP6CLASSIFIER_TOKENS_TCP */

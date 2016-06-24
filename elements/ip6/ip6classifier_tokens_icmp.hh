#ifndef IP6CLASSIFIER_TOKENS_ICMP
#define IP6CLASSIFIER_TOKENS_ICMP

#include "ip6classifier_tokens.hh"
#include <clicknet/icmp.h>
CLICK_DECLS

namespace ip6classification {

/*
 * @brief A Token representing an ICMP type, a special kind of Primitive
 * Whenever we see in our text something of the form "icmp type" followed by a number between 0 and 255, such as "icmp type 12" or 
 * "icmp type 209" we replace it by a ICMPTypePrimitiveToken.
 */
class ICMPTypePrimitiveToken : public PrimitiveToken {
public:
    ICMPTypePrimitiveToken(uint8_t icmp_type, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), icmp_type(icmp_type) { }
    virtual ~ICMPTypePrimitiveToken() {}
    
    virtual ICMPTypePrimitiveToken* clone_and_invert_not_keyword_seen() {
        if (this->is_preceded_by_not_keyword) {
            return new ICMPTypePrimitiveToken(this->icmp_type, false, this->an_operator); // not keyword seen is now inverted to false
        } else {
            return new ICMPTypePrimitiveToken(this->icmp_type, true, this->an_operator);  // not keyword seen is now inverted to true
        }
    }
    
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_icmp* icmp_header_of_this_packet = (click_icmp*) packet->icmp_header();   
    
        switch (an_operator) {
            case EQUALITY:
                return take_inverse_on_not((icmp_header_of_this_packet->icmp_type >> 8) == icmp_type); // normally we simply give back the answer of the equality but when the not
                                                                                                // keyword was seen we give back the inverse of this
            case INEQUALITY:
                return take_inverse_on_not((icmp_header_of_this_packet->icmp_type >> 8) != icmp_type);
            
            case GREATER_THAN:
                return take_inverse_on_not((icmp_header_of_this_packet->icmp_type >> 8) > icmp_type);
                
            case LESS_THAN:
                return take_inverse_on_not((icmp_header_of_this_packet->icmp_type >> 8) < icmp_type);
                
            case GREATER_OR_EQUAL_THAN:
                return take_inverse_on_not((icmp_header_of_this_packet->icmp_type >> 8) >= icmp_type);
                
            default:   // It is an LESS_OR_EQUAL_THAN
                return take_inverse_on_not((icmp_header_of_this_packet->icmp_type >> 8) <= icmp_type);
        }
    }
    
    virtual void print_name() {
        click_chatter("ICMPTypePrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an ICMPTypePrimitiveToken");
        PrimitiveToken::print();
    }
    
    
private:
    const uint8_t icmp_type;
};

};

CLICK_ENDDECLS

#endif /* IP6CLASSIFIER_TOKENS_ICMP */

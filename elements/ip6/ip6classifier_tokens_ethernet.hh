#ifndef IP6CLASSIFIER_TOKENS_ETHERNET
#define IP6CLASSIFIER_TOKENS_ETHERNET

#include "ip6classifier_tokens.hh"
#include "ip6classifier_operator.hh"

CLICK_DECLS

namespace ip6classification {

/*
 * @brief A token representing an Ethernet Host Primitive, a special kind of Primitive
 * Whenever we see in our text something of the form "host" followed by an Ethernet address, such as "host 00:0a:95:9d:68:16" or
 * "host 08:56:27:6f:2b:9c" we replace it by a EtherHostPrimitiveToken
 */ 
class EtherHostPrimitiveToken : public PrimitiveToken {
public:
    /*
     * @brief constructor, Token can only be created by giving an Ethernet address to create the Token with. ether_address[6] contains an Ethernet address.
     * @param ether_address contains an Ethernet address.
     * @param is_preceded_by_not_keyword true when this token was preceded by a not keyword, false otherwise
     * @param an_operator contains an operator that could be found between the keyword and the data. If nothing was found between the keyword and the data this keyword must be given the value EQUALITY_OPERATOR.     
     */
    EtherHostPrimitiveToken(uint8_t ether_address[6], bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) {
        for(int i = 0; i < 6; i++) {
            this->ether_address[i] = ether_address[i];
        }
    }
    virtual ~EtherHostPrimitiveToken() {}
    /*
     * @brief Clones this EtherHostPrimitiveToken but inverts the not keyword seen value.
     * If the "not keyword" was seen, the clone will indicate that the keyword was not seen. If the "not keyword" was not seen, the clone will indicate that the keyword was seen.
     * @return A clone of the node but with the not keyword seen value inverted.
     */
    virtual EtherHostPrimitiveToken* clone_and_invert_not_keyword_seen() {
        if (this->is_preceded_by_not_keyword == true) {
            return new EtherHostPrimitiveToken(this->ether_address, false, this->an_operator);
        } else {
            return new EtherHostPrimitiveToken(this->ether_address, true, this->an_operator);
        }
    }
    
    virtual void print_name() {
        click_chatter("EtherHostPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an EtherHostPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_ether *ether_header_of_this_packet = (click_ether*) packet->mac_header();
        if (an_operator == EQUALITY) {
            for(int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_dhost[i] != ether_address[i] || ether_header_of_this_packet->ether_shost[i] != ether_address[i]) {
                    return take_inverse_on_not(false);
                }
            }
            return take_inverse_on_not(true);
        } else if (an_operator == INEQUALITY) {
            for(int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_dhost[i] == ether_address[i] || ether_header_of_this_packet->ether_shost[i] == ether_address[i]) {
                    return take_inverse_on_not(false);
                }
            }        
            return take_inverse_on_not(true);
        } else {
            // Do we need to support >=, >, <, <= for this? Seems a bit weird/odd.
        }
        return true;
    }
private:
    uint8_t	ether_address[6];   // the Ethernet address (= 6 times 1 byte)
};

/*
 * @brief A token representing an Ethernet Src Host Primitive, a special kind of Primitive
 * Whenever we see in our text something of the form "src host" followed by an Ethernet address, such as "src host 00:0a:95:9d:68:16" or
 * "src host 08:56:27:6f:2b:9c" we replace it by a EtherSrcHostPrimitiveToken
 */ 
class EtherSrcHostPrimitiveToken : public PrimitiveToken {
public:
    /*
     * @brief constructor, Token can only be created by giving an Ethernet address to create the Token with. ether_address[6] contains an Ethernet address.
     * @param ether_address contains an Ethernet address.
     * @param is_preceded_by_not_keyword true when this token was preceded by a not keyword, false otherwise
     * @param an_operator contains an operator that could be found between the keyword and the data. If nothing was found between the keyword and the data this keyword must be given the value EQUALITY_OPERATOR.     
     */
    EtherSrcHostPrimitiveToken(uint8_t ether_address[6], bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) {
        for(int i = 0; i < 6; i++) {
            this->ether_address[i] = ether_address[i];
        }
    }
    virtual ~EtherSrcHostPrimitiveToken() {}    
    /*
     * @brief Clones this EtherSrcHostPrimitiveToken but inverts the not keyword seen value.
     * If the "not keyword" was seen, the clone will indicate that the keyword was not seen. If the "not keyword" was not seen, the clone will indicate that the keyword was seen.
     * @return A clone of the node but with the not keyword seen value inverted.
     */
    virtual EtherSrcHostPrimitiveToken* clone_and_invert_not_keyword_seen() {
        if (this->is_preceded_by_not_keyword == true) {
            return new EtherSrcHostPrimitiveToken(this->ether_address, false, this->an_operator);
        } else {
            return new EtherSrcHostPrimitiveToken(this->ether_address, true, this->an_operator);
        }
    }    
    
    virtual void print_name() {
        click_chatter("EtherSrcHostPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an EtherSrcHostPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_ether *ether_header_of_this_packet = (click_ether*) packet->mac_header();
        if (an_operator == EQUALITY) {
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_shost[i] != ether_address[i]) {
                    return take_inverse_on_not(false);
                }
            }
            return take_inverse_on_not(true);
        } else if (an_operator == INEQUALITY) {
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_shost[i] == ether_address[i]) {
                    return take_inverse_on_not(false);
                }
            }        
            return take_inverse_on_not(true);
        } else if (an_operator == GREATER_THAN) {
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_shost[i] > ether_address[i]) {
                    return take_inverse_on_not(true);
                } else if (ether_header_of_this_packet->ether_shost[i] == ether_address[i]) {
                    // Go to the next value to determine whether it the packet value is greater than the given address, the first bits were equal so we need to check deeper
                    // to make the decission.
                } else {
                    return take_inverse_on_not(false);  // it is less than
                }
            }
            return take_inverse_on_not(false);          // they are equal so return false
        } else if (an_operator == LESS_THAN) {
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_shost[i] < ether_address[i]) {
                    return take_inverse_on_not(true);
                } else if (ether_header_of_this_packet->ether_shost[i] == ether_address[i]) {
                    // Go to the next value to determine whether it the packet value is less than the given address, the first bits were equal so we need to check deeper
                    // to make the decission.
                } else {
                    return take_inverse_on_not(false);  // it is greater than
                }
            }
            return take_inverse_on_not(false);          // they are equal so return false
        } else if (an_operator == GREATER_OR_EQUAL_THAN) {
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_shost[i] > ether_address[i]) {
                    return take_inverse_on_not(true);
                } else if (ether_header_of_this_packet->ether_shost[i] == ether_address[i]) {
                    // Go to the next value to determine whether it the packet value is greater than or equal to the given address, the first bits were equal so we need to check deeper
                    // to make the decission.
                } else {
                    return take_inverse_on_not(false);  // it is less than
                }
            }
            return take_inverse_on_not(true);          // they are equal so return true
        } else {    // it is LESS_OR_EQUAL_THAN
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_shost[i] < ether_address[i]) {
                    return take_inverse_on_not(true);
                } else if (ether_header_of_this_packet->ether_shost[i] == ether_address[i]) {
                    // Go to the next value to determine whether it the packet value is less than or equal to the given address, the first bits were equal so we need to check deeper
                    // to make the decission.
                } else {
                    return take_inverse_on_not(false);  // it is greater than
                }
            }
            return take_inverse_on_not(true);          // they are equal so return true        
        }
    }
private:
    uint8_t	ether_address[6];   // the Ethernet address (= 6 times 1 byte)
};

/*
 * @brief A token representing an Ethernet Dst Host Primitive, a special kind of Primitive
 * Whenever we see in our text something of the form "dst host" followed by an Ethernet address, such as "dst host 00:0a:95:9d:68:16" or
 * "dst host 08:56:27:6f:2b:9c" we replace it by a EtherDstHostPrimitiveToken
 */ 
class EtherDstHostPrimitiveToken : public PrimitiveToken {
public:
    /*
     * @brief constructor, Token can only be created by giving an Ethernet address to create the Token with. ether_address[6] contains an Ethernet address.
     * @param ether_address contains an Ethernet address.
     */
    EtherDstHostPrimitiveToken(uint8_t ether_address[6], bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) {
        for(int i = 0; i < 6; i++) {
            this->ether_address[i] = ether_address[i];
        }
    }
    virtual ~EtherDstHostPrimitiveToken() {}        
    /*
     * @brief Clones this EtherDstHostPrimitiveToken but inverts the not keyword seen value.
     * If the "not keyword" was seen, the clone will indicate that the keyword was not seen. If the "not keyword" was not seen, the clone will indicate that the keyword was seen.
     * @return A clone of the node but with the not keyword seen value inverted.
     */
    virtual EtherDstHostPrimitiveToken* clone_and_invert_not_keyword_seen() {
        if (this->is_preceded_by_not_keyword == true) {
            return new EtherDstHostPrimitiveToken(this->ether_address, false, this->an_operator);
        } else {
            return new EtherDstHostPrimitiveToken(this->ether_address, true, this->an_operator);
        }
    }     
    
    virtual void print_name() {
        click_chatter("EtherDstHostPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an EtherDstHostPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_ether *ether_header_of_this_packet = (click_ether*) packet->mac_header();
        if (an_operator == EQUALITY) {
            for(int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_dhost[i] != ether_address[i]) {
                    return take_inverse_on_not(false);
                }
            }
            return take_inverse_on_not(true);
        } else if (an_operator == INEQUALITY) {
            for(int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_dhost[i] == ether_address[i]) {
                    return take_inverse_on_not(false);
                }
            }        
            return take_inverse_on_not(true);
        } else if (an_operator == GREATER_THAN) {
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_dhost[i] > ether_address[i]) {
                    return take_inverse_on_not(true);
                } else if (ether_header_of_this_packet->ether_dhost[i] == ether_address[i]) {
                    // Go to the next value to determine whether it the packet value is greater than the given address, the first bits were equal so we need to check deeper
                    // to make the decission.
                } else {
                    return take_inverse_on_not(false);  // it is less than
                }
            }
            return take_inverse_on_not(false);          // they are equal so return false
        } else if (an_operator == LESS_THAN) {
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_dhost[i] < ether_address[i]) {
                    return take_inverse_on_not(true);
                } else if (ether_header_of_this_packet->ether_dhost[i] == ether_address[i]) {
                    // Go to the next value to determine whether it the packet value is less than the given address, the first bits were equal so we need to check deeper
                    // to make the decission.
                } else {
                    return take_inverse_on_not(false);  // it is greater than
                }
            }
            return take_inverse_on_not(false);          // they are equal so return false
        } else if (an_operator == GREATER_OR_EQUAL_THAN) {
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_dhost[i] > ether_address[i]) {
                    return take_inverse_on_not(true);
                } else if (ether_header_of_this_packet->ether_dhost[i] == ether_address[i]) {
                    // Go to the next value to determine whether it the packet value is greater than or equal to the given address, the first bits were equal so we need to check deeper
                    // to make the decission.
                } else {
                    return take_inverse_on_not(false);  // it is less than
                }
            }
            return take_inverse_on_not(true);          // they are equal so return true
        } else {    // it is LESS_OR_EQUAL_THAN
            for (int i = 0; i < 6; i++) {
                if (ether_header_of_this_packet->ether_dhost[i] < ether_address[i]) {
                    return take_inverse_on_not(true);
                } else if (ether_header_of_this_packet->ether_dhost[i] == ether_address[i]) {
                    // Go to the next value to determine whether it the packet value is less than or equal to the given address, the first bits were equal so we need to check deeper
                    // to make the decission.
                } else {
                    return take_inverse_on_not(false);  // it is greater than
                }
            }
            return take_inverse_on_not(true);          // they are equal so return true        
        }
    }
private:
    uint8_t	ether_address[6];   // the Ethernet address (= 6 times 1 byte)
};

};

CLICK_ENDDECLS

#endif /* IP6CLASSIFICATION_TOKENS_ETHERNET */

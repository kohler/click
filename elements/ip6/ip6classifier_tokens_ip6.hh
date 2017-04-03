#ifndef IP6CLASSIFIER_TOKENS_IP6
#define IP6CLASSIFIER_TOKENS_IP6

#include "ip6classifier_tokens.hh"
#include "ip6helpers.hh"
#include <click/ip6address.hh>

CLICK_DECLS

namespace ip6classification {
/*
 * @brief A token representing an IPv6 Host Primitive, a special kind of Primitive
 * Whenever we see in our text something of the form "host" followed by an IPv6 address, such as "host 3ffe:1900:4545:3:200:f8ff:fe21:67cf" or
 * "host 2001:0db8:0a0b:12f0:0000:0000:0000:0001" we replace it by a IP6HostPrimitiveToken
 */ 
class IP6HostPrimitiveToken : public PrimitiveToken {
public:
    /*
     * @brief constructor, Token can only be created by giving an IPv6 address to create the Token with. in6_addr contains an IPv6 address.
     * @param ip6_address contains an IPv6 address.
     * @param is_preceded_by_not_keyword true when this token was preceded by a not keyword, false otherwise
     * @param an_operator contains an operator that could be found between the keyword and the data. If nothing was found between the keyword and the data this keyword must be given the value EQUALITY_OPERATOR.
     */
    IP6HostPrimitiveToken(in6_addr ip6_address, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) {
        this->ip6_address = ip6_address;
    }
    virtual ~IP6HostPrimitiveToken() {}
    /*
     * @brief Clones this IP6HostPrimitiveToken but inverts the not keyword seen value.
     * If the "not keyword" was seen, the clone will indicate that the keyword was not seen. If the "not keyword" was not seen, the clone will indicate that the keyword was seen.
     * @return A clone of the node but with the not keyword seen value inverted.
     */
    virtual IP6HostPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6HostPrimitiveToken(this->ip6_address, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6HostPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6HostPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        // First test whether the src addresses match, if yes we check_whether_packet_matches to true (because we found a match). If the src addresses don't match we have a last chance to get true,
        // that is when the dst addresses match. If nothing matches, we return false.
        
        // First try to match the source address
        uint8_t* ip6_src_address_of_this_packet = (uint8_t*) packet->network_header() + 8;
        bool ip6_src_addresses_match = true;
        for (int i = 0; i < 16; i++) {
            if (ip6_src_address_of_this_packet[i] != ip6_address.s6_addr[i]) {
                ip6_src_addresses_match = false;
                break;
            }
        }
        if (ip6_src_addresses_match) {
            if (!this->is_preceded_by_not_keyword) {
                return true;                // the src addresses matched
            } else {
                return false;
            }
        }
        
        // If that failed, try to match the destination address
        uint8_t* ip6_dst_address_of_this_packet = (uint8_t*) packet->network_header() + 24;
        for (int i = 0; i < 16; i++) {
            if (ip6_dst_address_of_this_packet[i] != ip6_address.s6_addr[i]) {
                if (!this->is_preceded_by_not_keyword) {    
                    return false;           // both the src addresses and dst addresses did not match
                } else {
                    return true;
                }
            }
        }
        
        if (!this->is_preceded_by_not_keyword) {
            return true;                    // the dst addresses matched
        } else {
            return false;
        }
    }
private:
    in6_addr ip6_address;
};

/*
 * @brief A token representing an IPv6 Src Host Primitive, a special kind of Primitive
 * Whenever we see in our text something of the form "src host" followed by an IPv6 address, such as "src host 3ffe:1900:4545:3:200:f8ff:fe21:67cf" or
 * "src host 2001:0db8:0a0b:12f0:0000:0000:0000:0001" we replace it by a IP6SrcHostPrimitiveToken
 */ 
class IP6SrcHostPrimitiveToken : public PrimitiveToken {
public:
    /*
     * @brief constructor, Token can only be created by giving an IPv6 address to create the Token with. in6_addr contains an IPv6 address.
     * @param ip6_address contains an IPv6 address.
     * @param is_preceded_by_not_keyword true when this token was preceded by a not keyword, false otherwise
     * @param an_operator contains an operator that could be found between the keyword and the data. If nothing was found between the keyword and the data this keyword must be given the value EQUALITY_OPERATOR.
     */
    IP6SrcHostPrimitiveToken(in6_addr ip6_address, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) {
        this->ip6_address = ip6_address;
    }
    virtual ~IP6SrcHostPrimitiveToken() {}
    /*
     * @brief Clones this IP6SrcHostPrimitiveToken but inverts the not keyword seen value.
     * If the "not keyword" was seen, the clone will indicate that the keyword was not seen. If the "not keyword" was not seen, the clone will indicate that the keyword was seen.
     * @return A clone of the node but with the not keyword seen value inverted.
     */
    virtual IP6SrcHostPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6SrcHostPrimitiveToken(this->ip6_address, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6SrcHostPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6SrcHostPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint8_t* ip6_dst_address_of_this_packet = (uint8_t*) packet->network_header() + 12;
        if (an_operator == EQUALITY || an_operator == INEQUALITY) { // both equality and inequality work pretty similar, that is why we take them together (to save code space)
            const bool take_negated_solution = EQUALITY ? this->is_preceded_by_not_keyword : !this->is_preceded_by_not_keyword;
            // The code is made '==' setting in mind. In case either a not preceded the keyword before '==', or no not preceded a keyword before '!=', we take the negated solution
            // e.g. with "not host == 10.5.7.2" or with "host != 10.5.7.2." we need to take the negated solution.

            for (int i = 0; i < 16; i++) {
                if (ip6_dst_address_of_this_packet[i] != ip6_address.s6_addr[i]) {
                    if (!take_negated_solution) {
                        return false;           // the dst addresses did not match
                    } else {
                        return true;
                    }
                }
            }
            if (!take_negated_solution) {
                return true;                    // the dst addresses dit match
            } else {
                return false;
            }
        } else if (an_operator == GREATER_OR_EQUAL_THAN || an_operator == LESS_THAN) { // ... again, >= and < work pretty similar (if you take negation in mind), so to save code space they were taken together
            const bool take_negated_solution = GREATER_OR_EQUAL_THAN ? this->is_preceded_by_not_keyword : !this->is_preceded_by_not_keyword;
            // The code is made '>=' setting in mind. In case either a not preceded the keyword before '>=', or no not preceded a keyword before '<', we take the negated solution
            // e.g. with "not host >= 10.5.7.2" or with "host < 10.5.7.2." we need to take the negated solution.
            for (int i = 0; i < 16; i++) {
                if (ip6_dst_address_of_this_packet[i] > ip6_address.s6_addr[i]) {
                    if (!take_negated_solution) {
                        return true;           // the dst addresses did not match
                    } else {
                        return false;
                    }
                } else if (ip6_dst_address_of_this_packet[i] == ip6_address.s6_addr[i]) { // is is equality (=), so we need to read the next symbol in order to decide
                    // read the next symbol.
                } else {            // it is less than (<)
                    if (!take_negated_solution) {
                        return false;           // the dst addresses did not match
                    } else {
                        return true;
                    }
                }
            }
            if (!take_negated_solution) {       // if everything was equal we have a match as well
                return true;                    // the dst addresses dit match
            } else {
                return false;
            }            
            
        } else {    // it is SMALLER_OR_EQUAL_THAN or GREATER THAN, ... again, <= and > work pretty similar (if you take negation in mind), so to save code space they were taken together
            const bool take_negated_solution = LESS_OR_EQUAL_THAN ? this->is_preceded_by_not_keyword : !this->is_preceded_by_not_keyword;            
            // The code is made '<=' setting in mind. In case either a not preceded the keyword before '<=', or no not preceded a keyword before '>', we take the negated solution
            // e.g. with "not host <= 10.5.7.2" or with "host > 10.5.7.2." we need to take the negated solution.       
            for (int i = 0; i < 16; i++) {
                if (ip6_dst_address_of_this_packet[i] < ip6_address.s6_addr[i]) {
                    if (!take_negated_solution) {
                        return true;           // the dst addresses did not match
                    } else {
                        return false;
                    }
                } else if (ip6_dst_address_of_this_packet[i] == ip6_address.s6_addr[i]) { // is is equality (=), so we need to read the next symbol in order to decide
                    // read the next symbol.
                } else {            // it is greater than (>)
                    if (!take_negated_solution) {
                        return false;           // the dst addresses did not match
                    } else {
                        return true;
                    }
                }
            }
            if (!take_negated_solution) {       // if everything was equal we have a match as well
                return true;                    // the dst addresses dit match
            } else {
                return false;
            }
        }    
    }
private:
    in6_addr ip6_address;
};

/*
 * @brief A token representing an IPv6 Dst Host Primitive, a special kind of Primitive
 * Whenever we see in our text something of the form "dst host" followed by an IPv6 address, such as "dst host 3ffe:1900:4545:3:200:f8ff:fe21:67cf" or
 * "dst host 2001:0db8:0a0b:12f0:0000:0000:0000:0001" we replace it by a IP6DstHostPrimitiveToken
 */ 
class IP6DstHostPrimitiveToken : public PrimitiveToken {
public:
    /*
     * @brief constructor, Token can only be created by giving an IPv6 address to create the Token with. in6_addr contains an IPv6 address.
     * @param ip6_address contains an IPv6 address.
     * @param is_preceded_by_not_keyword true when this token was preceded by a not keyword, false otherwise
     * @param an_operator contains an operator that could be found between the keyword and the data. If nothing was found between the keyword and the data this keyword must be given the value EQUALITY_OPERATOR.     
     */
    IP6DstHostPrimitiveToken(in6_addr ip6_address, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) {
        this->ip6_address = ip6_address;
    }
    virtual ~IP6DstHostPrimitiveToken() {}
    /*
     * @brief Clones this IP6DstHostPrimitiveToken but inverts the not keyword seen value.
     * If the "not keyword" was seen, the clone will indicate that the keyword was not seen. If the "not keyword" was not seen, the clone will indicate that the keyword was seen.
     * @return A clone of the node but with the not keyword seen value inverted.
     */
    virtual IP6DstHostPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6DstHostPrimitiveToken(this->ip6_address, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6DstHostPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6DstHostPrimitiveToken");
        PrimitiveToken::print();
    }        
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint8_t* ip6_dst_address_of_this_packet = (uint8_t*) packet->network_header() + 24;
        if (an_operator == EQUALITY || an_operator == INEQUALITY) { // both equality and inequality work pretty similar, that is why we take them together (to save code space)
            const bool take_negated_solution = EQUALITY ? this->is_preceded_by_not_keyword : !this->is_preceded_by_not_keyword;
            // The code is made '==' setting in mind. In case either a not preceded the keyword before '==', or no not preceded a keyword before '!=', we take the negated solution
            // e.g. with "not host == 10.5.7.2" or with "host != 10.5.7.2." we need to take the negated solution.
            for (int i = 0; i < 16; i++) {
                if (ip6_dst_address_of_this_packet[i] != ip6_address.s6_addr[i]) {
                    if (!take_negated_solution) {
                        return false;           // the dst addresses did not match
                    } else {
                        return true;
                    }
                }
            }
            if (!take_negated_solution) {
                return true;                    // the dst addresses dit match
            } else {
                return false;
            }
        } else if (an_operator == GREATER_OR_EQUAL_THAN || an_operator == LESS_THAN) { // ... again, >= and < work pretty similar (if you take negation in mind), so to save code space they were taken together
            const bool take_negated_solution = GREATER_OR_EQUAL_THAN ? this->is_preceded_by_not_keyword : !this->is_preceded_by_not_keyword;
            // The code is made '>=' setting in mind. In case either a not preceded the keyword before '>=', or no not preceded a keyword before '<', we take the negated solution
            // e.g. with "not host >= 10.5.7.2" or with "host < 10.5.7.2." we need to take the negated solution.
            for (int i = 0; i < 16; i++) {
                if (ip6_dst_address_of_this_packet[i] > ip6_address.s6_addr[i]) {
                    if (!take_negated_solution) {
                        return true;           // the dst addresses did not match
                    } else {
                        return false;
                    }
                } else if (ip6_dst_address_of_this_packet[i] == ip6_address.s6_addr[i]) { // is is equality (=), so we need to read the next symbol in order to decide
                    // read the next symbol.
                } else {            // it is less than (<)
                    if (!take_negated_solution) {
                        return false;           // the dst addresses did not match
                    } else {
                        return true;
                    }
                }
            }
            if (!take_negated_solution) {       // if everything was equal we have a match as well
                return true;                    // the dst addresses dit match
            } else {
                return false;
            }            
            
        } else {    // it is SMALLER_OR_EQUAL_THAN or GREATER THAN, ... again, <= and > work pretty similar (if you take negation in mind), so to save code space they were taken together
            const bool take_negated_solution = LESS_OR_EQUAL_THAN ? this->is_preceded_by_not_keyword : !this->is_preceded_by_not_keyword;            
            // The code is made '<=' setting in mind. In case either a not preceded the keyword before '<=', or no not preceded a keyword before '>', we take the negated solution
            // e.g. with "not host <= 10.5.7.2" or with "host > 10.5.7.2." we need to take the negated solution.       
            for (int i = 0; i < 16; i++) {
                if (ip6_dst_address_of_this_packet[i] < ip6_address.s6_addr[i]) {
                    if (!take_negated_solution) {
                        return true;           // the dst addresses did not match
                    } else {
                        return false;
                    }
                } else if (ip6_dst_address_of_this_packet[i] == ip6_address.s6_addr[i]) { // is is equality (=), so we need to read the next symbol in order to decide
                    // read the next symbol.
                } else {            // it is greater than (>)
                    if (!take_negated_solution) {
                        return false;           // the dst addresses did not match
                    } else {
                        return true;
                    }
                }
            }
            if (!take_negated_solution) {       // if everything was equal we have a match as well
                return true;                    // the dst addresses dit match
            } else {
                return false;
            }
        }
    }
private:
    in6_addr ip6_address;
};

class IP6VersionPrimitiveToken : public PrimitiveToken {
public:
    IP6VersionPrimitiveToken(uint8_t version, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), version(version) { }
    virtual ~IP6VersionPrimitiveToken() { }
    
    virtual IP6VersionPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6VersionPrimitiveToken(this->version, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6VersionPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6VersionPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint8_t* ip6_version_number_of_this_packet = (uint8_t*) packet->network_header();
        
        switch (an_operator)
        {
        case EQUALITY:
            return take_inverse_on_not(((*ip6_version_number_of_this_packet & 0b11110000) >> 4) == version);
        case INEQUALITY:
            return take_inverse_on_not(((*ip6_version_number_of_this_packet & 0b11110000) >> 4) != version);
        case GREATER_THAN:
            return take_inverse_on_not(((*ip6_version_number_of_this_packet & 0b11110000) >> 4) > version);
        case LESS_THAN:
            return take_inverse_on_not(((*ip6_version_number_of_this_packet & 0b11110000) >> 4) < version);
        case GREATER_OR_EQUAL_THAN:
            return take_inverse_on_not(((*ip6_version_number_of_this_packet & 0b11110000) >> 4) >= version);
        default:    // LESS_OR_EQUAL_THAN
            return take_inverse_on_not(((*ip6_version_number_of_this_packet & 0b11110000) >> 4) <= version);
        }
    }
private:
    const uint8_t version;
};

class IP6PayloadLengthPrimitiveToken : public PrimitiveToken {
public:
    IP6PayloadLengthPrimitiveToken(uint16_t payload_length, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), payload_length(payload_length) { }
    virtual ~IP6PayloadLengthPrimitiveToken() { }
    
    virtual IP6PayloadLengthPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6PayloadLengthPrimitiveToken(this->payload_length, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6PayloadLengthPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6PayloadLengthPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint16_t* ip6_payload_length_of_this_packet = (uint16_t*) packet->network_header() + 2;
        
        switch (an_operator)
        {
        case EQUALITY:
            return take_inverse_on_not(htons(*ip6_payload_length_of_this_packet) == payload_length);
        case INEQUALITY:
            return take_inverse_on_not(htons(*ip6_payload_length_of_this_packet) != payload_length);
        case GREATER_THAN:
            return take_inverse_on_not(htons(*ip6_payload_length_of_this_packet) > payload_length);
        case LESS_THAN:
            return take_inverse_on_not(htons(*ip6_payload_length_of_this_packet) < payload_length);
        case GREATER_OR_EQUAL_THAN:
            return take_inverse_on_not(htons(*ip6_payload_length_of_this_packet) >= payload_length);
        default:    // LESS_OR_EQUAL_THAN
            return take_inverse_on_not(htons(*ip6_payload_length_of_this_packet) <= payload_length);
        }        
    }
private:
    const uint16_t payload_length;
};

class IP6FlowlabelPrimitiveToken : public PrimitiveToken {
public:
    IP6FlowlabelPrimitiveToken(uint32_t flow_label, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), flow_label(flow_label) { }
    virtual ~IP6FlowlabelPrimitiveToken() { }
    
    virtual IP6FlowlabelPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6FlowlabelPrimitiveToken(this->flow_label, !this->is_preceded_by_not_keyword, this->an_operator);
    }    
    
    virtual void print_name() {
        click_chatter("IP6FlowlabelPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6FlowlabelPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint8_t* ip6_flow_label_of_this_packet = (uint8_t*) packet->network_header() + 1;
        uint32_t ip6_flow_label_packet_part1 = (*ip6_flow_label_of_this_packet & 0b00001111) << 16;
        uint32_t ip6_flow_label_packet_part2 = (*(ip6_flow_label_of_this_packet + 1) << 8);
        uint32_t ip6_flow_label_packet_part3 = *(ip6_flow_label_of_this_packet + 2);
        uint32_t ip6_flow_label_packet = htonl(ip6_flow_label_packet_part1 | ip6_flow_label_packet_part2 | ip6_flow_label_packet_part3);        
        
        switch (an_operator)
        {
        case EQUALITY:
            return take_inverse_on_not(htonl(ip6_flow_label_packet) == flow_label);
        case INEQUALITY:
            return take_inverse_on_not(htonl(ip6_flow_label_packet) != flow_label);
        case GREATER_THAN:
            return take_inverse_on_not(htonl(ip6_flow_label_packet) > flow_label);
        case LESS_THAN:
            return take_inverse_on_not(htonl(ip6_flow_label_packet) < flow_label);
        case GREATER_OR_EQUAL_THAN:
            return take_inverse_on_not(htonl(ip6_flow_label_packet) >= flow_label);
        default:    // LESS_OR_EQUAL_THAN
            return take_inverse_on_not(htonl(ip6_flow_label_packet) <= flow_label);
        }
    }
private:
    const uint32_t flow_label;
};

class IP6NextHeaderPrimitiveToken : public PrimitiveToken {
public:
    IP6NextHeaderPrimitiveToken(uint8_t next_header, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), next_header(next_header) { }
    virtual ~IP6NextHeaderPrimitiveToken() { }
    
    virtual IP6NextHeaderPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6NextHeaderPrimitiveToken(this->next_header, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6NextHeaderPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6NextHeaderPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint8_t* ip6_next_header_of_this_packet = (uint8_t*) packet->network_header() + 6;
        
        switch (an_operator)
        {
        case EQUALITY:
            return take_inverse_on_not(*ip6_next_header_of_this_packet == next_header);
        case INEQUALITY:
            return take_inverse_on_not(*ip6_next_header_of_this_packet != next_header);
        case GREATER_THAN:
            return take_inverse_on_not(*ip6_next_header_of_this_packet > next_header);
        case LESS_THAN:
            return take_inverse_on_not(*ip6_next_header_of_this_packet < next_header);
        case GREATER_OR_EQUAL_THAN:
            return take_inverse_on_not(*ip6_next_header_of_this_packet >= next_header);
        default:    // LESS_OR_EQUAL_THAN
            return take_inverse_on_not(*ip6_next_header_of_this_packet <= next_header);
        }
    }
private:
    const uint8_t next_header;
};

class IP6DSCPPrimitiveToken : public PrimitiveToken {
public:
    IP6DSCPPrimitiveToken(uint8_t dscp, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), dscp(dscp) { }
    virtual ~IP6DSCPPrimitiveToken() { }
    virtual IP6DSCPPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6DSCPPrimitiveToken(this->dscp, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6DSCPPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6DSCPPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint8_t* ip6_DSCP_of_this_packet_part_1 = (uint8_t*) packet->network_header();
        uint8_t ip6_DSCP_of_packet_part_1 = (*ip6_DSCP_of_this_packet_part_1 & 0b00001111) << 2; // already shifted in the right position so the parts can be merged with |
        uint8_t* ip6_DSCP_of_this_packet_part_2 = (uint8_t*) packet->network_header() + 1;
        uint8_t ip6_DSCP_of_packet_part_2 = (*ip6_DSCP_of_this_packet_part_2 & 0b11000000) >> 6; // the second part is also shifted in the right position now
        
        switch (an_operator)
        {
        case EQUALITY:
            return take_inverse_on_not((ip6_DSCP_of_packet_part_1 | ip6_DSCP_of_packet_part_2) == dscp);    // we merged both parts and checked whether the packed DSCP is equal to the DSCP given beforehand
        case INEQUALITY:
            return take_inverse_on_not((ip6_DSCP_of_packet_part_1 | ip6_DSCP_of_packet_part_2) != dscp);
        case GREATER_THAN:
            return take_inverse_on_not((ip6_DSCP_of_packet_part_1 | ip6_DSCP_of_packet_part_2) > dscp);
        case LESS_THAN:
            return take_inverse_on_not((ip6_DSCP_of_packet_part_1 | ip6_DSCP_of_packet_part_2) < dscp);
        case GREATER_OR_EQUAL_THAN:
            return take_inverse_on_not((ip6_DSCP_of_packet_part_1 | ip6_DSCP_of_packet_part_2) >= dscp);
        default:    // LESS_OR_EQUAL_THAN
            return take_inverse_on_not((ip6_DSCP_of_packet_part_1 | ip6_DSCP_of_packet_part_2) <= dscp);
        }
        
        

    }
private:
    uint8_t dscp;
};

class IP6ECNPrimitiveToken : public PrimitiveToken {
public:
    IP6ECNPrimitiveToken(uint8_t ecn, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), ecn(ecn) { }
    virtual ~IP6ECNPrimitiveToken() { }
    virtual IP6ECNPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6ECNPrimitiveToken(this->ecn, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6ECNPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6ECNPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint8_t* ip6_ECN_of_this_packet = (uint8_t*) packet->network_header() + 1;
        uint8_t ip6_ECN_of_packet = (*ip6_ECN_of_this_packet & 0b00110000) >> 4;
        
        switch (an_operator)
        {
        case EQUALITY:
            return take_inverse_on_not(ip6_ECN_of_packet == ecn);
        case INEQUALITY:
            return take_inverse_on_not(ip6_ECN_of_packet != ecn);
        case GREATER_THAN:
            return take_inverse_on_not(ip6_ECN_of_packet > ecn);
        case LESS_THAN:
            return take_inverse_on_not(ip6_ECN_of_packet < ecn);
        case GREATER_OR_EQUAL_THAN:
            return take_inverse_on_not(ip6_ECN_of_packet >= ecn);
        default:    // LESS_OR_EQUAL_THAN
            return take_inverse_on_not(ip6_ECN_of_packet <= ecn);
        }
    }
private:
    uint8_t ecn;
};

class IP6CEPrimitiveToken : public PrimitiveToken {
public:
    IP6CEPrimitiveToken(bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) { }
    virtual ~IP6CEPrimitiveToken() { }
    virtual IP6CEPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6CEPrimitiveToken(!this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6CEPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6CEPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint8_t* ip6_CE_of_this_packet = (uint8_t*) packet->network_header() + 1;
        
        return take_inverse_on_not((*ip6_CE_of_this_packet & 0b00110000) == 0b00110000);
    }

};

class IP6HLimPrimitiveToken : public PrimitiveToken {
public:
    IP6HLimPrimitiveToken(uint8_t hop_limit, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), hop_limit(hop_limit) { }
    virtual ~IP6HLimPrimitiveToken() { }
    virtual IP6HLimPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6HLimPrimitiveToken(this->hop_limit, !this->is_preceded_by_not_keyword, this->an_operator);
    }

    virtual void print_name() {
        click_chatter("IP6HLimPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6HLimPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        uint8_t* ip6_hlim_of_this_packet = (uint8_t*) packet->network_header() + 7;
        
        switch (an_operator)
        {
        case EQUALITY:
            return take_inverse_on_not(*ip6_hlim_of_this_packet == hop_limit);
        case INEQUALITY:
            return take_inverse_on_not(*ip6_hlim_of_this_packet != hop_limit);
        case GREATER_THAN:
            return take_inverse_on_not(*ip6_hlim_of_this_packet > hop_limit);
        case LESS_THAN:
            return take_inverse_on_not(*ip6_hlim_of_this_packet < hop_limit);
        case GREATER_OR_EQUAL_THAN:
            return take_inverse_on_not(*ip6_hlim_of_this_packet >= hop_limit);
        default:    // LESS_OR_EQUAL_THAN
            return take_inverse_on_not(*ip6_hlim_of_this_packet <= hop_limit);
        }
    }
private:
    const uint8_t hop_limit;
};

/*
 * @Brief Represents an IP6 frag Primitive.
 * This one is true if the fragmentation extension header exists.
 */
class IP6FragPrimitiveToken : public PrimitiveToken {
private:
public:
    IP6FragPrimitiveToken(bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) { }
    virtual ~IP6FragPrimitiveToken() { }
    virtual IP6FragPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6FragPrimitiveToken(!this->is_preceded_by_not_keyword, this->an_operator);
    }
    
    virtual void print_name() {
        click_chatter("IP6FragPrimitiveToken");
    }
    
    virtual void print() {
        click_chatter("We encountered an IP6FragPrimitiveToken");
        PrimitiveToken::print();
    }
    
    virtual bool check_whether_packet_matches(Packet *packet) {
        return ip6::has_fragmentation_extension_header(packet);
    }
};

class IP6UnfragPrimitiveToken : public PrimitiveToken {
public:
    IP6UnfragPrimitiveToken(bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator) { }
    virtual ~IP6UnfragPrimitiveToken() { }
    virtual IP6UnfragPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6UnfragPrimitiveToken(!this->is_preceded_by_not_keyword, this->an_operator);
    }
        
    virtual void print_name() {
        click_chatter("IP6UnfragPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6UnfragPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        return !(ip6::has_fragmentation_extension_header(packet));
    }
};


/*
 * @brief A Token representing an IPv6 Net Primitive, a special kind of Primitive
 * This token is used to check whether the packet source or destination address belongs to a certain IPv6 network
 * Whenever we see in our text something of the form "net" followed by an IPv6 address, such as "net 3ffe:1900:4545:0003:0200:f8ff:fe21:67cf/48", or 
 * "net 2001:cdba:0000:0000:0000:0000:3257:9652 mask ffff:ffff:ffff:ffff:0:0:0:0" we replace it by a IP6NetPrimitiveToken.
 */
class IP6NetPrimitiveToken : public PrimitiveToken {
public:
    IP6NetPrimitiveToken(IP6Address address, IP6Address mask, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), address(address), mask(mask) { }
    virtual ~IP6NetPrimitiveToken() { }
    virtual IP6NetPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6NetPrimitiveToken(address, mask, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    virtual void print_name() {
        click_chatter("IP6NetPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6NetPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_ip6 *network_header_of_this_packet = (click_ip6*) packet->network_header();
        return take_inverse_on_not((network_header_of_this_packet->ip6_src & mask) == (address & mask) || (network_header_of_this_packet->ip6_dst & mask) == (address & mask));
    }
private:
    IP6Address address;
    IP6Address mask;
};

/*
 * @brief A Token representing an IPv6 Src Net Primitive, a special kind of Primitive
 * This token is used to check whether the packet source address belongs to a certain IPv6 network
 * Whenever we see in our text something of the form "src net" followed by an IPv6 address, such as "src net 3ffe:1900:4545:0003:0200:f8ff:fe21:67cf/48", or 
 * "src net 2001:cdba:0000:0000:0000:0000:3257:9652 mask ffff:ffff:ffff:ffff:0:0:0:0" we replace it by a IP6SrcNetPrimitiveToken.
 */
class IP6SrcNetPrimitiveToken : public PrimitiveToken {
public:
    IP6SrcNetPrimitiveToken(IP6Address address, IP6Address mask, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), address(address), mask(mask) { }
    virtual ~IP6SrcNetPrimitiveToken() { }
    virtual IP6SrcNetPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6SrcNetPrimitiveToken(address, mask, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    virtual void print_name() {
        click_chatter("IP6SrcNetPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6SrcNetPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_ip6 *network_header_of_this_packet = (click_ip6*) packet->network_header();
        return take_inverse_on_not((network_header_of_this_packet->ip6_src & mask) == (address & mask));
    }
private:
    IP6Address address;
    IP6Address mask;
};

/*
 * @brief A Token representing an IPv6 Dst Net Primitive, a special kind of Primitive
 * This token is used to check whether the packet destination address belongs to a certain IPv6 network
 * Whenever we see in our text something of the form "dst net" followed by an IPv6 address, such as "dst net 3ffe:1900:4545:0003:0200:f8ff:fe21:67cf/48", or 
 * "dst net 2001:cdba:0000:0000:0000:0000:3257:9652 mask ffff:ffff:ffff:ffff:0:0:0:0" we replace it by a IP6DstNetPrimitiveToken.
 */
class IP6DstNetPrimitiveToken : public PrimitiveToken {
public:
    IP6DstNetPrimitiveToken(IP6Address address, IP6Address mask, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), address(address), mask(mask) { }
    virtual ~IP6DstNetPrimitiveToken() { }
    virtual IP6DstNetPrimitiveToken* clone_and_invert_not_keyword_seen() {
        return new IP6DstNetPrimitiveToken(address, mask, !this->is_preceded_by_not_keyword, this->an_operator);
    }
    virtual void print_name() {
        click_chatter("IP6DstNetPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered an IP6DstNetPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_ip6 *network_header_of_this_packet = (click_ip6*) packet->network_header();
        return take_inverse_on_not((network_header_of_this_packet->ip6_dst & mask) == (address & mask));
    }
private:
    IP6Address address;
    IP6Address mask;
};

};

CLICK_ENDDECLS

#endif /* IP6CLASSIFIER_TOKENS_IP6 */

#ifndef IP6CLASSIFIER_TOKENS_TCP_UDP
#define IP6CLASSIFIER_TOKENS_TCP_UDP

#include "ip6classifier_tokens.hh"
#include <clicknet/udp.h>

CLICK_DECLS

namespace ip6classification {


/*
 * @brief A token representing Port Primitive Token, a special kind of Primitive used to represent TCP/UDP ports.
 * PortTokens are used to represent requests that ask to find UDP or TCP packets whose src port or, dst port is equal
 * to the value given. A request such as "port 102" will match all packets that have their src port or dst port in 
 * either an UDP or TCP packet set to 102.
 */
class PortPrimitiveToken : public PrimitiveToken {
public:
    PortPrimitiveToken(uint16_t port_value, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), port_value(port_value) { }
    virtual ~PortPrimitiveToken() {}
    
    virtual PortPrimitiveToken* clone_and_invert_not_keyword_seen() {
        if (this->is_preceded_by_not_keyword) {
            return new PortPrimitiveToken(port_value, false, this->an_operator);
        } else {
            return new PortPrimitiveToken(port_value, true, this->an_operator);
        }
    }

    virtual void print_name() {
        click_chatter("PortPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered a PortPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_udp* udp_header_of_this_packet = (click_udp*) packet->udp_header();
        // normally we simply give back the answer of the equality but when the not keyword was seen we give back the inverse of this    
        return take_inverse_on_not(htons(udp_header_of_this_packet->uh_sport) == port_value || htons(udp_header_of_this_packet->uh_dport) == port_value);
    }
private:
    uint16_t port_value;
};

/*
 * @brief A token representing a Src Port Primitive Token, a special sort of Primitive token used to represent src TCP/UDP ports.
 * A Src Port Primitive Token is used to check whether the source port of either an UDP or a TCP packet is equal to the stated value.
 * As an example, "src port 52" will match all UDP and TCP packets whose source port is equal to 52.
 */
class SrcPortPrimitiveToken : public PrimitiveToken {
public:
    SrcPortPrimitiveToken(uint16_t port_value, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), port_value(port_value) { }
    virtual ~SrcPortPrimitiveToken() {}
    
    virtual SrcPortPrimitiveToken* clone_and_invert_not_keyword_seen() {
        if (this->is_preceded_by_not_keyword) {
            return new SrcPortPrimitiveToken(port_value, false, this->an_operator);
        } else {
            return new SrcPortPrimitiveToken(port_value, true, this->an_operator);
        }
    }    
    
    virtual void print_name() {
        click_chatter("SrcPortPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered a SrcPortPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_udp* udp_header_of_this_packet = (click_udp*) packet->udp_header();
        // normally we simply give back the answer of the equality but when the not keyword was seen we give back the inverse of this    
        click_chatter("udp_header_of_this_packet->uh_sport = %i", htons(udp_header_of_this_packet->uh_sport));
        click_chatter("port_value = %i", port_value);        
        return take_inverse_on_not(htons(udp_header_of_this_packet->uh_sport) == port_value);
    }
private:
    uint16_t port_value;
};

/*
 * @brief A token representing a Dst Port Primitive Token, a special sort of Primitive token used to represent src TCP/UDP ports.
 * A Dst Port Primitive Token is used to check whether the destination port of either an UDP or a TCP packet is equal to the stated value.
 * As an example, "dst port 23" will match all UDP and TCP packets whose destination port is equal to 23.
 */
class DstPortPrimitiveToken : public PrimitiveToken {
public:
    DstPortPrimitiveToken(uint16_t port_value, bool is_preceded_by_not_keyword, Operator an_operator) : PrimitiveToken(is_preceded_by_not_keyword, an_operator), port_value(port_value) { }
    virtual ~DstPortPrimitiveToken() {}
        
    virtual DstPortPrimitiveToken* clone_and_invert_not_keyword_seen() {
        if (this->is_preceded_by_not_keyword) {
            return new DstPortPrimitiveToken(port_value, false, this->an_operator);
        } else {
            return new DstPortPrimitiveToken(port_value, true, this->an_operator);
        }
    }

    virtual void print_name() {
        click_chatter("DstPortPrimitiveToken");
    }
    virtual void print() {
        click_chatter("We encountered a DstPortPrimitiveToken");
        PrimitiveToken::print();
    }
    virtual bool check_whether_packet_matches(Packet *packet) {
        click_udp* udp_header_of_this_packet = (click_udp*) packet->udp_header();
        // normally we simply give back the answer of the equality but when the not keyword was seen we give back the inverse of this    
        return take_inverse_on_not(htons(udp_header_of_this_packet->uh_dport) == port_value);
    }
private:
    uint16_t port_value;
};

};

CLICK_ENDDECLS

#endif /* IP6CLASSIFIER_TOKENS_TCP_UDP */

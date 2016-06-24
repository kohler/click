#ifndef CLICK_IP6CLASSIFIER_FACTORIES_HH
#define CLICK_IP6CLASSIFIER_FACTORIES_HH

#include <click/config.h>
#include <click/string.hh>
#include <click/ipaddress.hh>
#include <click/ip6address.hh>
#include <click/etheraddress.hh>
#include <click/args.hh>
#include "ip6classifier_tokens_ip6.hh"
#include "ip6classifier_tokens_ethernet.hh"
#include "ip6classifier_tokens_tcp_udp.hh"

CLICK_DECLS

namespace ip6classification {

/*
 * @brief This class is a factory and is used to create tokens for filter primitives of the form 'host $argument'.
 * For those who would not know what a factory is, search "Factory pattern" on the internet. Basically, a factory 
 * is a class responsible for creating other objects.
 */
class HostFactory {
public:
    /*
     * @brief creates the correct host keyword token based on the argument that followed this keyword.
     * This can be an IPHostPrimitiveToken, an IP6HostPrimitiveToken or an EtherHostPrimitiveToken, or NULL when the keyword does not match any of these three.
     * @return An IPHostPrimitiveToken, an IP6HostPrimitiveToken, an EtherHostPrimitiveToken, or NULL.
     */
    static Token* create_token(String argument_of_host_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        IP6Address result2;
        if (IP6AddressArg().parse(argument_of_host_keyword, result2)) {
            return new IP6HostPrimitiveToken(result2, just_seen_a_not_keyword, an_operator);
        }
        EtherAddress result3;
        if (EtherAddressArg().parse(argument_of_host_keyword, result3)) {
            return new EtherHostPrimitiveToken(result3.data(), just_seen_a_not_keyword, an_operator);
        }
        
        throw String("host was followed by an unparsable argument '") + argument_of_host_keyword + String("'; it should be followed by an IPv6 or Ethernet address");
    }
};

/*
 * @brief This class is a factory and is used to create tokens for filter primitives of the form 'src host $argument'.
 * For those who would not know what a factory is, search "Factory pattern" on the internet. Basically, a factory 
 * is a class responsible for creating other objects.
 */
class SrcHostFactory {
public:
    /*
     * @brief creates the correct src host keywords token based on the argument that followed these keywords.
     * This can be an IPSrcHostPrimitiveToken, an IP6SrcHostPrimitiveToken or an EtherSrcHostPrimitiveToken, or NULL when the keyword does not atch any of these three.
     * @return An IPSrcHostPrimitiveToken, an IP6SrcHostPrimitiveToken, an EtherSrcHostPrimitiveToken, or NULL.
     */
    static Token* create_token(String argument_of_src_host_keywords, bool just_seen_a_not_keyword, Operator an_operator) {
        IP6Address result2;
        if (IP6AddressArg().parse(argument_of_src_host_keywords, result2)) {
            return new IP6SrcHostPrimitiveToken(result2, just_seen_a_not_keyword, an_operator);
        }
        EtherAddress result3;
        if (EtherAddressArg().parse(argument_of_src_host_keywords, result3)) {
            return new EtherSrcHostPrimitiveToken(result3.data(), just_seen_a_not_keyword, an_operator);
        }
        
        throw String("src host was followed by an unparsable argument '") + argument_of_src_host_keywords + String("'; it should be followed by an IPv6 or Ethernet address");
    }
};

/*
 * @brief This class is a factory and is used to create tokens for filter primitives of the form 'dst host $argument'.
 * For those who would not know what a factory is, search "Factory pattern" on the internet. Basically, a factory 
 * is a class responsible for creating other objects.
 */
class DstHostFactory {
public:
    /*
     * @brief creates the correct src host keywords token based on the argument that followed these keywords.
     * This can be an IPDstHostPrimitiveToken, an IP6DstHostPrimitiveToken or an EtherDstHostPrimitiveToken, or NULL when the keyword does not match any of these three.
     * @return An IPDstHostPrimitiveToken, an IP6DstHostPrimitiveToken, an EtherDstHostPrimitiveToken, or NULL.
     */
    static Token* create_token(String argument_of_dst_host_keywords, bool just_seen_a_not_keyword, Operator an_operator) {
        IP6Address result2;
        if (IP6AddressArg().parse(argument_of_dst_host_keywords, result2)) {
            return new IP6DstHostPrimitiveToken(result2, just_seen_a_not_keyword, an_operator);
        }
        EtherAddress result3;
        if (EtherAddressArg().parse(argument_of_dst_host_keywords, result3)) {
            return new EtherDstHostPrimitiveToken(result3.data(), just_seen_a_not_keyword, an_operator);
        }
        throw String("dst host was followed by an unparsable argument '") + argument_of_dst_host_keywords + String("'; it should be followed by an IPv6 or Ethernet address");
    }
};

class PortFactory {
public:
    static Token* create_token(String argument_of_port_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint16_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint16_t>(argument_of_port_keyword, result)) {
            return new PortPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        throw String("port was followed by an unparsable argument '") + argument_of_port_keyword + String("'; it should be followed by an integer between 0 and 65535");
    }
};

class SrcPortFactory {
public:
    static Token* create_token(String argument_of_port_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint16_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint16_t>(argument_of_port_keyword, result)) {
            return new SrcPortPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        throw String("src port was followed by an unparsable argument '") + argument_of_port_keyword + String("'; it should be followed by an integer between 0 and 65535");
    }
};

class DstPortFactory {
public:
    static Token* create_token(String argument_of_port_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint16_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint16_t>(argument_of_port_keyword, result)) {
            return new DstPortPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        
        throw String("dst port was followed by an unparsable argument '") + argument_of_port_keyword + String("'; it should be followed by an integer between 0 and 65535");
    }
};

class ICMPTypeFactory {
public:
    static Token* create_token(String argument_of_icmp_type_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint8_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint8_t>(argument_of_icmp_type_keyword, result)) {
            return new ICMPTypePrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        throw String("icmp type was followed by the unparsable argument '") + argument_of_icmp_type_keyword + String("', it must be followed by an integer between 0 and 255");
    }
};

class IP6VersionFactory {
public:
    static Token* create_token(String argument_of_ip6_version_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint8_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint8_t>(argument_of_ip6_version_keyword, result)) {
            if (result > 15) { // This is a 4 bit field, the number must be at least 0 and maximally 15
                throw String("ip6 vers was followed by an integer, '") + argument_of_ip6_version_keyword + String("', but the integer was not between 0 and 15");
            }
            return new IP6VersionPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        throw String("ip6 vers was followed by the unparsable argument '") + argument_of_ip6_version_keyword + String("', it must be followed by an integer between 0 and 15");
    }
};

class IP6PayloadLengthFactory {
public:
    static Token* create_token(String argument_of_ip6_payload_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint16_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint16_t>(argument_of_ip6_payload_keyword, result)) {
            return new IP6PayloadLengthPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }

        throw String("ip6 plen was followed by the unparsable argument '") + argument_of_ip6_payload_keyword + String("', it must be followed by an integer between 0 and 65535");        
    }
};

class IP6FlowlabelFactory {
public:
    static Token* create_token(String argument_of_ip6_flowlabel_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint32_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint32_t>(argument_of_ip6_flowlabel_keyword, result)) {
            if (result > 1048575) { // This is a 20 bit field, the number must be at least 0 and maximally 1048575
                throw String("ip6 flow was followed by an integer, '") + argument_of_ip6_flowlabel_keyword + String("', but the integer was not between 0 and 1048575");
                return NULL;
            }
            return new IP6FlowlabelPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        
        throw String("ip6 plen was followed by the unparsable argument '") + argument_of_ip6_flowlabel_keyword + String("', it must be followed by an integer between 0 and 1048575s");        
    }
};

class IP6NextHeaderFactory {
public:
    static Token* create_token(String argument_of_ip6_next_header_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint8_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint8_t>(argument_of_ip6_next_header_keyword, result)) {
            return new IP6NextHeaderPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        
        throw String("ip6 nxt was followed by the unparsable argument '") + argument_of_ip6_next_header_keyword + String("', it must be followed by an integer between 0 and 255");        
    }

};

class IP6DSCPFactory {
public:
    static Token* create_token(String argument_of_ip6_dscp_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint8_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint8_t>(argument_of_ip6_dscp_keyword, result)) {
            if (result > 63) {  // This is a 6 bit field, the number must be at least 0 and maximally 63
                throw String("ip6 dscp was followed by the integer '") + argument_of_ip6_dscp_keyword + String("', but the integer was not between 0 and 63");        
            }
            return new IP6DSCPPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        
        throw String("ip6 dscp was followed by the unparsable argument '") + argument_of_ip6_dscp_keyword + String("', it must be followed by an integer between 0 and 63");        
    }
};

class IP6ECNFactory {
public:
    static Token* create_token(String argument_of_ip6_ecn_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint8_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint8_t>(argument_of_ip6_ecn_keyword, result)) {
            if (result > 3) {  /// This is a 2 bit field, the number must be at least 0 and maximally 3
                throw String("ip6 dscp was followed by the integer '") + argument_of_ip6_ecn_keyword + String("', but the integer was not between 0 and 3");        
            }
            return new IP6ECNPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        
        throw String("ip6 ecn was followed by the unparsable argument '") + argument_of_ip6_ecn_keyword + String("', it must be followed by an integer between 0 and 3");        
    }
};

class IP6HLimFactory {
public:
    static Token* create_token(String argument_of_ip6_hlim_keyword, bool just_seen_a_not_keyword, Operator an_operator) {
        uint8_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint8_t>(argument_of_ip6_hlim_keyword, result)) {
            return new IP6HLimPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        
        throw String("ip6 dscp was followed by the unparsable argument '") + argument_of_ip6_hlim_keyword + String("', it must be followed by an integer between 0 and 255");
    }
};

class NetFactory {
public:
    /*
     * @brief creates the correct net keyword token based on the argument(s) that followed this keyword.
     * This can be an IPNetPrimitiveToken, an IP6NetPrimitiveToken, or NULL when the keyword does not match any of these three.
     *
     * @param words_following_net A vector keeping track of words following the net keyword. Needed to create the correct Token.
     * @param an_operator The operator that possibly followed the 'net' keyword
     * @param was_written_in_CIDR_style Returns true if the 'net' keyword was followed by a CIDR style argument, false if it was followed by a 'mask' address argument.
     * @return An IPNetPrimitiveToken, an IP6NetPrimitiveToken, or NULL (in case something went wrong).
     */
    static Token* create_token(Vector<String> words_following_net, bool just_seen_a_not_keyword, Operator an_operator, bool& was_written_in_CIDR_style) {
        // Determine whether it is CIDR style
        int slash_location;     // if there is a slash its location will be hold in this variable
        if ((slash_location = words_following_net[0].find_left('/')) != -1) {        // It is CIDR style
            was_written_in_CIDR_style = true;
            
            IntArg *int_arg = new IntArg();
            int result;

            IP6Address ip6address;
            if ((IP6AddressArg().parse(words_following_net[0].substring(0,slash_location-1), ip6address)) && (int_arg->template parse<int>(words_following_net[0].substring(slash_location+1, 
            words_following_net[0].length()-slash_location-1),result))) {
                if (result >= 0 && result <= 128) {
                    return new IP6NetPrimitiveToken(ip6address, IP6Address::make_prefix(result), just_seen_a_not_keyword, an_operator);
                } else {
                    return NULL;    // This is wrong; the IPv6 address prefix should be between 0 and 128
                }
            } else {
                return NULL;    // What is left of the slash is not an IPv4 or IPv6 address
            }
        } else {                                                            // It is NOT CIDR style
            was_written_in_CIDR_style = false;
            if (words_following_net.size() != 3) {
                return NULL;                // It is not in CIDR style so 'net' must be followed by at least 3 arguments
            }
            if (words_following_net[1] != "mask") {
                return NULL;                // It is not in CIDR style so 'net' must be followed by at least 3 arguments from which the second one must be 'mask'
            }
            IP6Address ip6address, ip6mask;
            if ((IP6AddressArg().parse(words_following_net[0], ip6address)) && (IP6AddressArg().parse(words_following_net[2], ip6mask))) {
                return new IP6NetPrimitiveToken(ip6address, ip6mask, just_seen_a_not_keyword, an_operator);            
            } else {
                return NULL;            // It is not in CIDR style so it must be followed by 3 arguments from which the first and the third one must be both IPv6 or IPv4 addresses
            }
        }
    }
};

class SrcNetFactory {
public:
    /*
     * @brief creates the correct src net keyword token based on the argument(s) that followed this keyword.
     * This can be an IPSrcNetPrimitiveToken, an IP6SrcNetPrimitiveToken, or NULL when the keyword does not match any of these three.
     *
     * @param words_following_net A vector keeping track of words following the net keyword. Needed to create the correct Token.
     * @param an_operator The operator that possibly followed the 'net' keyword
     * @param was_written_in_CIDR_style Returns true if the 'net' keyword was followed by a CIDR style argument, false if it was followed by a 'mask' address argument.
     * @return An IPSrcNetPrimitiveToken, an IP6SrcNetPrimitiveToken, or NULL (in case something went wrong).
     */
    static Token* create_token(Vector<String> words_following_net, bool just_seen_a_not_keyword, Operator an_operator, bool& was_written_in_CIDR_style) {
        // Determine whether it is CIDR style
        int slash_location;     // if there is a slash its location will be hold in this variable
        if ((slash_location = words_following_net[0].find_left('/')) != -1) {        // It is CIDR style
            was_written_in_CIDR_style = true;
            
            IntArg *int_arg = new IntArg();
            int result;

            IP6Address ip6address;
            if ((IP6AddressArg().parse(words_following_net[0].substring(0,slash_location-1), ip6address)) && (int_arg->template parse<int>(words_following_net[0].substring(slash_location+1, 
            words_following_net[0].length()-slash_location-1),result))) {
                if (result >= 0 && result <= 128) {
                    return new IP6SrcNetPrimitiveToken(ip6address, IP6Address::make_prefix(result), just_seen_a_not_keyword, an_operator);
                } else {
                    return NULL;    // This is wrong; the IPv6 address prefix should be between 0 and 128
                }
            } else {
                return NULL;    // What is left of the slash is not an IPv4 or IPv6 address
            }
        } else {                                                            // It is NOT CIDR style
            was_written_in_CIDR_style = false;
            if (words_following_net.size() != 3) {
                return NULL;                // It is not in CIDR style so 'net' must be followed by at least 3 arguments
            }
            if (words_following_net[1] != "mask") {
                return NULL;                // It is not in CIDR style so 'net' must be followed by at least 3 arguments from which the second one must be 'mask'
            }
            IP6Address ip6address, ip6mask;
            if ((IP6AddressArg().parse(words_following_net[0], ip6address)) && (IP6AddressArg().parse(words_following_net[2], ip6mask))) {
                return new IP6SrcNetPrimitiveToken(ip6address, ip6mask, just_seen_a_not_keyword, an_operator);            
            } else {
                return NULL;            // It is not in CIDR style so it must be followed by 3 arguments from which the first and the third one must be both IPv6 or IPv4 addresses
            }
        }
    }
};

class DstNetFactory {
public:
    /*
     * @brief creates the correct dst net keyword token based on the argument(s) that followed this keyword.
     * This can be an IPDstNetPrimitiveToken, an IP6DstNetPrimitiveToken, or NULL when the keyword does not match any of these three.
     *
     * @param words_following_net A vector keeping track of words following the net keyword. Needed to create the correct Token.
     * @param an_operator The operator that possibly followed the 'net' keyword
     * @param was_written_in_CIDR_style Returns true if the 'net' keyword was followed by a CIDR style argument, false if it was followed by a 'mask' address argument.
     * @return An IPDstNetPrimitiveToken, an IP6DstNetPrimitiveToken, or NULL (in case something went wrong).
     */
    static Token* create_token(Vector<String> words_following_net, bool just_seen_a_not_keyword, Operator an_operator, bool& was_written_in_CIDR_style) {
        // Determine whether it is CIDR style
        int slash_location;     // if there is a slash its location will be hold in this variable
        if ((slash_location = words_following_net[0].find_left('/')) != -1) {        // It is CIDR style
            was_written_in_CIDR_style = true;
            
            IntArg *int_arg = new IntArg();
            int result;
            IP6Address ip6address;
            if ((IP6AddressArg().parse(words_following_net[0].substring(0,slash_location-1), ip6address)) && (int_arg->template parse<int>(words_following_net[0].substring(slash_location+1, 
            words_following_net[0].length()-slash_location-1),result))) {
                if (result >= 0 && result <= 128) {
                    return new IP6DstNetPrimitiveToken(ip6address, IP6Address::make_prefix(result), just_seen_a_not_keyword, an_operator);
                } else {
                    return NULL;    // This is wrong; the IPv6 address prefix should be between 0 and 128
                }
            } else {
                return NULL;    // What is left of the slash is not an IPv4 or IPv6 address
            }
        } else {                                                            // It is NOT CIDR style
            was_written_in_CIDR_style = false;
            if (words_following_net.size() != 3) {
                return NULL;                // It is not in CIDR style so 'net' must be followed by at least 3 arguments
            }
            if (words_following_net[1] != "mask") {
                return NULL;                // It is not in CIDR style so 'net' must be followed by at least 3 arguments from which the second one must be 'mask'
            }
            IP6Address ip6address, ip6mask;
            if ((IP6AddressArg().parse(words_following_net[0], ip6address)) && (IP6AddressArg().parse(words_following_net[2], ip6mask))) {
                return new IP6DstNetPrimitiveToken(ip6address, ip6mask, just_seen_a_not_keyword, an_operator);            
            } else {
                return NULL;            // It is not in CIDR style so it must be followed by 3 arguments from which the first and the third one must be both IPv6 or IPv4 addresses
            }
        }
    }
};

class TCPOptFactory {
public:
    static Token* create_token(String tcp_option_to_be_inspected, bool just_seen_a_not_keyword, Operator an_operator) {
        //'syn', 'fin', 'ack', 'rst', 'psh', 'urg'
        if (tcp_option_to_be_inspected == "syn") {
            if (an_operator == EQUALITY || an_operator == INEQUALITY) {
                return new TCPOptionNamePrimitiveToken(SYN, just_seen_a_not_keyword, an_operator);
            } else {
                throw String("tcp fin was followed by an incorrect operator, only '==' and '!=' are allowed with tcp syn");
            }
        } else if (tcp_option_to_be_inspected == "fin") {
            if (an_operator == EQUALITY || an_operator == INEQUALITY) {
                return new TCPOptionNamePrimitiveToken(FIN, just_seen_a_not_keyword, an_operator);
            } else {
                throw String("tcp opt fin was followed by an incorrect operator, only '==' and '!=' are allowed with tcp opt fin");
            }    
        } else if (tcp_option_to_be_inspected == "ack") {
            if (an_operator == EQUALITY || an_operator == INEQUALITY) {
                return new TCPOptionNamePrimitiveToken(ACK, just_seen_a_not_keyword, an_operator);
            } else {
                throw String("tcp opt fin was followed by an incorrect operator, only '==' and '!=' are allowed with tcp opt ack");
            }    
        } else if (tcp_option_to_be_inspected == "rst") {
            if (an_operator == EQUALITY || an_operator == INEQUALITY) {
                return new TCPOptionNamePrimitiveToken(RST, just_seen_a_not_keyword, an_operator);
            } else {
                throw String("tcp opt fin was followed by an incorrect operator, only '==' and '!=' are allowed with tcp opt rst");
            }    
        } else if (tcp_option_to_be_inspected == "psh") {
            if (an_operator == EQUALITY || an_operator == INEQUALITY) {
                return new TCPOptionNamePrimitiveToken(PSH, just_seen_a_not_keyword, an_operator);
            } else {
                throw String("tcp opt fin was followed by an incorrect operator, only '==' and '!=' are allowed with tcp opt psh");
            }    
        } else if (tcp_option_to_be_inspected == "urg") {
            if (an_operator == EQUALITY || an_operator == INEQUALITY) {
            } else {
                return new TCPOptionNamePrimitiveToken(URG, just_seen_a_not_keyword, an_operator);
                throw String("tcp opt fin was followed by an incorrect operator, only '==' and '!=' are allowed with tcp opt urg");
            }    
        } else {
            throw String("tcp opt can not be followed by '") + String(tcp_option_to_be_inspected) + String("', but should be followed by syn, fin, ack, rst, psh or urg");
        }
    }
};

class TCPWinFactory {
public:
    static Token* create_token(String argument_of_tcp_win, bool just_seen_a_not_keyword, Operator an_operator) {
        uint16_t result;
        
        IntArg* int_arg = new IntArg();
        if (int_arg->template parse<uint16_t>(argument_of_tcp_win, result)) {
            return new TCPReceiveWindowLengthPrimitiveToken(result, just_seen_a_not_keyword, an_operator);
        }
        
        throw String("tcp win was followed by the unparsable argument '") + argument_of_tcp_win + String("', it must be followed by an integer between 0 and 65535");
     }
};

};

CLICK_ENDDECLS
#endif /* CLICK_IP6FILTER_FACTORIES_HH */

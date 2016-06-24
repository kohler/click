/*
 * ip6classifier_lexer.{cc,hh} -- Lexer for IP-packet filter with tcpdumplike syntax
 * Glenn Minne
 *
 * Copyright (c) 2000-2007 Mazu Networks, Inc.
 * Copyright (c) 2010 Meraki, Inc.
 * Copyright (c) 2004-2011 Regents of the University of California
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "ip6classifier_lexer.hh"
CLICK_DECLS

namespace ip6classification {

/*
 * @brief This function constructs a Lexer object, and initializes it with a to be lexed string.
 * @param to_be_lexed_string This is the string that needs to be lexed and split up in tokens.
 */
Lexer::Lexer(String to_be_lexed_string) {
    this->to_be_lexed_string = to_be_lexed_string;
}

/*
 * @brief This function is the objects destructor. It does nothing.
 * Since the class does not contain pointers, nothing needs to be freed.
 */
Lexer::~Lexer() { }

/*
 * @brief Skips all te blanks it sees from start position i and will returns the first non-blank position i or -1 if it goes out of line
 * @param i the start position from which we need to start skipping blanks
 * @return the first position of a value that is not a blank, or -1 if we had blanks until the end of the file
 */
int
Lexer::skip_blanks(int i) {
    while(isspace(to_be_lexed_string[i])) {
        i++;
        if (i >= to_be_lexed_string.length()) {     // i is out of range, a new isspace(string[i]) call would crash
            return -1;
        }
    }
    return i;
}

/*
 * @brief Reads the next word of the to_be_lexed_string, starting to find the word from position i.
 * A word is either a parenthesis, or a combination of characters (not containig a parenthesis or a blank).
 * If it is a combination of characters, these are separated from each other in the filter expression with  a blank or a parenthesis.
 * 
 * Examples of words are: (, ), hello, bla, komma
 * Examples of what are not words: dog(                     // must be two words namely: dog, (
 *                                 dogs and cats            // must be three words namely: dogs, and, cats
 * @param i the position from which we need to be reading a word, which is either a parenthesis or a combination of characters ended by a blank, a parenthesis or the end of the filter expression.
 * @return the first position of a value that is not part of the word anymore, either a blank, a parenthesis, or -1 if we are at the end of the filter expression */
int
Lexer::read_word(int i, String& read_word) {
    // check whether the word is a parenthesis
    if(to_be_lexed_string[i] == '(') {
        read_word = "(";
        if (i+1 < to_be_lexed_string.length()) {
            return i+1;
        } else {
            return -1;  // indicating this was the last word
        }
    }
    if(to_be_lexed_string[i] == ')') {
        read_word = ")";
        if (i+1 < to_be_lexed_string.length()) {
            return i+1;
        } else {
            return -1; // indicating this was the last word
        }
    }
    // the word is a combination of characters
    int start_i = i;    // we save the start position for further use
    while(!(isspace(to_be_lexed_string[i]) || (to_be_lexed_string[i] == '(') || (to_be_lexed_string[i] == ')'))) {      // keep reading until we found a blank, a ( or a )
        i++;
        if (i >= to_be_lexed_string.length()) {
            read_word = to_be_lexed_string.substring(start_i, i - start_i);
            return -1;      // indicating this was the last word
        }
    }
    read_word = to_be_lexed_string.substring(start_i, i - start_i);
    return i;
}

// Skip blanks and read the next word of the "configuration string that was given when initializing this element".
// i must be set to the current character location we are scanning at the moment in the "cinfiguration string that was given when initializing this element".
// The result; if a string could be written; will be put inside the read_word variable parameter
// If a string could be read, it returns 0, when something goes wrong it returns -1.
// Throws an exception when we reached the end of the line and could not read a new word
void Lexer::skip_blanks_and_read_word(int& i, String& word_to_be_read, const String error) {
    i = skip_blanks(i);
    if (i == -1) {  // an error was found
        throw error;
    }
    
    i = read_word(i, word_to_be_read);
}

/*
 * @brief Checks whether the given word is an operator and if it is an operator, it will store the operator in the an_operator parameter. Returns >= 0 on succss and -1 on failure.
 * @param word the word for which we need to check whether it is an operator
 * @param an_operator the operator will be stored in this value if the string contained an operator
 * @return 0 on success, -1 on failure
 */ 
int
is_word_an_operator(String word, Operator& an_operator) {
    if (word == ">=") {
        an_operator = GREATER_OR_EQUAL_THAN; return 0;
    } else if (word == ">") {
        an_operator = GREATER_THAN; return 0;
    } else if (word == "<" ) {
        an_operator = LESS_THAN; return 0;
    } else if (word == "<=") {
        an_operator = LESS_OR_EQUAL_THAN; return 0;
    } else if (word == "!=") {
        an_operator = INEQUALITY; return 0;
    } else if (word == "==") {
        an_operator = EQUALITY; return 0;
    } else {
        return -1;
    }
}

/*
 * @brief This function splits up the string with which the Lexer was initialized up into tokens.
 * These tokens are the standard units the tcpdumplikesyntax is build up from. Basically we could
 * say these are the separate words, which are separated with blanks and tabs.
 * This function can never fail.
 * Example tokens are "dst host 2001:db8:a0b:12f0::1", "src port 589" and "host 10.0.1.2"
 * @return A list of tokens.
 */
int
Lexer::lex(Vector<Token*>& tokens, ErrorHandler *errh) {      // to_be_lexed_string must become a FilterString which extends String and has a matches method
    String current_word = "";
    Operator an_operator;   // this contains an operator if there was an operator between the keyword and the data.  (e.g. as in host == 12.15.6.2; host >= 10.5.9.4).
                            // if no operator was given equality is assumed (e.g. host == 12.15.6.2 is the same as host 12.15.6.2)
    int i = 0;
    bool just_seen_a_not_keyword = false;  // we use this variable to keep track of a possible not keyword seen (e.g. as in not host 12.5.91.1).
    while (true) {
        try {        
            i = skip_blanks(i); // skip the potential blanks at the start and go to the first non blank position
            if (i == -1) {      // end of line was seen after a series of blanks
                return 0;       // At the end of an expression it is allowed to have it followed by nothing anymore (or only by blanks)
                                // We return 0 because we have successfully read the last expression
            }
            i = read_word(i, current_word); // read out the next word, then process the word
            
            if(current_word == "not") {
                if (!just_seen_a_not_keyword == true) {
                    if (skip_blanks(i) >= 0) {
                        just_seen_a_not_keyword = true;
                    } else {
                        errh->error("not should not be the last word in a filter expression"); return -1;
                    }
                } else {
                    errh->error("we have seen 'not' two times in a row, that is not allowed"); return -1;
                }
            } else if(current_word == "and") {
                if (!just_seen_a_not_keyword) {
                    tokens.push_back(new AndCombinerToken());
                } else {
                    errh->error("and token should not be preceded by a not"); return -1;
                }
            } else if (current_word == "or") {
                if (!just_seen_a_not_keyword) {
                    tokens.push_back(new OrCombinerToken());
                } else {
                    errh->error("or token should not be preceded by a not"); return -1;
                }
            } else if (current_word == "(") {
                tokens.push_back(new LeftParenthesisToken(just_seen_a_not_keyword));
                just_seen_a_not_keyword = false;
            } else if (current_word == ")") {
                if (!just_seen_a_not_keyword) {
                    tokens.push_back(new RightParenthesisToken());
                } else {
                    errh->error(") token should not be preceded by a not"); return -1;
                }
            } else if (current_word == "src") {
                skip_blanks_and_read_word(i, current_word, "no second keyword followed after src; src should be followed by host or port");
                if (current_word == "host") {
                    skip_blanks_and_read_word(i, current_word, "src host was not followed by an argument");
                    Token *token;
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, is should be followed by data");
                        token = SrcHostFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = SrcHostFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                    tokens.push_back(token);
                    just_seen_a_not_keyword = false;
                } else if (current_word == "port") {
                    skip_blanks_and_read_word(i, current_word, "src port was not followed by an argument");
                    Token *token;
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, it should be followed by data");
                        token = SrcPortFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = SrcPortFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                    tokens.push_back(token);
                    just_seen_a_not_keyword = false;
                } else if (current_word == "net") {
                    bool was_the_argument_written_in_CIDR_style;    // This bool will get a value being passed to the SrcNetFactory
                    Vector<String> words_following_net;     // A Vector keeping track of words following 'net'. Used to construct the correct token by the Token factory.
                    skip_blanks_and_read_word(i, current_word, "no keyword or argument followed the src net keyword");
                    
                    words_following_net.push_back(current_word);
                    int old_i = i;          // When we did not use CIDR notation we need to go back to this 'i'
                    i = skip_blanks(i);
                    
                    if (i != -1) {
                        i = read_word(i, current_word);
                        words_following_net.push_back(current_word);
                        i = skip_blanks(i);
                        if (i != -1) {
                            i = read_word(i, current_word);
                            words_following_net.push_back(current_word);
                            Token *token = SrcNetFactory::create_token(words_following_net, just_seen_a_not_keyword, an_operator, was_the_argument_written_in_CIDR_style);
                            if (token != NULL) {
                                tokens.push_back(token);
                                just_seen_a_not_keyword = false;
                                if (was_the_argument_written_in_CIDR_style) {
                                    // the next to be read is really the next word to be read is further back, go back to the old i
                                    i = old_i;
                                }   // when not in CIDR style we do not need to go back so we need to do nothing in that case
                            } else {
                                if (was_the_argument_written_in_CIDR_style) {
                                    errh->error("src net was followed by an unparsable argument"); return -1;
                                } else {
                                    errh->error("src net mask was followed by an unparsable argument"); return -1;
                                }
                            }
                        } else {
                            Token *token = SrcNetFactory::create_token(words_following_net, just_seen_a_not_keyword, an_operator, was_the_argument_written_in_CIDR_style);
                            if (token != NULL) {
                                tokens.push_back(token);
                                just_seen_a_not_keyword = false;
                                if (was_the_argument_written_in_CIDR_style) {
                                    // the next to be read is really the next word to be read is further back, go back to the old i
                                    i = old_i;
                                }   // when not in CIDR style we do not need to go back so we need to do nothing in that case
                            } else {
                                if (was_the_argument_written_in_CIDR_style) {
                                    errh->error("src net was followed by an unparsable argument"); return -1;
                                } else {
                                    errh->error("src net mask was followed by an unparsable argument"); return -1;
                                }
                            }
                        }
                    } else {
                        Token *token = SrcNetFactory::create_token(words_following_net, just_seen_a_not_keyword, an_operator, was_the_argument_written_in_CIDR_style);
                        if (token != NULL) {
                            tokens.push_back(token);
                            just_seen_a_not_keyword = false;
                            if (was_the_argument_written_in_CIDR_style) {
                                // the next to be read is really the next word to be read is further back, go back to the old i
                                i = old_i;
                            }   // when not in CIDR style we do not need to go back so we need to do nothing in that case
                        } else {
                            if (was_the_argument_written_in_CIDR_style) {
                                errh->error("src net was followed by an unparsable argument"); return -1;
                            } else {
                                errh->error("src net mask was followed by an unparsable argument"); return -1;
                            }
                        }
                    }
                } else {
                    errh->error("unkown keyword '%s' followed after src, the keyword should be host or port", current_word.c_str()); return -1;
                }
            } else if (current_word == "dst") {
                skip_blanks_and_read_word(i, current_word, "no second keyword followed after dst; dst should be followed by host or port");
                if (current_word == "host") {
                    i = skip_blanks(i);
                    if (i != -1) {
                        i = read_word(i, current_word);
                        
                        Token *token;
                        if (is_word_an_operator(current_word, an_operator) >= 0) {
                            i = skip_blanks(i);
                            if (i != -1) {
                                i = read_word(i, current_word);
                                token = SrcHostFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                            } else {
                                errh->error("operator was only followed by blanks, that is not allowed, an operator must be followed by data"); return -1;
                            }
                        } else {    // no operator was given, equality is assumed and the current word already contains the data
                            token = SrcHostFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                        }
                        tokens.push_back(token);
                        just_seen_a_not_keyword = false;
                    } else {
                        errh->error("dst host was not followed by an argument"); return -1;
                    }
                } else if (current_word == "port") {
                    skip_blanks_and_read_word(i, current_word, "dst port was not followed by an argument");
                        
                    Token *token;
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = DstPortFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = DstPortFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                    tokens.push_back(token);
                    just_seen_a_not_keyword = false;
                } else if (current_word == "net") {
                    bool was_the_argument_written_in_CIDR_style;    // This bool will get a value being passed to the SrcNetFactory
                    Vector<String> words_following_net;     // A Vector keeping track of words following 'net'. Used to construct the correct token by the Token factory.
                    skip_blanks_and_read_word(i, current_word, "no keyword or argument followed the dst net keyword");
                    
                    words_following_net.push_back(current_word);
                    int old_i = i;          // When we did not use CIDR notation we need to go back to this 'i'
                    i = skip_blanks(i);
                    
                    if (i != -1) {
                        i = read_word(i, current_word);
                        words_following_net.push_back(current_word);
                        i = skip_blanks(i);
                        if (i != -1) {
                            i = read_word(i, current_word);
                            words_following_net.push_back(current_word);
                            Token *token = DstNetFactory::create_token(words_following_net, just_seen_a_not_keyword, an_operator, was_the_argument_written_in_CIDR_style);
                            if (token != NULL) {
                                tokens.push_back(token);
                                just_seen_a_not_keyword = false;
                                if (was_the_argument_written_in_CIDR_style) {
                                    // the next to be read is really the next word to be read is further back, go back to the old i
                                    i = old_i;
                                }   // when not in CIDR style we do not need to go back so we need to do nothing in that case
                            } else {
                                if (was_the_argument_written_in_CIDR_style) {
                                    errh->error("dst net was followed by an unparsable argument"); return -1;
                                } else {
                                    errh->error("dst net mask was followed by an unparsable argument"); return -1;
                                }
                            }
                        } else {
                            Token *token = DstNetFactory::create_token(words_following_net, just_seen_a_not_keyword, an_operator, was_the_argument_written_in_CIDR_style);
                            if (token != NULL) {
                                tokens.push_back(token);
                                just_seen_a_not_keyword = false;
                                if (was_the_argument_written_in_CIDR_style) {
                                    // the next to be read is really the next word to be read is further back, go back to the old i
                                    i = old_i;
                                }   // when not in CIDR style we do not need to go back so we need to do nothing in that case
                            } else {
                                if (was_the_argument_written_in_CIDR_style) {
                                    errh->error("dst net was followed by an unparsable argument"); return -1;
                                } else {
                                    errh->error("dst net mask was followed by an unparsable argument"); return -1;
                                }
                            }
                        }
                    } else {
                        Token *token = DstNetFactory::create_token(words_following_net, just_seen_a_not_keyword, an_operator, was_the_argument_written_in_CIDR_style);
                        if (token != NULL) {
                            tokens.push_back(token);
                            just_seen_a_not_keyword = false;
                            if (was_the_argument_written_in_CIDR_style) {
                                // the next to be read is really the next word to be read is further back, go back to the old i
                                i = old_i;
                            }   // when not in CIDR style we do not need to go back so we need to do nothing in that case
                        } else {
                            if (was_the_argument_written_in_CIDR_style) {
                                errh->error("dst net was followed by an unparsable argument"); return -1;
                            } else {
                                errh->error("dst net mask was followed by an unparsable argument"); return -1;
                            }
                        }
                    }
                } else {
                    errh->error("unkown keyword '%s' followed after dst, the keyword should be host, port or net", current_word.c_str()); return -1;
                }
            } else if (current_word == "host") {
                skip_blanks_and_read_word(i, current_word, "host keyword was not followed by an argument");
                    
                Token *token;
                if (is_word_an_operator(current_word, an_operator) >= 0) {
                    i = read_word(i, current_word);
                    token = HostFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                } else {    // no operator was given, equality is assumed and the current word already contains the data
                    token = HostFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                }
                tokens.push_back(token);
                just_seen_a_not_keyword = false;
            } else if (current_word == "port") {
                skip_blanks_and_read_word(i, current_word, "port was not followed by an argument");
                    
                Token *token;
                if (is_word_an_operator(current_word, an_operator) >= 0) {
                    skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                    i = read_word(i, current_word);
                    token = PortFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                } else {    // no operator was given, equality is assumed and the current word already contains the data
                    token = PortFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                }
                tokens.push_back(token);
                just_seen_a_not_keyword = false;
            } else if (current_word == "net") {
                bool was_the_argument_written_in_CIDR_style;    // This bool will get a value being passed to the SrcNetFactory
                Vector<String> words_following_net;     // A Vector keeping track of words following 'net'. Used to construct the correct token by the Token factory.
                skip_blanks_and_read_word(i, current_word, "no keyword or argument followed the net keyword");
                
                words_following_net.push_back(current_word);
                int old_i = i;          // When we did not use CIDR notation we need to go back to this 'i'
                i = skip_blanks(i);
                
                if (i != -1) {
                    i = read_word(i, current_word);
                    words_following_net.push_back(current_word);
                    i = skip_blanks(i);
                    if (i != -1) {
                        i = read_word(i, current_word);
                        words_following_net.push_back(current_word);
                        Token *token = NetFactory::create_token(words_following_net, just_seen_a_not_keyword, an_operator, was_the_argument_written_in_CIDR_style);
                        if (token != NULL) {
                            tokens.push_back(token);
                            just_seen_a_not_keyword = false;
                            if (was_the_argument_written_in_CIDR_style) {
                                // the next to be read is really the next word to be read is further back, go back to the old i
                                i = old_i;
                            }   // when not in CIDR style we do not need to go back so we need to do nothing in that case
                        } else {
                            if (was_the_argument_written_in_CIDR_style) {
                                errh->error("net was followed by an unparsable argument"); return -1;
                            } else {
                                errh->error("net mask was followed by an unparsable argument"); return -1;
                            }
                        }
                    } else {
                        Token *token = NetFactory::create_token(words_following_net, just_seen_a_not_keyword, an_operator, was_the_argument_written_in_CIDR_style);
                        if (token != NULL) {
                            tokens.push_back(token);
                            just_seen_a_not_keyword = false;
                            if (was_the_argument_written_in_CIDR_style) {
                                // the next to be read is really the next word to be read is further back, go back to the old i
                                i = old_i;
                            }   // when not in CIDR style we do not need to go back so we need to do nothing in that case
                        } else {
                            if (was_the_argument_written_in_CIDR_style) {
                                errh->error("net was followed by an unparsable argument"); return -1;
                            } else {
                                errh->error("net mask was followed by an unparsable argument"); return -1;
                            }
                        }
                    }
                } else {
                    Token *token = NetFactory::create_token(words_following_net, just_seen_a_not_keyword, an_operator, was_the_argument_written_in_CIDR_style);
                    if (token != NULL) {
                        tokens.push_back(token);
                        just_seen_a_not_keyword = false;
                        if (was_the_argument_written_in_CIDR_style) {
                            // the next to be read is really the next word to be read is further back, go back to the old i
                            i = old_i;
                        }   // when not in CIDR style we do not need to go back so we need to do nothing in that case
                    } else {
                        if (was_the_argument_written_in_CIDR_style) {
                            errh->error("net was followed by an unparsable argument"); return -1;
                        } else {
                            errh->error("net mask was followed by an unparsable argument"); return -1;
                        }
                    }
                }            
            } else if (current_word == "icmp") {
                skip_blanks_and_read_word(i, current_word, "no second keyword after icmp; it should be followed by type");
                Token *token;
                if (current_word == "type") {
                    skip_blanks_and_read_word(i, current_word, "no argument followed after icmp type statement");
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = ICMPTypeFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = ICMPTypeFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                }
            } else if (current_word == "ip6") {
                skip_blanks_and_read_word(i, current_word, "no second keyword after ip6; ip6 should be followed by vers, plen, flow, nxt, dscp, ecn, ce, hlim, frag, unfrag.");
                Token *token;
                if (current_word == "vers") {           // version
                    skip_blanks_and_read_word(i, current_word, "no argument followed after ip6 vers statement");
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = IP6VersionFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = IP6VersionFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                } else if (current_word == "plen") {    // payload length
                    skip_blanks_and_read_word(i, current_word, "no argument followed after ip6 plen statement");
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = IP6PayloadLengthFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = IP6PayloadLengthFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                } else if (current_word == "flow") {    // flow label
                    skip_blanks_and_read_word(i, current_word, "no argument followed after ip6 flow statement");
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = IP6FlowlabelFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = IP6FlowlabelFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                } else if (current_word == "nxt") {     // next header
                    skip_blanks_and_read_word(i, current_word, "no argument followed after ip6 nxt statement");
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = IP6NextHeaderFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = IP6NextHeaderFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }                
                } else if (current_word == "dscp") {
                    skip_blanks_and_read_word(i, current_word, "no argument followed after ip6 dscp statement");
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = IP6DSCPFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = IP6DSCPFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                } else if (current_word == "ecn") {
                    skip_blanks_and_read_word(i, current_word, "no argument followed after ip6 ecn statement");
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = IP6ECNFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {
                        token = IP6ECNFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);           
                    }
                } else if (current_word == "ce") {
                    token = new IP6CEPrimitiveToken(just_seen_a_not_keyword, an_operator);           
                } else if (current_word == "hlim") {    // hop limit
                    skip_blanks_and_read_word(i, current_word, "no argument followed after ip6 hlim statement");
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = IP6HLimFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = IP6HLimFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                } else if (current_word == "frag") {
                    token = new IP6FragPrimitiveToken(just_seen_a_not_keyword, EQUALITY);
                } else if (current_word == "unfrag") {
                    token = new IP6UnfragPrimitiveToken(just_seen_a_not_keyword, EQUALITY);
                } else {
                    errh->error("unkown keyword '%s' followed ip6, it should be followed by vers, plen, flow, nxt, dscp, ecn, ce, hlim, frag or unfrag.", current_word.c_str()); return -1;
                }
                tokens.push_back(token);
                just_seen_a_not_keyword = false;
            } else if (current_word == "tcp") {
                Token *token;
                skip_blanks_and_read_word(i, current_word, "no argument followed after tcp keyword");
                if (current_word == "opt") {
                    skip_blanks_and_read_word(i, current_word, "no argument followed tcp opt statement");
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = TCPOptFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = TCPOptFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                    tokens.push_back(token);
                    just_seen_a_not_keyword = false;
                } else if (current_word == "win") {
                    skip_blanks_and_read_word(i, current_word, "no argument followed after tcp win statement"); 
                    if (is_word_an_operator(current_word, an_operator) >= 0) {
                        skip_blanks_and_read_word(i, current_word, "operator was only followed by blanks, an operator must be followed by data");
                        token = TCPWinFactory::create_token(current_word, just_seen_a_not_keyword, an_operator);
                    } else {    // no operator was given, equality is assumed and the current word already contains the data
                        token = TCPWinFactory::create_token(current_word, just_seen_a_not_keyword, EQUALITY);
                    }
                    tokens.push_back(token);
                    just_seen_a_not_keyword = false;                                   
                } else {
                    errh->error("unkown keyword '%s' followed tcp, it should be opt or win", current_word.c_str()); return -1;
                }
            } else if (current_word == "true" || current_word == "-") {
                tokens.push_back(new TruePrimitiveToken(just_seen_a_not_keyword, EQUALITY));
                just_seen_a_not_keyword = false;
            } else if (current_word == "false") {
                tokens.push_back(new FalsePrimitiveToken(just_seen_a_not_keyword, EQUALITY));
                just_seen_a_not_keyword = false;            
            } else {
                errh->error("unkown keyword '%s' found, see the click manual page for the list of acceptabl keywords", current_word.c_str());
                return -1;        
            }

            if (i == -1) {  // end of line was seen after a word
                return 0;
            }
        } catch (String exception) {   // exception contains the error code
            errh->error(exception.c_str());
            return -1;
        }
    }
}

}; // namespace ipfiltering

CLICK_ENDDECLS
ELEMENT_PROVIDES(FilterL)

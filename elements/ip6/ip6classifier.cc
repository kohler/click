/*
 * ip6classifier.{cc,hh} -- IPv6-packet filter with tcpdumplike syntax
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
#include "ip6classifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/args.hh>
#include "ip6classifier_lexer.hh"
#include "ip6classifier_parser.hh"
#include <fstream>

CLICK_DECLS

IP6Classifier::IP6Classifier() {}

IP6Classifier::~IP6Classifier() {}

//
// CONFIGURATION
// In short: a) Take each pattern an divide it up into tokens
//           b) Pass the tokens to a Parser
//           c) The parser returns an Abstract Syntax Tree for each pattern
//           d) Use this Abstract Syntax Tree to match against packets in the push method
//

int
IP6Classifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    ErrorHandler *errh2 = ErrorHandler::default_handler();

    // Make an Abstract Syntax Tree for each output
    for (int i = 0; i < conf.size(); i++) {
        ip6classification::Lexer lexer(conf[i]);
        Vector<ip6classification::Token*> tokens;
        int success = lexer.lex(tokens, errh2);
        if (success >= 0) {
// USE BELOW FOR DEBUGGING TOKENS
//            click_chatter("The tokens in the list are: ");
//            for (int i = 0; i < tokens.size(); i++) {
//                tokens[i]->print_name();
//            }
            ip6classification::Parser parser(tokens);
            ip6classification::AST ast; // an abstract syntax tree
            success = parser.parse(ast, errh2);
            
            if (success >= 0) {
// USE BELOW FOR DEBUGGING ALREADY COMBINED TOKENS
//                ast.print();
                ast_list.push_back(ast);    // add this AST to the list of ASTs
            }
        }
    }
    return 0;
}

//
// RUNNING
// Here we do what was above described in d).
//

void
IP6Classifier::push(int, Packet *p)
{
    for (int i = 0; i < ast_list.size(); i++) {     // for each AST (abstract syntax tree) of the abstract syntax list do
        const bool matches = ast_list[i].check_whether_packet_matches(p);
        if (matches) {
            output(i).push(p);
            return;
        }
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IP6Classifier)

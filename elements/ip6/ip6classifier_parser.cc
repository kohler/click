/*
 * ip6classification_parser.{cc,hh} -- Parser for IP-packet filter with tcpdumplike syntax
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

#include "ip6classifier_parser.hh"
CLICK_DECLS

namespace ip6classification {

/*
 * @brief This function constructs a Lexer object, and initializes it with a to be lexed string.
 * @param to_be_lexed_string This is the string that needs to be lexed and split up in tokens.
 */
Parser::Parser(Vector<Token*> to_be_processed_tokens) {
    this->to_be_processed_tokens = to_be_processed_tokens;
}

/*
 * @brief This function is the objects destructor. It does nothing special.
 * Since the class does not contain pointers, nothing needs to be freed.
 */
Parser::~Parser() {
}

/*
 * @brief This function interprets the given tokens and forms an AST with them.
 * The tokens are given as an input and this function makes an AST out of them.
 * This AST consists of tokens which are interconnected to each other in a tree
 * struture.
 * @param ast The actual abstract syntax tree.
 * @return return 0 on success, -1 on failure
 */
int
Parser::parse(AST &ast, ErrorHandler *errh) {
    if (dynamic_cast<CombinerToken*> (to_be_processed_tokens[0])) {
        errh->error("The first token in the list may not be a combiner token (i.e., and, or)"); return -1;
    }
    for (int i = 0; i < to_be_processed_tokens.size(); i++) {
        int success = parse_stack.push_on_stack_and_possibly_evaluate(to_be_processed_tokens[i], errh); // pushes the token on the stack and evaluates if needed
        if (success < 0) {
            return -1;
        }
    }
    parse_stack.push_on_stack_and_possibly_evaluate(new EndOfLineToken(), errh);    // We end every parse expression with an EndOfLineToken
    return parse_stack.get_AST(ast, errh);
}

};

CLICK_ENDDECLS
ELEMENT_PROVIDES(IP6ClassificationParser)

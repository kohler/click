/*
 * click.y -- sample Yacc grammar for the Click configuration language
 * Eddie Kohler
 *
 * Copyright (c) 2011 Regents of the University of California
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

%token IDENTIFIER	/* "a" */
%token CLASSNAME_IDENTIFIER
			/* IDENTIFIER currently defined as a class name */
%token PARAMETER_IDENTIFIER
			/* IDENTIFIER followed by a PARAMETER
			   (otherwise would require 2-token lookahead) */
%token NUMBER		/* "1" */
%token PARAMETER	/* "$a" */
%token CONFIGSTRING	/* string between parentheses */
%token COLONCOLON
%token BARBAR
%token DOTDOTDOT
%token ARROW
%token ARROW2
%token ELEMENTCLASS
%token REQUIRE
%token DEFINE

%expect 10

%%

/* Useless input and output ports are accepted by this grammar.  The
   following strings parse, but should cause errors:

   [0] a -> b;
   a -> b [1];
   [0] a :: A;
   a :: A [0];

   Higher-level processing should detect and report these errors.

   Similarly, an output port following an explicit declaration, as in "a ::
   A [0]", should be accepted only if exactly one element was declared.

   The grammar will parse a standalone declaration like "a, b, c :: Class;"
   as "(a), (b), (c :: Class)".  However, for standalone declarations (and
   standalone declarations only), the Class declaration applies to all
   named elements. */

stmts:	stmts stmt
	| empty;

stmt:	connection
	| elementclassstmt
	| requirestmt
	| definestmt
	| ';';

connection:
	elements conntail
	| conntail;

conntail:
	ARROW elements conntail
	| ARROW2 elements conntail
	| ARROW
	| ARROW2;

elements:
	element
	| elements ',' element;

element:
	element_reference opt_port;
	| port element_reference opt_port;

element_reference:
	element_name
	| element_name COLONCOLON class opt_config
	| class opt_config
	| group;

opt_config:
	'(' CONFIGSTRING ')'
	| empty;

opt_port:
	port
	| empty;

port:	'[' ']'
	| '[' port_numbers ']'
	| '[' port_numbers ',' ']';

port_numbers:
	NUMBER
	| port_numbers ',' NUMBER;

element_name:
	IDENTIFIER;

class:	CLASSNAME_IDENTIFIER
	| '{' compounds '}'
	| '{' compounds BARBAR DOTDOTDOT '}';

group:	'(' stmts ')';

compounds:
	compound
	| compounds BARBAR compound;

compound:
	stmts
	| opt_formals '|' stmts;

opt_formals:
	formals
	| empty;

formals:
	formal
	| formals ',' formal;

formal:	PARAMETER
	| PARAMETER_IDENTIFIER PARAMETER;

elementclassstmt:
	ELEMENTCLASS IDENTIFIER class;

requirestmt:
	REQUIRE '(' CONFIGSTRING ')';

definestmt:
	DEFINE '(' CONFIGSTRING ')';

empty:	/* empty */;


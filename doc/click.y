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
%token ELEMENTCLASS
%token REQUIRE
%token DEFINE

%%

stmts:	stmts stmt
	| empty;

stmt:	connection
	| multi_declaration
	| elementclassstmt
	| requirestmt
	| definestmt
	| ';';

connection:
	element conntail;

conntail:
	opt_port ARROW opt_port connection
	| empty;

element:
	element_name
	| element_name COLONCOLON class opt_config
	| class opt_config;

opt_config:
	'(' CONFIGSTRING ')'
	| empty;

opt_port:
	'[' NUMBER ']'
	| empty;

multi_declaration:
	multi_element_names COLONCOLON class opt_config;

multi_element_names:
	element_name ',' element_names;

element_names:
	element_name
	| element_names ',' element_name;

element_name:
	IDENTIFIER;

class:	CLASSNAME_IDENTIFIER
	| '{' compounds '}';
	| '{' compounds BARBAR DOTDOTDOT '}';

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


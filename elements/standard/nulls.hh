#ifndef NULLS_HH
#define NULLS_HH
#include "element.hh"

/*
 * =c
 * Null1()
 * Null2()
 * Null3()
 * Null4()
 * Null5()
 * Null6()
 * Null7()
 * Null8()
 * =d
 * Copies of Null that do not share code.
 * =a Null
 */

class Null1 : public Element {
  
 public:
  
  Null1()					{ add_input(); add_output(); }
  
  const char *class_name() const		{ return "Null1"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Null1 *clone() const				{ return new Null1; }

  void push(int, Packet *p)			{ output(0).push(p); }
  Packet *pull(int)				{ return input(0).pull(); }
  
};

class Null2 : public Element {
  
 public:
  
  Null2()					{ add_input(); add_output(); }
  
  const char *class_name() const		{ return "Null2"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Null2 *clone() const				{ return new Null2; }

  void push(int, Packet *p)			{ output(0).push(p); }
  Packet *pull(int)				{ return input(0).pull(); }
  
};

class Null3 : public Element {
  
 public:
  
  Null3()					{ add_input(); add_output(); }
  
  const char *class_name() const		{ return "Null3"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Null3 *clone() const				{ return new Null3; }

  void push(int, Packet *p)			{ output(0).push(p); }
  Packet *pull(int)				{ return input(0).pull(); }
  
};

class Null4 : public Element {
  
 public:
  
  Null4()					{ add_input(); add_output(); }
  
  const char *class_name() const		{ return "Null4"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Null4 *clone() const				{ return new Null4; }

  void push(int, Packet *p)			{ output(0).push(p); }
  Packet *pull(int)				{ return input(0).pull(); }
  
};

class Null5 : public Element {
  
 public:
  
  Null5()					{ add_input(); add_output(); }
  
  const char *class_name() const		{ return "Null5"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Null5 *clone() const				{ return new Null5; }

  void push(int, Packet *p)			{ output(0).push(p); }
  Packet *pull(int)				{ return input(0).pull(); }
  
};

class Null6 : public Element {
  
 public:
  
  Null6()					{ add_input(); add_output(); }
  
  const char *class_name() const		{ return "Null6"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Null6 *clone() const				{ return new Null6; }

  void push(int, Packet *p)			{ output(0).push(p); }
  Packet *pull(int)				{ return input(0).pull(); }
  
};

class Null7 : public Element {
  
 public:
  
  Null7()					{ add_input(); add_output(); }
  
  const char *class_name() const		{ return "Null7"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Null7 *clone() const				{ return new Null7; }

  void push(int, Packet *p)			{ output(0).push(p); }
  Packet *pull(int)				{ return input(0).pull(); }
  
};

class Null8 : public Element {
  
 public:
  
  Null8()					{ add_input(); add_output(); }
  
  const char *class_name() const		{ return "Null8"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Null8 *clone() const				{ return new Null8; }

  void push(int, Packet *p)			{ output(0).push(p); }
  Packet *pull(int)				{ return input(0).pull(); }
  
};

#endif

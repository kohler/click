#ifndef TEE_HH
#define TEE_HH
#include "element.hh"

/*
 * =c
 * Tee([N])
 *
 * PullTee([N])
 * =d
 * Tee sends a copy of each incoming packet out each output.
 * The optional argument controls how many outputs the Tee
 * has, and defaults to 2.
 *
 * PullTee has one pull output and N-1 push outputs.
 * Each time the pull output pulls a packet, the PullTee
 * sends a copy out the push outputs.
 *
 * =a Broadcast
 */

class Tee : public Element {
  
 public:
  
  Tee()						: Element(1, 2) { }
  
  const char *class_name() const		{ return "Tee"; }
  Processing default_processing() const	{ return PUSH; }
  
  Tee *clone() const;
  int configure(const String &, ErrorHandler *);
  
  void push(int, Packet *);
  
};

class PullTee : public Element {
  
 public:
  
  PullTee()					: Element(1, 2) { }
  explicit PullTee(int n)			: Element(1, n) { }
  
  const char *class_name() const		{ return "PullTee"; }
  void processing_vector(Vector<int>&, int, Vector<int>&, int) const;
  
  PullTee *clone() const;
  int configure(const String &, ErrorHandler *);
  
  Packet *pull(int);
  
};

#endif

#ifndef TEE_HH
#define TEE_HH
#include <click/element.hh>

/*
 * =c
 * Tee([N])
 *
 * PullTee([N])
 * =s
 * duplicates packets
 * V<duplication>
 * =d
 * Tee sends a copy of each incoming packet out each output.
 *
 * PullTee's input and its first output are pull; its other outputs are push.
 * Each time the pull output pulls a packet, it
 * sends a copy out the push outputs.
 *
 * By default, Tee and PullTee have an unlimited number of outputs,
 * but you can set a specific number of outputs by giving the optional
 * argument N.
 */

class Tee : public Element {
  
 public:
  
  Tee();
  ~Tee();
  
  const char *class_name() const		{ return "Tee"; }
  const char *processing() const		{ return PUSH; }
  
  Tee *clone() const;
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  
  void push(int, Packet *);
  
};

class PullTee : public Element {
  
 public:
  
  PullTee();
  ~PullTee();
  
  const char *class_name() const		{ return "PullTee"; }
  const char *processing() const		{ return "l/lh"; }
  
  PullTee *clone() const;
  void notify_noutputs(int);
  int configure(const Vector<String> &, ErrorHandler *);
  
  Packet *pull(int);
  
};

#endif

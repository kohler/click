#ifndef IPRWPATTERNS_HH
#define IPRWPATTERNS_HH
#include "elements/ip/iprewriter.hh"

/*
 * =c
 * IPRewriterPatterns(NAME PATTERN, ...)
 * =io
 * None
 * =d
 *
 * This element stores information about shared patterns that IPRewriter and
 * related elements can use. Each configuration argument is a name and a
 * pattern, `NAME SADDR SPORT DADDR DPORT'. The NAMEs for every argument in
 * every IPRewriterPatterns element in the configuration must be distinct.
 *
 * =a IPRewriter
 */

class IPRewriterPatterns : public Element {

  HashMap<String, int> _name_map;
  Vector<IPRewriter::Pattern *> _patterns;

 public:

  IPRewriterPatterns();
  
  const char *class_name() const	{ return "IPRewriterPatterns"; }
  IPRewriterPatterns *clone() const	{ return new IPRewriterPatterns; }

  int configure_phase() const	{ return IPRewriter::CONFIGURE_PHASE_PATTERNS;}
  int configure(const Vector<String> &, ErrorHandler *);
  void uninitialize();

  static IPRewriter::Pattern *find(Element *, const String &, ErrorHandler *);
  
};

#endif

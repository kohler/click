#ifndef CLICK_LEXERTINFO_HH
#define CLICK_LEXERTINFO_HH
#include "lexert.hh"

class LexerTInfo { public:

  typedef int pos1, pos2;
  
  LexerTInfo()					{ }
  
  virtual void notify_comment(int, int) { }
  virtual void notify_line_directive(int, int) { }
  virtual void notify_keyword(const String &, int, int) { }
  virtual void notify_config_string(int, int) { }
  
};

#endif

// -*- c-basic-offset: 4 -*-
#ifndef CLICK_LEXERTINFO_HH
#define CLICK_LEXERTINFO_HH
#include "lexert.hh"
class ElementT;
class ElementClassT;

class LexerTInfo { public:

    LexerTInfo() { }
    
    virtual void notify_comment(int pos1, int pos2);
    virtual void notify_error(const String &message, int pos1, int pos2);
    virtual void notify_line_directive(int pos1, int pos2);
    virtual void notify_keyword(const String &keyword, int pos1, int pos2);
    virtual void notify_config_string(int pos1, int pos2);
    virtual void notify_class_declaration(ElementClassT *, bool anonymous,
					  int pos1, int name_pos1, int pos2);
    virtual void notify_class_extension(ElementClassT *, int pos1, int pos2);
    virtual void notify_class_reference(ElementClassT *, int pos1, int pos2);
    virtual void notify_element_declaration(
		ElementT *e,
		ElementClassT *owner, int pos1, int name_pos2, int decl_pos2);
    virtual void notify_element_reference(
		ElementT *e,
		ElementClassT *owner, int pos1, int pos2);
  
};

#endif

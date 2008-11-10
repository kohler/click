// -*- c-basic-offset: 4 -*-
#ifndef CLICK_LEXERTINFO_HH
#define CLICK_LEXERTINFO_HH
#include "lexert.hh"
class ElementT;
class ElementClassT;

class LexerTInfo { public:

    LexerTInfo()		{ }
    virtual ~LexerTInfo()	{ }

    virtual void notify_comment(const char *pos1, const char *pos2);
    virtual void notify_error(const String &message, const char *pos1, const char *pos2);
    virtual void notify_line_directive(const char *pos1, const char *pos2);
    virtual void notify_keyword(const String &keyword, const char *pos1, const char *pos2);
    virtual void notify_config_string(const char *pos1, const char *pos2);
    virtual void notify_class_declaration(ElementClassT *, bool anonymous,
		const char *pos1, const char *name_pos1, const char *pos2);
    virtual void notify_class_extension(ElementClassT *, const char *pos1, const char *pos2);
    virtual void notify_class_reference(ElementClassT *, const char *pos1, const char *pos2);
    virtual void notify_element_declaration(
		ElementT *e, const char *pos1, const char *name_pos2, const char *decl_pos2);
    virtual void notify_element_reference(
		ElementT *e, const char *pos1, const char *pos2);

};

#endif

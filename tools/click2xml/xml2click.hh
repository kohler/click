// -*- c-basic-offset: 4 -*-
#ifndef CLICK_XML2CLICK_HH
#define CLICK_XML2CLICK_HH

struct CxElement {
    String name;
    String class_name;
    String class_id;
    String config;
    int ninputs;
    int noutputs;
    String landmark;
    String xml_landmark;
    CxElement()			: ninputs(-1), noutputs(-1) { }
};

struct CxConnection {
    String from;
    int fromport;
    String to;
    int toport;
    String xml_landmark;
    CxConnection()		: fromport(0), toport(0) { }
};

struct CxConfig {
    Vector<CxElement> _elements;
    Vector<CxConnection> _connections;

    Vector<String> _formals;
    Vector<String> _formal_types;
    CxConfig *_enclosing;
    int _depth;
    String _name;
    String _id;
    String _prev_class_name;
    String _prev_class_id;
    bool _is_synonym;
    bool _filled;
    String _landmark;
    String _xml_landmark;

    int _decl_ninputs;
    int _decl_noutputs;
    int _decl_nformals;

    ElementClassT *_type;
    RouterT *_router;
    bool _completing;

    CxConfig(CxConfig *enclosing, const String &xml_landmark);
    ~CxConfig();

    String readable_name() const;
    RouterT *router(ErrorHandler *);
    int complete_elementclass(ErrorHandler *);
    int complete(ErrorHandler *);
};

#endif

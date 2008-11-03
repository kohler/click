// -*- c-basic-offset: 4; related-file-name: "../../lib/archive.cc" -*-
#ifndef CLICK_ARCHIVE_HH
#define CLICK_ARCHIVE_HH
#include <click/string.hh>
#include <click/vector.hh>
CLICK_DECLS
class ErrorHandler;

struct ArchiveElement {

    String name;
    int date;
    int uid;
    int gid;
    int mode;
    String data;

    bool live() const			{ return name; }
    bool dead() const			{ return !name; }
    void kill()				{ name = String(); }

    static int parse(const String &str, Vector<ArchiveElement> &ar, ErrorHandler *errh = 0);
    static String unparse(const Vector<ArchiveElement> &ar, ErrorHandler *errh = 0);
    static int arindex(const Vector<ArchiveElement> &ar, const String &name);

};

CLICK_ENDDECLS
#endif

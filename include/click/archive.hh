// -*- c-basic-offset: 2; related-file-name: "../../lib/archive.cc" -*-
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
  
};

int separate_ar_string(const String &, Vector<ArchiveElement> &,
		       ErrorHandler * = 0);
String create_ar_string(const Vector<ArchiveElement> &, ErrorHandler * = 0);

CLICK_ENDDECLS
#endif

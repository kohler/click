#ifndef ARCHIVE_HH
#define ARCHIVE_HH
#include "string.hh"
#include "vector.hh"
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

#endif


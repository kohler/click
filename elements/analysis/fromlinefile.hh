// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_FROMLINEFILE_HH
#define CLICK_FROMLINEFILE_HH
#include <click/string.hh>
#include <click/vector.hh>
#include <stdio.h>
CLICK_DECLS
class ErrorHandler;
class Element;
class Packet;
class WritablePacket;

class FromLineFile { public:

    FromLineFile();
    ~FromLineFile()			{ cleanup(); }

    const String &filename() const	{ return _filename; }
    String &filename()			{ return _filename; }

    void set_landmark_pattern(const String &lp) { _landmark_pattern = lp; }
    String landmark(const String &landmark_pattern) const;
    String landmark() const		{ return landmark(_landmark_pattern); }
    String print_filename() const;
    int lineno() const			{ return _lineno; }
    void set_lineno(int lineno)		{ _lineno = lineno; }

    int initialize(ErrorHandler *);
    void add_handlers(Element *) const;
    void cleanup();
    
    int read_line(String &, ErrorHandler *); // result null-terminated
    int peek_line(String &, ErrorHandler *); // result not null-terminated

    int error(ErrorHandler *, const char *format, ...) const;
    int warning(ErrorHandler *, const char *format, ...) const;
    
    const uint8_t *get_unaligned(size_t, ErrorHandler * = 0);
    
  private:

    enum { BUFFER_SIZE = 32768 };
    
    int _fd;
    char *_buffer;
    int _pos;
    int _len;
    int _buffer_len;
    int _save_char;
    int _lineno;

    String _filename;
    String _landmark_pattern;
    FILE *_pipe;
    off_t _file_offset;

    int read_buffer(ErrorHandler *);

    static String filename_handler(Element *, void *);
    static String filesize_handler(Element *, void *);
    static String filepos_handler(Element *, void *);
    
};

CLICK_ENDDECLS
#endif

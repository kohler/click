// -*- related-file-name: "../../lib/fromfile.cc"; c-basic-offset: 4 -*-
#ifndef CLICK_FROMFILE_HH
#define CLICK_FROMFILE_HH
#include <click/string.hh>
#include <click/vector.hh>
#include <stdio.h>
CLICK_DECLS
class ErrorHandler;
class Element;
class Packet;
class WritablePacket;

class FromFile { public:

    FromFile();
    ~FromFile()				{ cleanup(); }

    const String &filename() const	{ return _filename; }
    String &filename()			{ return _filename; }
    bool initialized() const		{ return _fd >= 0; }

    void set_landmark_pattern(const String &lp) { _landmark_pattern = lp; }
    String landmark(const String &landmark_pattern) const;
    String landmark() const		{ return landmark(_landmark_pattern); }
    String print_filename() const;
    int lineno() const			{ return _lineno; }
    void set_lineno(int lineno)		{ _lineno = lineno; }

    off_t file_pos() const		{ return _file_offset + _pos; }

    int configure_keywords(Vector<String> &conf, Element *, ErrorHandler *);
    int initialize(ErrorHandler *, bool allow_nonexistent = false);
    void add_handlers(Element *, bool filepos_writable = false) const;
    void cleanup();
    void take_state(FromFile &, ErrorHandler *);

    int seek(off_t want, ErrorHandler *);

    int read(void *, uint32_t, ErrorHandler * = 0);
    const uint8_t *get_unaligned(size_t, void *, ErrorHandler * = 0);
    const uint8_t *get_aligned(size_t, void *, ErrorHandler * = 0);
    String get_string(size_t, ErrorHandler * = 0);
    Packet *get_packet(size_t, uint32_t sec, uint32_t subsec, ErrorHandler *);
    Packet *get_packet_from_data(const void *buf, size_t buf_size, size_t full_size, uint32_t sec, uint32_t subsec, ErrorHandler *);
    void shift_pos(int delta)		{ _pos += delta; }

    int read_line(String &str, ErrorHandler *errh, bool temporary = false);
    int peek_line(String &str, ErrorHandler *errh, bool temporary = false);

    int error(ErrorHandler *, const char *format, ...) const;
    int warning(ErrorHandler *, const char *format, ...) const;

  private:

    enum { BUFFER_SIZE = 32768 };

    int _fd;
    const uint8_t *_buffer;
    uint32_t _pos;
    uint32_t _len;

    WritablePacket *_data_packet;

#ifdef ALLOW_MMAP
    bool _mmap;
#endif

#ifdef ALLOW_MMAP
    enum { WANT_MMAP_UNIT = 4194304 }; // 4 MB
    size_t _mmap_unit;
    off_t _mmap_off;
#endif

    String _filename;
    FILE *_pipe;
    off_t _file_offset;
    String _landmark_pattern;
    int _lineno;

#ifdef ALLOW_MMAP
    int read_buffer_mmap(ErrorHandler *);
#endif
    int read_buffer(ErrorHandler *);
    bool read_packet(ErrorHandler *);
    int skip_ahead(ErrorHandler *);

    static String filename_handler(Element *, void *);
    static String filesize_handler(Element *, void *);
    static String filepos_handler(Element *, void *);
    static int filepos_write_handler(const String&, Element*, void*, ErrorHandler*);

};

CLICK_ENDDECLS
#endif

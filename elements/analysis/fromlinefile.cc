// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fromtcpdump.{cc,hh} -- element reads packets from IP summary dump file
 * Eddie Kohler
 *
 * Copyright (c) 2003 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include "fromlinefile.hh"
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <click/element.hh>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
CLICK_DECLS

FromLineFile::FromLineFile()
    : _fd(-1), _buffer(0), _pos(0), _len(0), _buffer_len(0),
      _landmark_pattern("%f:%l"), _pipe(0)
{
}

String
FromLineFile::print_filename() const
{
    if (!_filename || _filename == "-")
	return String::stable_string("<stdin>", 7);
    else
	return _filename;
}

String
FromLineFile::landmark(const String &landmark_pattern) const
{
    StringAccum sa;
    const char *e = landmark_pattern.end();
    for (const char *s = landmark_pattern.begin(); s < e; s++)
	if (s < e - 1 && s[0] == '%' && s[1] == 'f') {
	    sa << print_filename();
	    s++;
	} else if (s < e - 1 && s[0] == '%' && s[1] == 'l') {
	    sa << _lineno;
	    s++;
	} else if (s < e - 1 && s[0] == '%' && s[1] == '%') {
	    sa << '%';
	    s++;
	} else
	    sa << *s;
    return sa.take_string();
}

int
FromLineFile::error(ErrorHandler *errh, const char *format, ...) const
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    va_list val;
    va_start(val, format);
    errh->verror(ErrorHandler::ERR_ERROR, landmark(), format, val);
    va_end(val);
    return ErrorHandler::ERROR_RESULT;
}

int
FromLineFile::warning(ErrorHandler *errh, const char *format, ...) const
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    va_list val;
    va_start(val, format);
    errh->verror(ErrorHandler::ERR_WARNING, landmark(), format, val);
    va_end(val);
    return ErrorHandler::ERROR_RESULT;
}

int
FromLineFile::read_buffer(ErrorHandler *errh)
{
    if (_pos == 0 && _len == _buffer_len) {
	_buffer_len += BUFFER_SIZE;
	if (!(_buffer = (char *)realloc(_buffer, _buffer_len)))
	    return error(errh, strerror(ENOMEM));
    }

    if (_len == _buffer_len) {
	memmove(_buffer, _buffer + _pos, _len - _pos);
	_len -= _pos;
	_file_offset += _pos;
	_pos = 0;
    }
    int initial_len = _len;
    
    while (_len < _buffer_len) {
	ssize_t got = read(_fd, _buffer + _len, _buffer_len - _len);
	if (got > 0)
	    _len += got;
	else if (got == 0)	// premature end of file
	    return _len - initial_len;
	else if (got < 0 && errno != EINTR && errno != EAGAIN)
	    return error(errh, strerror(errno));
    }
    
    return _len - initial_len;
}

int
FromLineFile::read_line(String &result, ErrorHandler *errh)
{
    int epos = _pos;
    if (_save_char)
	_buffer[epos] = _save_char;
    
    while (1) {
	bool done = false;
	
	if (epos >= _len) {
	    int delta = epos - _pos;
	    int errcode = read_buffer(errh);
	    if (errcode < 0 || (errcode == 0 && delta == 0))	// error
		return errcode;
	    else if (errcode == 0)
		done = true;
	    epos = _pos + delta;
	}

	while (epos < _len && _buffer[epos] != '\n' && _buffer[epos] != '\r')
	    epos++;

	if (epos < _len || done) {
	    if (epos < _len && _buffer[epos] == '\r')
		epos++;
	    if (epos < _len && _buffer[epos] == '\n')
		epos++;

	    // add terminating '\0'
	    if (epos == _buffer_len) {
		_buffer_len += BUFFER_SIZE;
		if (!(_buffer = (char *)realloc(_buffer, _buffer_len)))
		    return error(errh, strerror(ENOMEM));
	    }
	    _save_char = _buffer[epos];
	    _buffer[epos] = '\0';

	    result = String::stable_string(_buffer + _pos, epos - _pos);
	    _pos = epos;
	    _lineno++;
	    return 1;
	}
    }
}

int
FromLineFile::peek_line(String &result, ErrorHandler *errh)
{
    int before_pos = _pos;
    int retval = read_line(result, errh);
    if (retval > 0) {
	if (_save_char)
	    _buffer[_pos] = _save_char;
	_save_char = 0;
	_pos = before_pos;
	_lineno--;
    }
    return retval;
}

const uint8_t *
FromLineFile::get_unaligned(size_t size, ErrorHandler *errh)
{
    if (_save_char) {
	_buffer[_pos] = _save_char;
	_save_char = 0;
    }
    while (_pos + (int)size > _len) {
	if (read_buffer(errh) <= 0)
	    return 0;
    }
    const char *chunk = _buffer + _pos;
    _pos += size;
    return reinterpret_cast<const uint8_t *>(chunk);
}

int
FromLineFile::initialize(ErrorHandler *errh)
{
    _pipe = 0;
    if (_filename == "-")
	_fd = STDIN_FILENO;
    else
	_fd = open(_filename.cc(), O_RDONLY);

  retry_file:
    if (_fd < 0)
	return error(errh, strerror(errno));

    _pos = _len = _file_offset = _save_char = _lineno = 0;
    int result = read_buffer(errh);
    if (result < 0)
	return -1;
    else if (result == 0)
	return error(errh, "empty file");

    // check for a gziped or bzip2d dump
    if (_fd == STDIN_FILENO || _pipe)
	/* cannot handle gzip or bzip2 */;
    else if (compressed_data(reinterpret_cast<const unsigned char *>(_buffer), _len)) {
	close(_fd);
	_fd = -1;
	if (!(_pipe = open_uncompress_pipe(_filename, reinterpret_cast<const unsigned char *>(_buffer), _len, errh)))
	    return -1;
	_fd = fileno(_pipe);
	goto retry_file;
    }

    return 0;
}

void
FromLineFile::cleanup()
{
    if (_pipe)
	pclose(_pipe);
    else if (_fd >= 0 && _fd != STDIN_FILENO)
	close(_fd);
    _fd = -1;
    _pipe = 0;
    free(_buffer);
    _buffer = 0;
}

String
FromLineFile::filename_handler(Element *e, void *thunk)
{
    FromLineFile *fd = reinterpret_cast<FromLineFile *>((uint8_t *)e + (intptr_t)thunk);
    return fd->print_filename() + "\n";
}

String
FromLineFile::filesize_handler(Element *e, void *thunk)
{
    FromLineFile *fd = reinterpret_cast<FromLineFile *>((uint8_t *)e + (intptr_t)thunk);
    struct stat s;
    if (fd->_fd >= 0 && fstat(fd->_fd, &s) >= 0 && S_ISREG(s.st_mode))
	return String(s.st_size) + "\n";
    else
	return "-\n";
}

String
FromLineFile::filepos_handler(Element *e, void *thunk)
{
    FromLineFile *fd = reinterpret_cast<FromLineFile *>((uint8_t *)e + (intptr_t)thunk);
    return String(fd->_file_offset + fd->_pos) + "\n";
}

void
FromLineFile::add_handlers(Element *e) const
{
    intptr_t offset = (const uint8_t *)this - (const uint8_t *)e;
    e->add_read_handler("filename", filename_handler, (void *)offset);
    e->add_read_handler("filesize", filesize_handler, (void *)offset);
    e->add_read_handler("filepos", filepos_handler, (void *)offset);
}

ELEMENT_REQUIRES(userlevel)
ELEMENT_PROVIDES(FromLineFile)
CLICK_ENDDECLS

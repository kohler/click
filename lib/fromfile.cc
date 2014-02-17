// -*- related-file-name: "../include/click/fromfile.hh"; c-basic-offset: 4 -*-
/*
 * fromfile.{cc,hh} -- provides convenient, fast access to files
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2001-2003 International Computer Science Institute
 * Copyright (c) 2004-2007 The Regents of the University of California
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
#include <click/fromfile.hh>
#include <click/args.hh>
#include <click/error.hh>
#include <click/element.hh>
#include <click/straccum.hh>
#include <click/userutils.hh>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef ALLOW_MMAP
# include <sys/mman.h>
#endif
CLICK_DECLS

FromFile::FromFile()
    : _fd(-1), _buffer(0), _data_packet(0),
#ifdef ALLOW_MMAP
      _mmap(true),
#endif
      _filename(), _pipe(0), _landmark_pattern("%f"), _lineno(0)
{
}

int
FromFile::configure_keywords(Vector<String> &conf, Element *e, ErrorHandler *errh)
{
#ifndef ALLOW_MMAP
    bool mmap = false;
#else
    bool mmap = _mmap;
#endif
    if (Args(e, errh).bind(conf)
	.read("MMAP", mmap)
	.consume() < 0)
	return -1;
#ifdef ALLOW_MMAP
    _mmap = mmap;
#else
    if (mmap)
	errh->warning("'MMAP true' is not supported on this platform");
#endif
    return 0;
}

String
FromFile::print_filename() const
{
    if (!_filename || _filename == "-")
	return String::make_stable("<stdin>", 7);
    else
	return _filename;
}

String
FromFile::landmark(const String &landmark_pattern) const
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
FromFile::error(ErrorHandler *errh, const char *format, ...) const
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    va_list val;
    va_start(val, format);
    int r = errh->xmessage(landmark(), ErrorHandler::e_error, format, val);
    va_end(val);
    return r;
}

int
FromFile::warning(ErrorHandler *errh, const char *format, ...) const
{
    if (!errh)
	errh = ErrorHandler::default_handler();
    va_list val;
    va_start(val, format);
    int r = errh->xmessage(landmark(), ErrorHandler::e_warning_annotated, format, val);
    va_end(val);
    return r;
}

#ifdef ALLOW_MMAP
static void
munmap_destructor(unsigned char *data, size_t amount, void *)
{
    if (munmap((caddr_t)data, amount) < 0)
	click_chatter("FromFile: munmap: %s", strerror(errno));
}

int
FromFile::read_buffer_mmap(ErrorHandler *errh)
{
    if (_mmap_unit == 0) {
	size_t page_size = getpagesize();
	_mmap_unit = (WANT_MMAP_UNIT / page_size) * page_size;
	_mmap_off = 0;
	// don't report most errors on the first time through
	errh = ErrorHandler::silent_handler();
    }

    // get length of file
    struct stat statbuf;
    if (fstat(_fd, &statbuf) < 0)
	return error(errh, "stat: %s", strerror(errno));

    // check for end of file
    // But return -1 if we have not mmaped before: it might be a pipe, not
    // true EOF.
    if (_mmap_off >= statbuf.st_size)
	return (_mmap_off == 0 ? -1 : 0);

    // actually mmap
    _len = _mmap_unit;
    if ((off_t)(_mmap_off + _len) > statbuf.st_size)
	_len = statbuf.st_size - _mmap_off;

    void *mmap_data = mmap(0, _len, PROT_READ, MAP_SHARED, _fd, _mmap_off);

    if (mmap_data == MAP_FAILED)
	return error(errh, "mmap: %s", strerror(errno));

    _data_packet = Packet::make((unsigned char *)mmap_data, _len, munmap_destructor, 0);
    _buffer = _data_packet->data();
    _file_offset = _mmap_off;
    _mmap_off += _len;

# ifdef HAVE_MADVISE
    // don't care about errors
    (void) madvise((caddr_t)mmap_data, _len, MADV_SEQUENTIAL);
# endif

    return 1;
}
#endif

int
FromFile::read_buffer(ErrorHandler *errh)
{
    if (_data_packet)
	_data_packet->kill();
    _data_packet = 0;

    _file_offset += _len;
    _pos -= _len;		// adjust _pos by _len: it might validly point
				// beyond _len
    _len = 0;

    if (_fd < 0)
	return -EBADF;

#ifdef ALLOW_MMAP
    if (_mmap) {
	int result = read_buffer_mmap(errh);
	if (result >= 0)
	    return result;
	// else, try a regular read
	_mmap = false;
	(void) lseek(_fd, _mmap_off, SEEK_SET);
	_len = 0;
    }
#endif

    _data_packet = Packet::make(0, 0, BUFFER_SIZE, 0);
    if (!_data_packet)
	return error(errh, strerror(ENOMEM));
    _buffer = _data_packet->data();
    unsigned char *data = _data_packet->data();
    assert(_data_packet->headroom() == 0);

    while (_len < BUFFER_SIZE) {
	ssize_t got = ::read(_fd, data + _len, BUFFER_SIZE - _len);
	if (got > 0)
	    _len += got;
	else if (got == 0)	// premature end of file
	    return _len;
	else if (got < 0 && errno != EINTR && errno != EAGAIN)
	    return error(errh, strerror(errno));
    }

    return _len;
}

int
FromFile::read(void *vdata, uint32_t dlen, ErrorHandler *errh)
{
    unsigned char *data = reinterpret_cast<unsigned char *>(vdata);
    uint32_t dpos = 0;

    while (dpos < dlen) {
	if (_pos < _len) {
	    uint32_t howmuch = dlen - dpos;
	    if (howmuch > _len - _pos)
		howmuch = _len - _pos;
	    memcpy(data + dpos, _buffer + _pos, howmuch);
	    dpos += howmuch;
	    _pos += howmuch;
	}
	if (dpos < dlen && read_buffer(errh) <= 0)
	    return dpos;
    }

    return dlen;
}

int
FromFile::read_line(String &result, ErrorHandler *errh, bool temporary)
{
    // first, try to read a line from the current buffer
    const unsigned char *s = _buffer + _pos;
    const unsigned char *e = _buffer + _len;
    while (s < e && *s != '\n' && *s != '\r')
	s++;
    if (s < e && (*s == '\n' || s + 1 < e)) {
	s += (*s == '\r' && s[1] == '\n' ? 2 : 1);
	int new_pos = s - _buffer;
	if (temporary)
	    result = String::make_stable((const char *) (_buffer + _pos), new_pos - _pos);
	else
	    result = String((const char *) (_buffer + _pos), new_pos - _pos);
	_pos = new_pos;
	_lineno++;
	return 1;
    }

    // otherwise, build up a line
    StringAccum sa;
    sa.append(_buffer + _pos, _len - _pos);

    while (1) {
	int errcode = read_buffer(errh);
	if (errcode < 0 || (errcode == 0 && !sa))
	    return errcode;

	// check doneness
	bool done;
	if (sa && sa.back() == '\r') {
	    if (_len > 0 && _buffer[0] == '\n')
		sa << '\n', _pos++;
	    done = true;
	} else if (errcode == 0) {
	    _pos = _len;
	    done = true;
	} else {
	    s = _buffer, e = _buffer + _len;
	    while (s < e && *s != '\n' && *s != '\r')
		s++;
	    if (s < e && (*s == '\n' || s + 1 < e)) {
		s += (*s == '\r' && s[1] == '\n' ? 2 : 1);
		sa.append(_buffer, s - _buffer);
		_pos = s - _buffer;
		done = true;
	    } else {
		sa.append(_buffer, _len);
		done = false;
	    }
	}

	if (done) {
	    result = sa.take_string();
	    _lineno++;
	    return 1;
	}
    }
}

int
FromFile::peek_line(String &result, ErrorHandler *errh, bool temporary)
{
    int before_pos = _pos;
    int retval = read_line(result, errh, temporary);
    if (retval > 0) {
	_pos = before_pos;
	_lineno--;
    }
    return retval;
}

int
FromFile::seek(off_t want, ErrorHandler* errh)
{
    if (want >= _file_offset && want < (off_t) (_file_offset + _len)) {
	_pos = want;
	return 0;
    }

#ifdef ALLOW_MMAP
    if (_mmap) {
	_mmap_off = (want / _mmap_unit) * _mmap_unit;
	_pos = _len + want - _mmap_off;
	return 0;
    }
#endif

    // check length of file
    struct stat statbuf;
    if (fstat(_fd, &statbuf) < 0)
	return error(errh, "stat: %s", strerror(errno));
    if (S_ISREG(statbuf.st_mode) && statbuf.st_size && want > statbuf.st_size)
	return errh->error("FILEPOS out of range");

    // try to seek
    if (lseek(_fd, want, SEEK_SET) != (off_t) -1) {
	_pos = _len;
	_file_offset = want - _len;
	return 0;
    }

    // otherwise, read data
    while ((off_t) (_file_offset + _len) < want && _len)
	if (read_buffer(errh) < 0)
	    return -1;
    _pos = want - _file_offset;
    return 0;
}

int
FromFile::initialize(ErrorHandler *errh, bool allow_nonexistent)
{
    // must set for allow_nonexistent case
    _pos = _len = 0;
    // open file
    if (!_filename || _filename == "-")
	_fd = STDIN_FILENO;
    else
	_fd = open(_filename.c_str(), O_RDONLY);
    if (_fd < 0) {
	int e = -errno;
	if (e != -ENOENT || !allow_nonexistent)
	    errh->error("%s: %s", print_filename().c_str(), strerror(-e));
	return e;
    }

  retry_file:
#ifdef ALLOW_MMAP
    _mmap_unit = 0;
#endif
    _file_offset = 0;
    _pos = _len = 0;
    int result = read_buffer(errh);
    if (result < 0)
	return -1;
    else if (result == 0) {
	if (!allow_nonexistent)
	    error(errh, "empty file");
	return -ENOENT;
    }

    // check for a gziped or bzip2d dump
    if (_fd == STDIN_FILENO || _pipe)
	/* cannot handle gzip or bzip2 */;
    else if (compressed_data(_buffer, _len)) {
	close(_fd);
	_fd = -1;
	if (!(_pipe = open_uncompress_pipe(_filename, _buffer, _len, errh)))
	    return -1;
	_fd = fileno(_pipe);
	goto retry_file;
    }

    return 0;
}

void
FromFile::take_state(FromFile &o, ErrorHandler *errh)
{
    _fd = o._fd;
    o._fd = -1;
    _pipe = o._pipe;
    o._pipe = 0;

    _buffer = o._buffer;
    _pos = o._pos;
    _len = o._len;

    _data_packet = o._data_packet;
    o._data_packet = 0;

#ifdef ALLOW_MMAP
    if (_mmap != o._mmap)
	errh->warning("different MMAP states");
    _mmap = o._mmap;
    _mmap_unit = o._mmap_unit;
    _mmap_off = o._mmap_off;
#else
    (void) errh;
#endif

    _file_offset = o._file_offset;
}

void
FromFile::cleanup()
{
    if (_pipe)
	pclose(_pipe);
    else if (_fd >= 0 && _fd != STDIN_FILENO)
	close(_fd);
    _pipe = 0;
    _fd = -1;
    if (_data_packet)
	_data_packet->kill();
    _data_packet = 0;
}

const uint8_t *
FromFile::get_aligned(size_t size, void *buffer, ErrorHandler *errh)
{
    // we may need to read bits of the file
    if (_pos + size <= _len) {
	const uint8_t *chunk = _buffer + _pos;
	_pos += size;
#if HAVE_INDIFFERENT_ALIGNMENT
	return reinterpret_cast<const uint8_t *>(chunk);
#else
	// make a copy if required for alignment
	if (((uintptr_t)(chunk) & 3) == 0)
	    return reinterpret_cast<const uint8_t *>(chunk);
	else {
	    memcpy(buffer, chunk, size);
	    return reinterpret_cast<uint8_t *>(buffer);
	}
#endif
    } else if (read(buffer, size, errh) == (int)size)
	return reinterpret_cast<uint8_t *>(buffer);
    else
	return 0;
}

const uint8_t *
FromFile::get_unaligned(size_t size, void *buffer, ErrorHandler *errh)
{
    // we may need to read bits of the file
    if (_pos + size <= _len) {
	const uint8_t *chunk = _buffer + _pos;
	_pos += size;
	return reinterpret_cast<const uint8_t *>(chunk);
    } else if (read(buffer, size, errh) == (int)size)
	return reinterpret_cast<uint8_t *>(buffer);
    else
	return 0;
}

String
FromFile::get_string(size_t size, ErrorHandler *errh)
{
    // we may need to read bits of the file
    if (_pos + size <= _len) {
	const uint8_t *chunk = _buffer + _pos;
	_pos += size;
	return String::make_stable((const char *) chunk, size);
    } else {
	String s = String::make_uninitialized(size);
	if (read(s.mutable_data(), size, errh) == (int) size)
	    return s;
	else
	    return String();
    }
}

Packet *
FromFile::get_packet(size_t size, uint32_t sec, uint32_t subsec, ErrorHandler *errh)
{
    if (_pos + size <= _len) {
	if (Packet *p = _data_packet->clone()) {
	    p->shrink_data(_buffer + _pos, size);
	    p->timestamp_anno().assign(sec, subsec);
	    _pos += size;
	    return p;
	}
    } else {
	if (WritablePacket *p = Packet::make(0, 0, size, 0)) {
	    if (read(p->data(), size, errh) < (int)size) {
		p->kill();
		return 0;
	    } else {
		p->timestamp_anno().assign(sec, subsec);
		return p;
	    }
	}
    }
    error(errh, strerror(ENOMEM));
    return 0;
}

Packet *
FromFile::get_packet_from_data(const void *data_void, size_t data_size, size_t size, uint32_t sec, uint32_t subsec, ErrorHandler *errh)
{
    const uint8_t *data = reinterpret_cast<const uint8_t *>(data_void);
    if (data >= _buffer && data + size <= _buffer + _len) {
	if (Packet *p = _data_packet->clone()) {
	    p->shrink_data(data, size);
	    p->timestamp_anno().assign(sec, subsec);
	    return p;
	}
    } else {
	if (WritablePacket *p = Packet::make(0, 0, size, 0)) {
	    memcpy(p->data(), data, data_size);
	    if (data_size < size
		&& read(p->data() + data_size, size - data_size, errh) != (int)(size - data_size)) {
		p->kill();
		return 0;
	    }
	    p->timestamp_anno().assign(sec, subsec);
	    return p;
	}
    }
    error(errh, strerror(ENOMEM));
    return 0;
}

String
FromFile::filename_handler(Element *e, void *thunk)
{
    FromFile *fd = reinterpret_cast<FromFile *>((uint8_t *)e + (intptr_t)thunk);
    return fd->print_filename();
}

String
FromFile::filesize_handler(Element *e, void *thunk)
{
    FromFile *fd = reinterpret_cast<FromFile *>((uint8_t *)e + (intptr_t)thunk);
    struct stat s;
    if (fd->_fd >= 0 && fstat(fd->_fd, &s) >= 0 && S_ISREG(s.st_mode))
	return String(s.st_size);
    else
	return "-";
}

String
FromFile::filepos_handler(Element* e, void* thunk)
{
    FromFile* fd = reinterpret_cast<FromFile*>((uint8_t*)e + (intptr_t)thunk);
    return String(fd->_file_offset + fd->_pos);
}

int
FromFile::filepos_write_handler(const String& str, Element* e, void* thunk, ErrorHandler* errh)
{
    off_t offset;
    if (!cp_file_offset(cp_uncomment(str), &offset))
	return errh->error("argument must be file offset");
    FromFile* fd = reinterpret_cast<FromFile*>((uint8_t*)e + (intptr_t)thunk);
    return fd->seek(offset, errh);
}

void
FromFile::add_handlers(Element* e, bool filepos_writable) const
{
    intptr_t offset = (const uint8_t *)this - (const uint8_t *)e;
    e->add_read_handler("filename", filename_handler, (void *)offset);
    e->add_read_handler("filesize", filesize_handler, (void *)offset);
    e->add_read_handler("filepos", filepos_handler, (void *)offset);
    if (filepos_writable)
	e->add_write_handler("filepos", filepos_write_handler, (void *)offset);
}

CLICK_ENDDECLS

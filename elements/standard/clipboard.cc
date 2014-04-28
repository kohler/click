// -*- c-basic-offset: 4 -*-
/*
 * clipboard.{cc,hh} -- copies data from one packet to another
 */
#include <click/config.h>
#include "clipboard.hh"
#include <click/args.hh>
#include <click/error.hh>
CLICK_DECLS


Clipboard::Clipboard()
{
}


int
Clipboard::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Vector<Range> ranges = Vector<Range>();
    Range range;
    int clipboardSize = 0;
    _minPacketLength = 0;

    for (int argNo = 0; argNo < conf.size(); argNo++) {
        String arg = conf[argNo];
        int i = arg.find_left('/');
        if (i <= 0 || i >= arg.length() - 1) {
            errh->error("range %d: expected '/' between offset and length", argNo);
            return -1;
        }

        if (Args(this, errh)
            .push_back(arg.substring(0, i))
            .push_back(arg.substring(i + 1))
            .read_mp("OFFSET", range.offset)
            .read_mp("LENGTH", range.length)
            .complete() < 0) {
            errh->error("range %d: invalid offset or length", argNo);
            return -1;
        }

        ranges.push_back(range);
        clipboardSize += range.length;
        if (range.offset + range.length > _minPacketLength)
            _minPacketLength = range.offset + range.length;
    }

    _ranges = ranges;
    _clipboard.resize(clipboardSize);
    return 0;
}


Packet *
Clipboard::pull(int port)
{
    Packet *p = input(port).pull();
    if (!p) return NULL;
    if (port == 0) copy(p);
    else p = paste(p);
    return p;
}


void
Clipboard::push(int port, Packet *p)
{
    if (port == 0) copy(p);
    else p = paste(p);
    output(port).push(p);
}


void
Clipboard::copy(Packet *p)
{
    // Configure guarantees us that _clipboard is big enough to hold all ranges.
    unsigned char *dst = &_clipboard[0];
    for (int i = 0; i < _ranges.size(); i++) {
        Range range = _ranges[i];
        const unsigned char *src = p->data() + range.offset;
        memcpy(dst, src, range.length);
        dst += range.length;
    }
}


Packet *
Clipboard::paste(Packet *p)
{
    if (p->length() < _minPacketLength) return p;

    WritablePacket *q = p->uniqueify();
    if (!q) return NULL;

    const unsigned char *src = &_clipboard[0];
    unsigned char *dst = q->data();

    for (int i = 0; i < _ranges.size(); i++) {
        Range range = _ranges[i];
        memcpy(dst + range.offset, src, range.length);
        src += range.length;
    }
    return q;
}


CLICK_ENDDECLS
EXPORT_ELEMENT(Clipboard)

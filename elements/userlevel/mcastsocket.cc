/*
 * mcastsocket.{cc,hh} -- transports packets via multicast UDP
 *
 * Copyright (c) 2004  The Trustees of Princeton University (Trustees).
 * Copyright (c) 2006-2007 Regents of the University of California
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
#include <click/error.hh>
#include <click/args.hh>
#include <click/glue.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>
#include <click/packet.hh>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include "mcastsocket.hh"

CLICK_DECLS


McastSocket::McastSocket()
  : _task(this),_recv_sock(-1), _send_sock(-1), _rq(NULL), _wq(NULL),
    _loop(true), _timestamp(true), _rcvbuf(-1), _snaplen(2048),
    _sndbuf(-1), _headroom(Packet::default_headroom)
{
}


McastSocket::~McastSocket()
{
}


int
McastSocket::configure(Vector<String> &conf, ErrorHandler *errh)
{
    IPAddress mcast_ip;
    uint16_t mcast_port;

    IPAddress source_ip;
    uint16_t source_port;

    Args args = Args(this, errh).bind(conf);
    if (args
        .read_mp("MCASTIP", mcast_ip)
        .read_mp("MCASTPORT", IPPortArg(IPPROTO_UDP), mcast_port)
        .read_p("SOURCEIP", source_ip)
        .read_p("SOURCEPORT", IPPortArg(IPPROTO_UDP), source_port)
        .read("SNAPLEN", _snaplen)
        .read("HEADROOM", _headroom)
        .read("TIMESTAMP", _timestamp)
        .read("RCVBUF", _rcvbuf)
        .read("SNDBUF", _sndbuf)
        .read("LOOP", _loop)
        .complete() < 0)
        return -1;

    _mcast.sin_family = AF_INET;
    _mcast.sin_port = htons(mcast_port);
    _mcast.sin_addr = mcast_ip.in_addr();

    _source.sin_family = AF_INET;
    _source.sin_port = htons(source_port);
    _source.sin_addr = source_ip.in_addr();

    return 0;
}


int
McastSocket::initialize_socket_error(ErrorHandler *errh, const char *syscall)
{
    int e = errno; // preserve errno
    cleanup();
    return errh->error("%s: %s", syscall, strerror(e));
}


int
McastSocket::initialize(ErrorHandler *errh)
{
#define SETSOCKOPT(sockfd, level, optname, optval) \
    if (setsockopt(sockfd, level, optname, &optval, sizeof optval) < 0) \
        return initialize_socket_error(errh, "setsockopt(" #optname ")")

    // Create sockets.
    _recv_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    _send_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (_recv_sock < 0 || _send_sock < 0)
        return initialize_socket_error(errh, "socket");

    // Set socket send buffer size.
    if (_sndbuf >= 0)
        SETSOCKOPT(_send_sock, SOL_SOCKET, SO_SNDBUF, _sndbuf);

    // Set socket receive buffer size.
    if (_rcvbuf >= 0)
        SETSOCKOPT(_recv_sock, SOL_SOCKET, SO_RCVBUF, _rcvbuf);

    // Allow multiple local listeners on mcast address.
    int one = 1;
    SETSOCKOPT(_recv_sock, SOL_SOCKET, SO_REUSEADDR, one);

    // Bind recv_sock to mcast address.
    if (bind(_recv_sock, (struct sockaddr *)&_mcast, sizeof _mcast) < 0)
        return initialize_socket_error(errh, "bind");

    if (_source.sin_addr.s_addr) {
        // Bind send_sock to source address.
        if (bind(_send_sock, (struct sockaddr *)&_source, sizeof _source) < 0)
            return initialize_socket_error(errh, "bind");

        // If source port is 0 (auto allocate), update address struct with actual port.
        if (_source.sin_port == 0) {
            socklen_t len = sizeof _source;
            if (getsockname(_send_sock, (sockaddr *)&_source, &len) != 0 || len != sizeof _source)
                return initialize_socket_error(errh, "getsockname");
        }

        // "Bind" send_sock to source interface.
        SETSOCKOPT(_send_sock, IPPROTO_IP, IP_MULTICAST_IF, _source.sin_addr);
    }

    // Join recv_sock to mcast address (on source interface, if specified, else any).
    struct ip_mreq mreq = { _mcast.sin_addr, _source.sin_addr };
    SETSOCKOPT(_recv_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, mreq);

    // Enable multicast loopback iff requested.
    int loop = _loop;
    SETSOCKOPT(_send_sock, IPPROTO_IP, IP_MULTICAST_LOOP, loop);

    // Nonblocking I/O and close-on-exec for the sockets.
    fcntl(_send_sock, F_SETFL, O_NONBLOCK);
    fcntl(_send_sock, F_SETFD, FD_CLOEXEC);
    fcntl(_recv_sock, F_SETFL, O_NONBLOCK);
    fcntl(_recv_sock, F_SETFD, FD_CLOEXEC);

    if (noutputs())
        add_select(_recv_sock, SELECT_READ);

    if (ninputs() && input_is_pull(0)) {
        ScheduleInfo::join_scheduler(this, &_task, errh);
        _signal = Notifier::upstream_empty_signal(this, 0, &_task);
        add_select(_send_sock, SELECT_WRITE);
    }

    return 0;
#undef SETSOCKOPT
}


void
McastSocket::cleanup(CleanupStage)
{
    cleanup();
}


void
McastSocket::cleanup()
{
    if (_recv_sock >= 0) {
        remove_select(_recv_sock, SELECT_READ);
        close(_recv_sock);
        _recv_sock = -1;
    }

    if (_send_sock >= 0) {
        remove_select(_send_sock, SELECT_WRITE);
        close(_send_sock);
        _send_sock = -1;
    }

    if (_rq)
        _rq->kill();
    if (_wq)
        _wq->kill();
}


void
McastSocket::selected(int, int)
{
    if (noutputs()) {
        // read data from socket
        if (!_rq)
            _rq = Packet::make(_headroom, 0, _snaplen, 0);
        if (_rq) {
            struct sockaddr_in from;
            socklen_t from_len = sizeof from;
            int len = recvfrom(_recv_sock, _rq->data(), _rq->length(), MSG_TRUNC,
                (struct sockaddr *)&from, &from_len);
            assert(from_len == sizeof from);

            if (len < 0) {
                if (errno != EAGAIN) {
                    click_chatter("%s: %s", declaration().c_str(), strerror(errno));
                    cleanup();
                }
            } else if (_source.sin_addr.s_addr &&
                _source.sin_addr.s_addr == from.sin_addr.s_addr &&
                _source.sin_port == from.sin_port) {
                // This is our own traffic. Ignore it.
                _rq->kill();
                _rq = NULL;
            } else {
                if (len > _snaplen) {
                    // truncate packet to max length (should never happen)
                    assert(_rq->length() == (uint32_t)_snaplen);
                    SET_EXTRA_LENGTH_ANNO(_rq, len - _snaplen);
                } else {
                    // trim packet to actual length
                    _rq->take(_snaplen - len);
                }

                // set timestamp
                if (_timestamp)
                    _rq->timestamp_anno().assign_now();

                // push packet
                output(0).push(_rq);
                _rq = NULL;
            }
        }
    }

    if (ninputs() && input_is_pull(0))
        run_task(0);
}


int
McastSocket::write_packet(Packet *p)
{
    assert(_send_sock >= 0);

    while (true) {
        int len = sendto(_send_sock, p->data(), p->length(), 0, (struct sockaddr *)&_mcast, sizeof _mcast);
        if (len == (int) p->length()) break;

        // Out of memory or would block, try again later.
        if (errno == ENOBUFS || errno == EAGAIN)
            return -1;

        // Interrupted by signal, try again immediately.
        if (errno == EINTR)
            continue;

        // fatal error
        click_chatter("%s: %s", declaration().c_str(), strerror(errno));
        cleanup();
        break;
    }

    p->kill();
    return 0;
}


void
McastSocket::push(int, Packet *p)
{
    fd_set fds;
    int err;

    if (_send_sock >= 0) {
        // block
        do {
            FD_ZERO(&fds);
            FD_SET(_send_sock, &fds);
            err = select(_send_sock + 1, NULL, &fds, NULL, NULL);
        } while (err < 0 && errno == EINTR);

        if (err >= 0) {
            // write
            do {
                err = write_packet(p);
            } while (err < 0 && (errno == ENOBUFS || errno == EAGAIN));
        }

        if (err < 0) {
            click_chatter("%s: %s, dropping packet", declaration().c_str(), strerror(err));
            p->kill();
        }
    } else
        p->kill();
}


bool
McastSocket::run_task(Task *)
{
    assert(ninputs() && input_is_pull(0));
    bool any = false;

    if (_send_sock >= 0) {
        Packet *p = NULL;
        int err = 0;

        // write as much as we can
        do {
            p = _wq ? _wq : input(0).pull();
            _wq = NULL;
            if (p) {
                any = true;
                err = write_packet(p);
            }
        } while (p && err >= 0);

        if (err < 0) {
            // queue packet for writing when socket becomes available
            _wq = p;
            p = NULL;
            add_select(_send_sock, SELECT_WRITE);
        } else if (_signal)
            // more pending
            // (can't use fast_reschedule() cause selected() calls this)
            _task.reschedule();
        else
            // wrote all we could and no more pending
            remove_select(_send_sock, SELECT_WRITE);
    }

    // true if we wrote at least one packet
    return any;
}


void
McastSocket::add_handlers()
{
    add_task_handlers(&_task);
}


CLICK_ENDDECLS
EXPORT_ELEMENT(McastSocket)

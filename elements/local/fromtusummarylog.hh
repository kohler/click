/* -*- c-basic-offset: 4 -*- */
#ifndef FROMTUSUMMARYLOG_HH
#define FROMTUSUMMARYLOG_HH
#include <click/element.hh>
#include <click/task.hh>

class FromTUSummaryLog : public Element { public:

    FromTUSummaryLog();
    ~FromTUSummaryLog();

    const char *class_name() const	{ return "FromTUSummaryLog"; }
    FromTUSummaryLog *clone() const	{ return new FromTUSummaryLog; }
    const char *processing() const	{ return AGNOSTIC; }

    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();
    void add_handlers();

    Packet *pull(int);
    void run_scheduled();
    
  private:

    int _fd;

    char *_buf;
    int _pos;
    int _len;
    int _cap;
    
    String _filename;
    Task _task;
    bool _active;
    bool _stop;

    bool read_more_buf();
    Packet *try_read_packet();
    Packet *read_packet();
    
};

#endif

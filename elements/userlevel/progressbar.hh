// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_PROGRESSBAR_HH
#define CLICK_PROGRESSBAR_HH
#include <click/element.hh>
#include <click/timer.hh>

/*
=c

ProgressBar(POSHANDLER [, SIZEHANDLER, I<KEYWORDS>])

=s debugging

prints a progress bar to standard error

=io

None

=d

Reads progress information from handlers, and displays an ASCII-art progress
bar on standard error, indicating how much progress has been made and how much
reamins to go.

POSHANDLER and SIZEHANDLER are read handlers. Each of them should return an
unsigned number. POSHANDLER is checked each time the progress bar displays;
SIZEHANDLER is checked just once, the first time the progress bar comes up.
Intuitively, POSHANDLER represents the "position"; the process is complete
when its value equals the "size" returned by SIZEHANDLER.

Keyword arguments are:

=over 8

=item BANNER

String. Print this string before the progress bar. For example, this might be
a section of some filename. Default is empty.

=item UPDATE

Time in seconds (millisecond precision). The progress bar updates itself with
this frequency. Default is 1 second.

=back

Only available in user-level processes.

=e

This ProgressBar shows how far into the file FromDump has gotten:

  fd :: FromDump(~/largedump.gz) -> ...
  ProgressBar(fd.filepos, fd.filesize);

Here are some example progress bars. The first form occurs when the file size
is known; the second, when it is not known.

   74% |**************     | 23315KB    00:01 ETA
  
  |           ***          |  5184KB    --:-- ETA

=n

Code based on the progress bar in the OpenSSH project's B<scp> program. Its
authors are listed as Timo Rinne, Tatu Ylonen, Theo de Raadt, and Aaron
Campbell.

=a

FromDump */

class ProgressBar : public Element { public:

    ProgressBar();
    ~ProgressBar();

    const char *class_name() const		{ return "ProgressBar"; }
    ProgressBar *clone() const			{ return new ProgressBar; }

    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();

    void run_scheduled();

  private:

#if HAVE_INT64_TYPES
    typedef uint64_t thermometer_t;
#else
    typedef uint32_t thermometer_t;
#endif

    enum { ST_FIRST, ST_MIDDLE, ST_DONE };
    
    bool _have_size;
    int _status;
    thermometer_t _size;
    thermometer_t _last_pos;
    struct timeval _start_time;
    struct timeval _stall_time;
    struct timeval _last_time;
    String _banner;

    Timer _timer;
    uint32_t _interval;
    
    Element *_size_element;
    int _size_hi;
    Element *_pos_element;
    int _pos_hi;
    
};

#endif

// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_PROGRESSBAR_HH
#define CLICK_PROGRESSBAR_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS

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
remains to go.

POSHANDLER and SIZEHANDLER are read handlers. Each of them should return an
unsigned number. POSHANDLER is checked each time the progress bar displays;
SIZEHANDLER is checked just once, the first time the progress bar comes up.
Intuitively, POSHANDLER represents the "position"; the process is complete
when its value equals the "size" returned by SIZEHANDLER. You may give
multiple position and/or size handlers, as a space-separated list; their
values are added together.

Keyword arguments are:

=over 8

=item BANNER

String. Print this string before the progress bar. For example, this might be
a section of some filename. Default is empty.

=item UPDATE

Time in seconds (millisecond precision). The progress bar updates itself with
this frequency. Default is 1 second.

=item ACTIVE

Boolean. The progress bar will not initially display itself if this is false.
Default is true.

=item DELAY

Time in seconds (millisecond precision). Don't print a progress bar until at
least DELAY seconds have passed. Use this to avoid trivial progress bars (that
is, progress bars that immediately go to 100%). Default is no delay.

=item CHECK_STDOUT

Boolean. If true, and the standard output is connected to a terminal, then do
not print a progress bar. Default is false.

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

Code based on the progress bar in the OpenSSH project's B<scp> program, whose
authors are listed as Timo Rinne, Tatu Ylonen, Theo de Raadt, and Aaron
Campbell.

=h mark_stopped write-only

When written, the progress bar changes to indicate that the transfer has
stopped, possibly prematurely.

=h mark_done write-only

When written, the progress bar changes to indicate that the transfer has
successfully completed.

=h active read/write

Returns or sets the ACTIVE setting, a Boolean value. An inactive progress bar
will not redraw itself.

=h banner read/write

Returns or sets the BANNER string.

=h poshandler read/write

Returns or sets the read handlers used to read the position, as a
space-separated list.

=h sizehandler read/write

Returns or sets the read handlers used to read the size, as a space-separated
list.

=h reset write-only

When written, resets the progress bar to its initial state: the size is read
again, for example. Also sets ACTIVE to true.

=a

FromDump */

class ProgressBar : public Element { public:

    ProgressBar();
    ~ProgressBar();

    const char *class_name() const		{ return "ProgressBar"; }
    ProgressBar *clone() const			{ return new ProgressBar; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);
    void add_handlers();

    void run_scheduled();

    void complete(bool is_full);

  private:

#if HAVE_INT64_TYPES
    typedef uint64_t thermometer_t;
#else
    typedef uint32_t thermometer_t;
#endif

    enum { ST_FIRST, ST_MIDDLE, ST_DONE, ST_FIRSTDONE, ST_DEAD };
    
    bool _have_size;
    int _status;
    thermometer_t _size;
    thermometer_t _last_pos;
    struct timeval _start_time;
    struct timeval _stall_time;
    struct timeval _last_time;
    struct timeval _delay_time;
    String _banner;

    Timer _timer;
    uint32_t _interval;
    uint32_t _delay_ms;
    bool _active;

    Vector<Element *> _es;
    Vector<int> _his;
    int _first_pos_h;

    bool get_value(int first, int last, thermometer_t *);
    
    static String read_handler(Element *, void *);
    static int write_handler(const String &, Element *, void *, ErrorHandler*);
    
};

CLICK_ENDDECLS
#endif

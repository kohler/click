// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_PROGRESSBAR_HH
#define CLICK_PROGRESSBAR_HH
#include <click/element.hh>
#include <click/timer.hh>
CLICK_DECLS
class Handler;

/*
=c

ProgressBar(POSHANDLER [, SIZEHANDLER, I<KEYWORDS>])

=s control

prints a progress bar to standard error

=d

Reads progress information from handlers, and displays an ASCII-art progress
bar on standard error, indicating how much progress has been made and how much
remains to go.

POSHANDLER and SIZEHANDLER are read handlers. Each of them should return a
nonnegative real number. POSHANDLER is checked each time the progress bar
displays; SIZEHANDLER is checked just once, the first time the progress bar
comes up. Intuitively, POSHANDLER represents the "position"; the process is
complete when its value equals the "size" returned by SIZEHANDLER. You may
give multiple position and/or size handlers, as a space-separated list; their
values are added together.

Keyword arguments are:

=over 8

=item FIXED_SIZE

Nonnegative real number. Used as the size when SIZEHANDLER is not supplied.
Default is no fixed size.

=item BANNER

String. Print this string before the progress bar. For example, this might be
a section of some filename. Default is empty.

=item UPDATE

Time in seconds (millisecond precision). The progress bar updates itself with
this frequency. Default is 1/4 second.

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

=h pos read-only

Returns the progress bar's current position.

=h size read/write

Returns or sets the progress bar's size value, which is used to compute how
close the process is to completion.

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

    ProgressBar() CLICK_COLD;
    ~ProgressBar() CLICK_COLD;

    const char *class_name() const		{ return "ProgressBar"; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    int initialize(ErrorHandler *) CLICK_COLD;
    void cleanup(CleanupStage) CLICK_COLD;
    void add_handlers() CLICK_COLD;

    void run_timer(Timer *);

    void complete(bool is_full);

  private:

    enum { ST_FIRST, ST_MIDDLE, ST_DONE, ST_FIRSTDONE, ST_DEAD };

    bool _have_size;
    int _status;
    double _size;
    double _last_pos;
    Timestamp _start_time;
    Timestamp _stall_time;
    Timestamp _last_time;
    Timestamp _delay_time;
    String _banner;

    Timer _timer;
    uint32_t _interval;
    uint32_t _delay_ms;
    bool _active;

    Vector<Element*> _es;
    Vector<const Handler*> _hs;
    int _first_pos_h;

    bool get_value(int first, int last, double *);

    static String read_handler(Element *, void *) CLICK_COLD;
    static int write_handler(const String &, Element *, void *, ErrorHandler*) CLICK_COLD;

};

CLICK_ENDDECLS
#endif

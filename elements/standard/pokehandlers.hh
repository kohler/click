// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_POKEHANDLERS_HH
#define CLICK_POKEHANDLERS_HH
#include <click/element.hh>
#include <click/timer.hh>

/*
 * =c
 * PokeHandlers([DELAY or HANDLER] ...)
 * =s debugging
 * calls write handlers at specified times
 * =io
 * None
 * =d
 *
 * Runs write handlers automatically at specified times. Each configuration
 * argument should be either `DELAY', a delay in seconds; `ELEMENT.HANDLERNAME
 * VALUE', a directive to write VALUE into ELEMENT's HANDLERNAME write
 * handler; `quit'; or `loop'. PokeHandlers processes its arguments in order,
 * writing to handlers as they appear. A `DELAY' directive causes it to wait
 * for DELAY seconds before continuing. `quit' stops the driver, and `loop'
 * restarts the program from the beginning.
 *
 * At user level, errors reported by write handlers are printed to standard
 * error. In the Linux kernel module, they are printed to /var/log/messages
 * (accessible through dmesg(1)) and to /proc/click/errors.
 * =e
 *   PokeHandlers(red.max_p 0.8,
 *                1.5, // delay for 1.5 seconds
 *                red.max_p 0.5);
 * =a PeekHandlers, dmesg(1) */

class PokeHandlers : public Element { public:

    PokeHandlers();
    ~PokeHandlers();

    const char *class_name() const		{ return "PokeHandlers"; }
    PokeHandlers *clone() const			{ return new PokeHandlers; }
    
    int configure(const Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void uninitialize();

  private:

    static Element * const QUIT_MARKER = (Element *)1;
    static Element * const LOOP_MARKER = (Element *)2;

    int _pos;
    Vector<Element *> _h_element;
    Vector<String> _h_handler;
    Vector<String> _h_value;
    Vector<int> _h_timeout;
    Timer _timer;

    static void timer_hook(Timer *, void *);
    void add(Element *, const String &, const String &, int);

};

#endif

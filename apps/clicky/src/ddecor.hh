#ifndef CLICKY_DDECOR_HH
#define CLICKY_DDECOR_HH 1
#include "dwidget.hh"
#include "permstr.hh"
#include <deque>
namespace clicky {
class dfullness_style;
class dactivity_style;

class ddecor { public:

    ddecor(ddecor *next)
	: _next(next) {
    }
    
    virtual ~ddecor() {
    }

    virtual void draw(delt *e, double *sides, dcontext &dcx);
    virtual void notify(wmain *w, delt *e, handler_value *hv);

    static void draw_list(ddecor *dd, delt *e, double *sides, dcontext &dcx) {
	while (dd) {
	    dd->draw(e, sides, dcx);
	    dd = dd->_next;
	}
    }

    static void notify_list(ddecor *dd, wmain *w, delt *e, handler_value *hv) {
	while (dd) {
	    dd->notify(w, e, hv);
	    dd = dd->_next;
	}
    }
    
    static void free_list(ddecor *&dd) {
	while (dd) {
	    ddecor *n = dd->_next;
	    delete dd;
	    dd = n;
	}
    }
    
  private:
    
    ddecor *_next;
    
};


class dfullness_decor : public ddecor { public:

    dfullness_decor(PermString name, wdiagram *d, delt *e, ddecor *next);

    void draw(delt *e, double *sides, dcontext &dcx);
    void notify(wmain *w, delt *e, handler_value *hv);

  private:

    PermString _name;
    ref_ptr<dfullness_style> _dfs;
    double _capacity;
    double _hvalue;
    double _drawn;
    
};


class dactivity_decor : public ddecor { public:

    dactivity_decor(PermString name, wdiagram *d, delt *e, ddecor *next);
    ~dactivity_decor();

    void draw(delt *e, double *sides, dcontext &dcx);
    void notify(wmain *w, delt *e, handler_value *hv);

    gboolean on_decay();
    
  private:

    PermString _name;
    wmain *_w;
    delt *_e;
    ref_ptr<dactivity_style> _das;

    struct sample {
	double raw;
	double cooked;
	double timestamp;
	sample(double r, double c, double t)
	    : raw(r), cooked(c), timestamp(t) {
	}
    };

    std::deque<sample> _samples;
    double _drawn;
    guint _decay_source;

    double clean_samples(double now, bool want_prev);
    
};

}
#endif

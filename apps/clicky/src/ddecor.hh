#ifndef CLICKY_DDECOR_HH
#define CLICKY_DDECOR_HH 1
#include "dwidget.hh"
#include "permstr.hh"
namespace clicky {
class dfullness_style;

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

    dfullness_decor(PermString name, wmain *w, delt *e, ddecor *next);

    void draw(delt *e, double *sides, dcontext &dcx);
    void notify(wmain *w, delt *e, handler_value *hv);

  private:

    PermString _name;
    ref_ptr<dfullness_style> _dfs;
    double _capacity;
    double _hvalue;
    double _drawn;
    
};

}
#endif

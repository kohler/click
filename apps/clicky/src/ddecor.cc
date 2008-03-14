#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include "ddecor.hh"
#include "dstyle.hh"
#include "diagram.hh"
#include "ddecor.hh"
#include "wrouter.hh"
#include <click/confparse.hh>
#include <math.h>
extern "C" {
#include "support.h"
}
namespace clicky {

void ddecor::draw(delt *, double *, dcontext &)
{
}

void ddecor::notify(wmain *, delt *, handler_value *)
{
}

dfullness_decor::dfullness_decor(PermString name, wmain *w, delt *e,
				 ddecor *next)
    : ddecor(next), _name(name),
      _dfs(w->ccss()->fullness_style(_name, e)),
      _capacity(-1), _hvalue(-1), _drawn(-1)
{
    if (_dfs->length)
	e->handler_interest(w, _dfs->length, _dfs->autorefresh > 0, _dfs->autorefresh_period);
    if (_dfs->capacity && !cp_double(_dfs->capacity, &_capacity))
	e->handler_interest(w, _dfs->capacity, _dfs->autorefresh > 1, _dfs->autorefresh_period);
}


void dfullness_decor::draw(delt *e, double *sides, dcontext &dcx)
{
    if (_hvalue > 0 && sides) {
	int o = e->orientation();
	double xpos = sides[o];
	sides[o] = fma(std::max(std::min(_hvalue, 1.0), 0.0),
		       sides[o] - sides[o ^ 2], sides[o ^ 2]);
	cairo_set_source_rgba(dcx, _dfs->color[0], _dfs->color[1], _dfs->color[2], _dfs->color[3]);
	cairo_move_to(dcx, sides[1], sides[2]);
	cairo_line_to(dcx, sides[3], sides[2]);
	cairo_line_to(dcx, sides[3], sides[0]);
	cairo_line_to(dcx, sides[1], sides[0]);
	cairo_close_path(dcx);
	cairo_fill(dcx);
	_drawn = _hvalue;
	sides[o] = xpos;
    }
}

void dfullness_decor::notify(wmain *w, delt *e, handler_value *hv)
{
    handler_value *lv = 0, *cv = 0;
    if (hv->handler_name() == _dfs->length)
	lv = hv;
    else if (hv->handler_name() == _dfs->capacity)
	cv = hv;
    else
	return;

    if (!lv)
	lv = w->hvalues().find(e->flat_name() + ".length");
    if (!cv && _dfs->capacity && _capacity < 0)
	cv = w->hvalues().find(e->flat_name() + ".capacity");
    
    if (lv && !lv->have_hvalue())
	lv->refresh(w);
    if (cv && !cv->have_hvalue())
	cv->refresh(w);

    double l, c;
    _hvalue = -1;
    if (lv && lv->have_hvalue() && cp_double(lv->hvalue(), &l)) {
	if (!_dfs->capacity)
	    _hvalue = l;
	else if (_capacity >= 0)
	    _hvalue = l / _capacity;
	else if (cv && cv->have_hvalue() && cp_double(cv->hvalue(), &c))
	    _hvalue = l / c;
    }

    if ((_drawn < 0) != (_hvalue < 0)
	|| (fabs(_drawn - _hvalue) * e->side_length(e->orientation())
	    * w->diagram()->scale()) > 0.5)
	w->diagram()->redraw(*e);
}

}

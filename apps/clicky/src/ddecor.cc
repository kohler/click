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
#include <click/timestamp.hh>
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


/*****
 *
 *
 *
 */

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


/*****
 *
 *
 *
 */

#define ALPHA 0.875
#define LOG_ALPHA -.13353139262452262314

dactivity_decor::dactivity_decor(PermString name, wmain *w, delt *e,
				 ddecor *next)
    : ddecor(next), _name(name), _w(w), _e(e),
      _das(w->ccss()->activity_style(_name, e)),
      _drawn(0), _decay_source(0)
{
    if (_das->handler)
	e->handler_interest(w, _das->handler, _das->autorefresh > 0, _das->autorefresh_period, true);
}

dactivity_decor::~dactivity_decor()
{
    if (_decay_source)
	g_source_remove(_decay_source);
}

extern "C" {
static gboolean on_activity_decay(gpointer user_data)
{
    dactivity_decor *da = reinterpret_cast<dactivity_decor *>(user_data);
    return da->on_decay();
}
}

gboolean dactivity_decor::on_decay()
{
    _w->diagram()->redraw(*_e);
    return FALSE;
}

static double square(double d)
{
    return d * d;
}

double dactivity_decor::clean_samples(double now, bool want_prev)
{
    double prev_sample = 0;
    unsigned prev_max = (unsigned) -1;
    double max = 0;
    double rate_ago = now - _das->rate_period;

    for (unsigned i = 0; i < _samples.size(); ++i) {
	sample &s = _samples[i];
	if (s.timestamp <= rate_ago || i == 0)
	    prev_sample = s.raw;
	if (s.timestamp <= rate_ago && _das->decay <= 0)
	    s.cooked = 0;
	if (s.cooked) {
	    double val = s.cooked;
	    if (s.timestamp < rate_ago)
		val -= std::min(1., square((rate_ago - s.timestamp) / _das->decay));
	    if (val == 0)
		s.cooked = 0;
	    else if (val > max) {
		max = val;
		if (prev_max != (unsigned) -1)
		    _samples[prev_max].cooked = 0;
		prev_max = i;
	    }
	}
	if (s.timestamp <= rate_ago || i == 0)
	    prev_sample = s.raw;
    }
    while (_samples.size() > 1
	   && _samples.front().cooked == 0
	   && (_samples[1].timestamp <= rate_ago
	       || _samples[0].raw == _samples[1].raw))
	_samples.pop_front();

    return want_prev ? prev_sample : max;
}

void dactivity_decor::draw(delt *, double *sides, dcontext &dcx)
{
    if (!sides)
	return;

    double now = Timestamp::now().doubleval();
    if (_das->type == dactivity_absolute)
	_drawn = (_samples.size() ? _samples.back().cooked : 0);
    else
	_drawn = clean_samples(now, false);
    if (_drawn * _das->color[3]) {
	cairo_set_source_rgba(dcx, _das->color[0], _das->color[1], _das->color[2], _drawn * _das->color[3]);
	cairo_move_to(dcx, sides[1], sides[2]);
	cairo_line_to(dcx, sides[3], sides[2]);
	cairo_line_to(dcx, sides[3], sides[0]);
	cairo_line_to(dcx, sides[1], sides[0]);
	cairo_close_path(dcx);
	cairo_fill(dcx);
	if (_decay_source)
	    g_source_remove(_decay_source);
	int dto = std::max((int) (80 * _das->decay), 33);
	_decay_source = g_timeout_add(dto, on_activity_decay, this);
    }
}

void dactivity_decor::notify(wmain *w, delt *e, handler_value *hv)
{
    if (hv->handler_name() != _das->handler)
	return;

    double new_value;
    if (hv->have_hvalue() && cp_double(hv->hvalue(), &new_value)) {
	double now = Timestamp::now().doubleval();
	double cooked;
	if (_das->type == dactivity_absolute || _samples.size() == 0) {
	    _samples.clear();
	    cooked = std::min(new_value / _das->max_value, 1.);
	} else {
	    double prev_value = clean_samples(now, true);
	    cooked = std::min(std::max(new_value - prev_value, 0.) / _das->max_value, 1.);
	}
	_samples.push_back(sample(new_value, cooked, now));
	if (128 * fabs(cooked - _drawn) > _das->max_value)
	    w->diagram()->redraw(*e);
    }
}

}

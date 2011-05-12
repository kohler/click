#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <click/config.h>
#include "ddecor.hh"
#include "crouter.hh"
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

void ddecor::notify(crouter *, delt *, handler_value *)
{
}


/*****
 *
 *
 *
 */

dfullness_decor::dfullness_decor(PermString name, crouter *cr, delt *e,
				 ddecor *next)
    : ddecor(next), _name(name),
      _dfs(cr->ccss()->fullness_style(_name, cr, e)),
      _capacity(-1), _hvalue(-1), _drawn(-1)
{
    if (_dfs->length)
	e->handler_interest(cr, _dfs->length, _dfs->autorefresh > 0, _dfs->autorefresh_period);
    if (_dfs->capacity && !cp_double(_dfs->capacity, &_capacity))
	e->handler_interest(cr, _dfs->capacity, _dfs->autorefresh > 1, _dfs->autorefresh_period);
    notify(cr, e, 0);
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

void dfullness_decor::notify(crouter *cr, delt *e, handler_value *hv)
{
    handler_value *lv = 0, *cv = 0;
    if (hv) {
	if (hv->handler_name() == _dfs->length)
	    lv = hv;
	else if (hv->handler_name() == _dfs->capacity)
	    cv = hv;
	else
	    return;
    }

    if (!lv)
	lv = cr->hvalues().find(e->flat_name() + "." + _dfs->length);
    if (!cv && _dfs->capacity && _capacity < 0)
	cv = cr->hvalues().find(e->flat_name() + "." + _dfs->capacity);

    if (lv && !lv->have_hvalue())
	lv->refresh(cr);
    if (cv && !cv->have_hvalue())
	cv->refresh(cr);

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

    if ((_drawn < 0) != (_hvalue < 0))
	cr->repaint(*e);
    else
	cr->repaint_if_visible(*e,
		(_drawn - _hvalue) * e->side_length(e->orientation()));
}


/*****
 *
 *
 *
 */

#define ALPHA 0.875
#define LOG_ALPHA -.13353139262452262314

dactivity_decor::dactivity_decor(PermString name, crouter *cr, delt *e,
				 ddecor *next)
    : ddecor(next), _name(name), _cr(cr), _e(e),
      _das(cr->ccss()->activity_style(_name, cr, e)),
      _drawn_activity(0), _decay_source(0)
{
    if (_das->handler)
	e->handler_interest(cr, _das->handler, _das->autorefresh > 0, _das->autorefresh_period, true);
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
    _cr->repaint(*_e);
    return FALSE;
}

static double square(double d)
{
    return d * d;
}

double dactivity_decor::get_activity(double now)
{
    double oldest = now - _das->decay;
    double youngest = now - _das->autorefresh_period;

    unsigned min_samples = _das->type == dactivity_rate ? 1 : 0;
    while (_samples.size() > min_samples
	   && _samples.front().timestamp <= oldest)
	_samples.pop_front();

    double value = 0;
    double total_weight = 0;
    for (unsigned i = 0; i < _samples.size(); ++i) {
	double this_metric = _samples[i].metric;
	double this_weight;
	if (i == _samples.size() - 1 && _samples[i].timestamp >= youngest)
	    this_weight = 1;
	else if (_samples[i].timestamp <= oldest)
	    continue;
	else
	    this_weight = square((_samples[i].timestamp - oldest) / _das->decay);
	value += this_metric * this_weight;
	total_weight += this_weight;
    }

    value /= total_weight;
    if (value <= _das->min_value)
	return 0;
    else if (value >= _das->max_value)
	return 1;
    else
	return (value - _das->min_value) / (_das->max_value - _das->min_value);
}

void color_interpolate(double *c, const double *c1, double m, const double *c2)
{
    if (c1[3] == 0) {
	memcpy(c, c2, sizeof(double) * 3);
	c[3] = c2[3]*m;
    } else if (c2[3] == 0) {
	memcpy(c, c1, sizeof(double) * 3);
	c[3] = c1[3]*(1-m);
    } else
	for (int i = 0; i < 4; i++)
	    c[i] = c1[i]*(1-m) + c2[i]*m;
}

void dactivity_decor::draw(delt *, double *sides, dcontext &dcx)
{
    if (!sides)
	return;

    double now = Timestamp::now().doubleval();
    double activity = _drawn_activity = get_activity(now);
    if (activity <= 1/128. && !_das->colors[4])
	return;

    int p;
    if (_das->colors.size() == 10)
	p = 0;
    else
	for (p = 0;
	     p < _das->colors.size() - 5 && activity >= _das->colors[p + 5];
	     p += 5)
	    /* nada */;

    double *color, colorbuf[4];
    if (fabs(activity - _das->colors[p]) < 1 / 128.)
	color = &_das->colors[p+1];
    else {
	color = colorbuf;
	double m = (activity - _das->colors[p]) / (_das->colors[p+5] - _das->colors[p]);
	color_interpolate(color, &_das->colors[p+1], m, &_das->colors[p+6]);
    }

    if (color[3]) {
	cairo_set_source_rgba(dcx, color[0], color[1], color[2], color[3]);
	cairo_move_to(dcx, sides[1], sides[2]);
	cairo_line_to(dcx, sides[3], sides[2]);
	cairo_line_to(dcx, sides[3], sides[0]);
	cairo_line_to(dcx, sides[1], sides[0]);
	cairo_close_path(dcx);
	cairo_fill(dcx);
	if (_decay_source)
	    g_source_remove(_decay_source);
	int dto;
	if (_das->decay == 0)
	    dto = _das->autorefresh_period;
	else
	    dto = std::max((int) (80 * _das->decay), 33);
	_decay_source = g_timeout_add(dto, on_activity_decay, this);
    }
}

void dactivity_decor::notify(crouter *cr, delt *e, handler_value *hv)
{
    if (hv->handler_name() != _das->handler)
	return;

    double new_value;
    if (hv->have_hvalue() && cp_double(hv->hvalue(), &new_value)) {
	double now = Timestamp::now().doubleval();
	double new_metric;
	if (_das->type == dactivity_absolute)
	    new_metric = new_value;
	else {			// _das->type == dactivity_rate
	    if (_samples.empty())
		new_metric = 0;
	    else {
		double delta_t = now - _samples.back().timestamp;
		double delta_v = new_value - _samples.back().value;
		if (delta_v < 0) {
		    if (new_value < 20000000.0
			&& delta_v >= -4294967296.0
			&& delta_v <= -4250000000.0)
			delta_v += 4294967296.0;
		}
		new_metric = delta_t ? delta_v / delta_t : 0;
	    }
	}
	_samples.push_back(sample(new_value, new_metric, now));
	if (128 * fabs(get_activity(now) - _drawn_activity) >= 1)
	    cr->repaint(*e);
    }
}

}

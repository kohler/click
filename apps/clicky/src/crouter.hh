#ifndef CLICKY_CROUTER_HH
#define CLICKY_CROUTER_HH 1
#include "gathererror.hh"
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <click/bitvector.hh>
#include <deque>
#include "hvalues.hh"
class ElementT;
class ConnectionT;
class ElementClassT;
class RouterT;
class ElementMap;
class ProcessingT;
class LexerTInfo;
class rectangle;
namespace clicky {
class cdriver;
class csocket_cdriver;
class clickfs_cdriver;
class cdiagram;
class dcss_set;

typedef Vector<Pair<String, String> > messagevector;

class crouter { public:

    crouter(dcss_set *ccss);
    virtual ~crouter();

    String landmark() const {
	return _landmark;
    }

    // router
    bool empty() const;
    RouterT *router() const {
	return _r;
    }
    bool element_exists(const String &ename, bool only_primitive = false) const;
    ElementClassT *element_type(const String &ename) const;

    ElementMap *element_map() const {
	return _emap;
    }
    ProcessingT *processing() const {
	return _processing;
    }

    struct reachable_t {
	Bitvector main;
	HashTable<String, Bitvector> compound;
	bool operator()(const String &context, int eindex) const {
	    if (!context)
		return main.size() && main[eindex];
	    const Bitvector *v = compound.get_pointer(context);
	    return v && v->size() && (*v)[eindex];
	}
    };
    const reachable_t &downstream(const String &str);
    const reachable_t &upstream(const String &str);

    // errors
    GatherErrorHandler *error_handler() const {
	return &_gerrh;
    }
    virtual void on_error(bool replace, const String &dialog) {
	(void) replace, (void) dialog;
    }

    // setting configuration
    virtual void clear(bool alive);

    void set_landmark(const String &landmark);
    virtual void on_landmark_changed() {
    }

    const String &config() const {
	return _conf;
    }
    void set_config(const String &conf, bool replace);
    virtual LexerTInfo *on_config_changed_prepare() {
	return 0;
    }
    virtual void on_config_changed(bool replace, LexerTInfo *linfo) {
	(void) replace, (void) linfo;
    }

    // style
    dcss_set *ccss() const {
	return _ccss;
    }
    void set_ccss_media(const String &media);
    virtual void on_ccss_changed() {
    }

    // throbber
    void throbber(bool show);
    virtual void on_throbber_changed(bool show) {
	(void) show;
    }

    class throb_after { public:
	throb_after(crouter *cr, int timeout);
	~throb_after();
	// actually private:
	crouter *_cr;
	guint _timeout;
    };

    // driver
    cdriver *driver() const {
	return _driver_active ? _driver : 0;
    }
    bool driver_local() const {
	return _driver_active && _driver_process;
    }
    void set_driver(cdriver *driver, bool active);
    void kill_driver();
    void run(ErrorHandler *errh);
    virtual void on_driver_changed() {
    }
    virtual void on_driver_connected();

    void select_driver(int driver) {
	_selected_driver = driver;
    }
    int selected_driver() const {
	return _selected_driver;
    }

    handler_values &hvalues() {
	return _hvalues;
    }
    const handler_values &hvalues() const {
	return _hvalues;
    }

    virtual void on_handler_create(handler_value *hv, bool was_empty) {
	(void) hv, (void) was_empty;
    }
    virtual void on_handler_read(const String &hname, const String &hparam,
				 const String &hvalue,
				 int status, messagevector &messages);
    virtual void on_handler_read(handler_value *hv, bool changed) {
	(void) hv, (void) changed;
    }
    virtual void on_handler_write(const String &hname, const String &hvalue,
				  int status, messagevector &messages);
    virtual void on_handler_check_write(const String &hname,
					int status, messagevector &messages);

    // diagram
    void export_diagram(const char *filename, bool eps, cdiagram *cd);

    virtual void repaint(const rectangle &rect) {
	(void) rect;
    }
    virtual void repaint_if_visible(const rectangle &rect, double dimen) {
	(void) rect, (void) dimen;
    }

  private:

    String _landmark;
    String _conf;
    RouterT *_r;
    ElementMap *_emap;
    int _selected_driver;
    ProcessingT *_processing;
    HashTable<String, reachable_t> _downstreams;
    HashTable<String, reachable_t> _upstreams;

    handler_values _hvalues;
    cdriver *_driver;
    bool _driver_active;
    pid_t _driver_process;

    dcss_set *_ccss;
    bool _router_ccss;

    mutable GatherErrorHandler _gerrh;
    int _throbber_count;

    struct reachable_match_t {
	String _name;
	int _port;
	bool _forward;
	RouterT *_router;
	String _router_name;
	ProcessingT *_processing;
	Bitvector _seed;

	reachable_match_t(const String &name, int port,
			  bool forward, RouterT *router,
			  ProcessingT *processing);
	reachable_match_t(const reachable_match_t &m, ElementT *subelement);
	~reachable_match_t();
	inline bool get_seed(int eindex, int port) const;
	inline void set_seed(const ConnectionT &conn);
	inline void set_seed_connections(ElementT *element, int port);
	bool add_matches(reachable_t &reach, ErrorHandler *debug_errh);
	void export_matches(reachable_t &reach, ErrorHandler *debug_errh);
    };
    void calculate_reachable(const String &str, bool forward, reachable_t &reach);

    void calculate_router_ccss();

};


inline const crouter::reachable_t &crouter::upstream(const String &str)
{
    reachable_t &r = _upstreams[str];
    if (!r.main.size())
	calculate_reachable(str, false, r);
    return r;
}

inline const crouter::reachable_t &crouter::downstream(const String &str)
{
    reachable_t &r = _downstreams[str];
    if (!r.main.size())
	calculate_reachable(str, true, r);
    return r;
}

String g_click_to_utf8(const String &str);
bool cp_host_port(const String &hosts, const String &ports, IPAddress *result_addr, uint16_t *result_port, ErrorHandler *errh);
int do_fd_connected(int fd, ErrorHandler *errh);

}
#endif

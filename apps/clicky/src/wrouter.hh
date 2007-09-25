#ifndef CLICKY_WROUTER_HH
#define CLICKY_WROUTER_HH 1
#include "gathererror.hh"
#include <click/ipaddress.hh>
#include <click/straccum.hh>
#include <deque>
#include <click/hashmap.hh>
class ElementT;
class RouterT;
class ElementMap;
class ProcessingT;
class ClickyDiagram;

class RouterWindow { public:

    class whandler;
    class wdriver;
    class wdriver_csocket;
    class wdriver_kernel;
    
    RouterWindow();
    ~RouterWindow();

    void clear(bool alive);
    void show();

    void set_landmark(const String &landmark);
    void set_save_file(const String &savefile, bool loading);
    void set_config(String conf, bool replace);

    // implementation properties
    GatherErrorHandler *error_handler() const {
	return &_gerrh;
    }
    wdriver *driver() const {
	return _driver_active ? _driver : 0;
    }
    whandler *handlers() const {
	return _handlers;
    }
    ClickyDiagram *diagram() const {
	return _diagram;
    }
    ElementMap *element_map() const {
	return _emap;
    }
    ProcessingT *processing() const {
	return _processing;
    }

    // GTK properties
    GtkWindow *window() const {
	return GTK_WINDOW(_window);
    }
    PangoAttrList *small_attr() const {
	return _small_attr;
    }
    GtkTextTagTable *binary_tag_table() const {
	return _binary_tag_table;
    }
    GtkTextTag *binary_tag() const {
	return _binary_tag;
    }

    // throbber
    void throbber_show();
    void throbber_hide();
    class throb_after { public:
	throb_after(RouterWindow *rw, int timeout);
	~throb_after();
	// actually private:
	RouterWindow *_rw;
	guint _timeout;
    };
    
    // driver
    void set_csocket(GIOChannel *socket, bool ready);

    typedef Vector<Pair<String, String> > messagevector;
    void on_driver(wdriver *driver, bool active);
    void on_read(const String &hname, const String &data, int status, messagevector &messages);
    void on_write(const String &hname, int status, messagevector &messages);
    void on_check_write(const String &hname, int status, messagevector &messages);
    
    void errors_fill(bool initial);

    // not really public
    void on_open_file();
    void on_open_socket();
    void on_open_kernel();
    void on_save_file(bool save_as);
    void on_config_changed();
    void config_set_driver(int driver);
    void config_check(bool install);
    
    const String &element_showing() const {
	return _eview_name;
    }
    void element_show(String ename, int expand, bool incremental);

    void set_diagram_mode(bool diagram);
    
    gboolean error_view_event(GdkEvent *event);
    gboolean error_tag_event(GtkTextTag *tag, GObject *event_object,
			     GdkEvent *event, GtkTextIter *pos);
    void on_error_scroll_timeout();
    
    void element_tree_sort(int state);
    
    gboolean csocket_event(GIOCondition);

    enum { elist_sort_none = 0, elist_sort_name, elist_sort_class };
    enum { cscmd_config, cscmd_list, cscmd_handlers, cscmd_readhandler,
	   cscmd_writehandler,
	   cscmd_quiet,		// commands >= cscmd_quiet don't report errors
	   cscmd_check_hotconfig };
    enum { cscmdflag_background = 1, cscmdflag_clear = 2 };
    
  private:

    // router
    String _landmark;
    String _savefile;
    static String last_savefile;
    String _conf;
    RouterT *_r;
    ElementMap *_emap;
    int _selected_driver;
    ProcessingT *_processing;
    mutable GatherErrorHandler _gerrh;

    int _throbber_count;
    
    // UI features
    GtkWidget *_window;

    PangoFontDescription *_bold_font;
    PangoAttrList *_small_attr;
    
    GtkWidget *_config_view;
    GtkTextBuffer *_config_buffer;
    bool _config_clean;

    GtkWidget *_error_view;
    GtkTextBuffer *_error_buffer;
    GtkTextTag *_error_hover_tag;
    GtkTextTag *_error_highlight_tag;
    int _error_endpos;
    int _error_hover_index;
    GtkTextTag *_config_error_highlight_tag;
    int _error_highlight_index;
    gdouble _error_highlight_x;
    gdouble _error_highlight_y;
    guint _error_scroller; 

    GtkTreeView *_elist_view;
    GtkTreeStore *_elist_store;
    int _elist_sort;
    GtkTextTag *_config_element_highlight_tag;
    ElementT *_element_highlight;

    String _eview_name;

    // configuration
    gulong _config_changed_signal;
    
    GtkTextTagTable *_binary_tag_table;
    mutable GtkTextTag *_binary_tag;
    
    GdkCursor *_normal_cursor;
    GdkCursor *_link_cursor;

    // subsystems
    whandler *_handlers;
    wdriver *_driver;
    bool _driver_active;
    ClickyDiagram *_diagram;
    
    void dialogs_connect();

    static void fill_elements(RouterT *r, const String &compound, int compound_state, Vector<Pair<String, ElementT *> > &v);
    void fill_elements_tree_store(GtkTreeStore *treestore, RouterT *r, GtkTreeIter *parent, const String &compound);
    void etree_fill();

    bool error_view_motion_offsets(int off1, int off2, int index);
    bool error_view_motion_position(gint x, gint y);
    void error_unhighlight();

    void element_unhighlight();

    void config_choose_driver();
    void config_changed_initialize(bool check, bool save);

    friend class whandler;
    friend class ClickyDiagram;
    
};

bool cp_host_port(const String &hosts, const String &ports, IPAddress *result_addr, uint16_t *result_port, ErrorHandler *errh);
int do_fd_connected(int fd, ErrorHandler *errh);

#endif

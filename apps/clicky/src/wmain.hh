#ifndef CLICKY_WMAIN_HH
#define CLICKY_WMAIN_HH 1
#include "crouter.hh"
namespace clicky {
class whandler;
class wdiagram;

class wmain : public crouter { public:
    
    wmain();
    ~wmain();

    void clear(bool alive);
    void show();

    void set_save_file(const String &savefile, bool loading);

    wdiagram *diagram() const {
	return _diagram;
    }
    whandler *handlers() const {
	return _handlers;
    }

    // GTK properties
    GtkWindow *window() const {
	return GTK_WINDOW(_window);
    }
    PangoAttrList *bold_attr() const {
	return _bold_attr;
    }
    PangoAttrList *small_attr() const {
	return _small_attr;
    }
    PangoAttrList *small_bold_attr() const {
	return _small_bold_attr;
    }
    GtkTextTagTable *binary_tag_table() const {
	return _binary_tag_table;
    }
    GtkTextTag *binary_tag() const {
	return _binary_tag;
    }

    // not really public
    void on_landmark_changed();
    void on_ccss_changed();
    LexerTInfo *on_config_changed_prepare();
    void on_config_changed(bool replace, LexerTInfo *linfo);
    void on_throbber_changed(bool show);
    void on_error(bool replace, const String &dialog);
    
    void on_handler_create(handler_value *hv, bool was_empty);
    void on_handler_read(const String &hname, const String &hparam,
			 const String &hvalue,
			 int status, messagevector &messages);
    void on_handler_read(handler_value *hv, bool changed);
    void on_handler_write(const String &hname, const String &hvalue,
			  int status, messagevector &messages);
    void on_handler_check_write(const String &hname,
				int status, messagevector &messages);

    void on_open_file();
    void on_open_socket();
    void on_open_kernel();
    void on_save_file(bool save_as);
    void on_export_diagram();
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

    void repaint(const rectangle &rect);
    void repaint_if_visible(const rectangle &rect, double dimen);
    
    void element_tree_sort(int state);
    
    gboolean csocket_event(GIOCondition);

    enum { elist_sort_none = 0, elist_sort_name, elist_sort_class };
    
  private:

    // router
    String _savefile;
    static String last_savefile;

    // UI features
    GtkWidget *_window;

    PangoAttrList *_small_attr;
    PangoAttrList *_bold_attr;
    PangoAttrList *_small_bold_attr;
    
    GtkWidget *_config_view;
    GtkTextBuffer *_config_buffer;
    bool _config_clean_errors;
    bool _config_clean_elements;

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
    wdiagram *_diagram;
    
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
    friend class wdiagram;
    
};

}
#endif

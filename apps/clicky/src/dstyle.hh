#ifndef CLICKY_DSTYLE_HH
#define CLICKY_DSTYLE_HH 1
#include <gtk/gtk.h>
namespace clicky {

enum { dhlt_hover = 0,
       dhlt_click = 1,
       dhlt_pressed = 2,
       dhlt_rect_click = 3 };

struct dstyle {
    double port_layout_length;
    double port_length[2];
    double port_width[2];
    double port_separation;
    double min_port_distance;
    double port_offset;
    double min_port_offset;
    double agnostic_separation;
    double port_agnostic_separation[2];
    double inside_dx;
    double inside_dy;
    double inside_contents_dy;
    double min_preferred_height;
    double height_increment;
    double element_dx;
    double element_dy;
    double min_dimen;
    double min_queue_width;
    double min_queue_height;
    double queue_line_sep;

    PangoAttrList *name_attrs;
    PangoAttrList *class_attrs;
};

}
#endif

#ifndef GRIDHEADERINFO_HH
#define GRIDHEADERINFO_HH

/*
 * =c
 * GridHeaderInfo
 * =s Grid
 *
 * This element can be used by scripts to find out various sizes and
 * offsets of the grid protocol headers.
 *
 * The point of this element is to provide information that can be
 * used to properly build Grid click configurations -- some of the
 * Classifiers etc. require offset information.  Probably this whole
 * approach is broken and I should just write a GridClassifier... but
 * then what's the point of a generic classifier?  I am confused...
 *
 * =io
 * None
 * =d
 *
 * =h
 * Returns or sets boolean value of whether or not this node is a
 * gateway. */


#include <click/element.hh>
#include "grid.hh"

class GridHeaderInfo : public Element {
  
public:

  GridHeaderInfo();
  ~GridHeaderInfo();

  const char *class_name() const { return "GridHeaderInfo"; }

  GridHeaderInfo *clone() const { return new GridHeaderInfo; } 
  int configure(const Vector<String> &, ErrorHandler *);
  bool can_live_reconfigure() const { return true; }

  grid_location get_current_location(unsigned int *seq_no = 0);

  void add_handlers();
  int read_args(const Vector<String> &conf, ErrorHandler *errh);

  enum {
    grid_hdr_version,             
                                  
    sizeof_grid_location,         
    sizeof_grid_hdr,              
    sizeof_grid_nbr_entry,        
    sizeof_grid_hello,            
    sizeof_grid_nbr_encap,        
    sizeof_grid_loc_query,        
    sizeof_grid_route_probe,      
    sizeof_grid_route_reply,      
                                  
    offsetof_grid_hdr_version,    
    offsetof_grid_hdr_type,       
    offsetof_grid_hdr_ip,         
    offsetof_grid_hdr_tx_ip,      
                                  
    offsetof_grid_nbr_encap_dst_ip,
                                  
    offsetof_grid_loc_query_dst_ip
  };
};

#endif

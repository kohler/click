#ifndef GRIDHEADERINFO_HH
#define GRIDHEADERINFO_HH

/*
 * =c
 * GridHeaderInfo
 * =s Grid
 * Provide information about Grid header version, header sizes, and offsets 
 * for the current router exectuable.
 *
 * =io
 * None
 * =d
 *
 * This element can be used by scripts to find out various sizes and
 * offsets of the Grid protocol headers.
 *
 * The point of this element is to provide information that can be
 * used to properly build Grid click configurations -- some of the
 * classifiers etc. require offset information.  Probably this whole
 * approach is broken and I should just write a GridClassifier... but
 * then what's the point of a generic classifier?  I am confused...
 *
 *
 * =h grid_hdr_version               read-only
 * Return the Grid header version as a hexadecimal number.
 *
 * 
 *
 * =h sizeof_grid_location           read-only
 * Each sizeof_foo handler returns C<sizeof(foo)>.
 * =h sizeof_grid_hdr                read-only
 * =h sizeof_grid_nbr_entry          read-only
 * =h sizeof_grid_hello              read-only
 * =h sizeof_grid_nbr_encap          read-only
 * =h sizeof_grid_loc_query          read-only
 * =h sizeof_grid_route_probe        read-only
 * =h sizeof_grid_route_reply        read-only
 * 
 *
 * =h offsetof_grid_hdr_version      read-only
 * Each offsetof_grid_hdr_foo handler returns C<offsetof(grid_hdr, foo)>
 * =h offsetof_grid_hdr_type         read-only
 * =h offsetof_grid_hdr_ip           read-only
 * =h offsetof_grid_hdr_tx_ip        read-only
 * 
 * =h offsetof_grid_nbr_encap_dst_ip read-only
 * Returns C<offsetof(grid_nbr_encap, dst_ip)>
 *
 * =h offsetof_grid_loc_query_dst_ip read-only
 * Returns C<offsetof(grid_loc_query, dst_ip)>
 */

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

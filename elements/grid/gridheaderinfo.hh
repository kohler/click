#ifndef GRIDHEADERINFO_HH
#define GRIDHEADERINFO_HH
#include <click/element.hh>
#include "grid.hh"
CLICK_DECLS

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
 * The following handler documentation may not be exhaustive, or may
 * be out of date; use the ``handlers'' handler to see what is
 * actually available.
 *
 * =h grid_hdr_version               read-only
 * Return the Grid header version as a hexadecimal number (no preceding 0x)
 *
 * =h grid_ether_proto               read-only
 * Return the Grid ethernet protocol number as four hexadecimal digits (no preceding 0x)
 *
 * =h grid_proto_hello              read-only
 * Each grid_proto_foo returns the the C<GRID_FOO> protocol number as two hexadecimal digits (no preceding 0x)
 * =h grid_proto_lr_hello           read-only
 * =h grid_proto_nbr_encap          read-only
 * =h grid_proto_loc_query          read-only
 * =h grid_proto_loc_reply          read-only
 * =h grid_proto_route_probe        read-only
 * =h grid_proto_route_reply        read-only
 * =h grid_proto_geocast            read-only
 * =h grid_proto_link_probe         read-only
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
 * =h sizeof_grid_link_probe         read-only
 * =h sizeof_grid_link_entry         read-only
 *
 * =h sizeof_linkstat_link_probe     read_only
 * =h sizeof_linkstat_link_entry     read_only
 *
 * =h offsetof_grid_hdr_version      read-only
 * Each offsetof_grid_hdr_foo handler returns C<offsetof(grid_hdr, foo)>
 * =h offsetof_grid_hdr_type         read-only
 * =h offsetof_grid_hdr_ip           read-only
 * =h offsetof_grid_hdr_tx_ip        read-only
 * =h offsetof_grid_hdr_total_len    read-only
 *
 * =h offsetof_grid_nbr_encap_dst_ip read-only
 * Returns C<offsetof(grid_nbr_encap, dst_ip)>
 *
 * =h offsetof_grid_loc_query_dst_ip read-only
 * Returns C<offsetof(grid_loc_query, dst_ip)> */

class GridHeaderInfo : public Element {

public:

  GridHeaderInfo() CLICK_COLD;
  ~GridHeaderInfo() CLICK_COLD;

  const char *class_name() const { return "GridHeaderInfo"; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  bool can_live_reconfigure() const { return true; }

  void add_handlers() CLICK_COLD;
  int read_args(const Vector<String> &conf, ErrorHandler *errh);

  struct info_t {
    char name[100];
    unsigned int val;
    char base; // 'd' or 'h' for decimal or hexadecimal
    int width; // number of digits; -1 means no width specifier
  };
};

CLICK_ENDDECLS
#endif

#ifndef pep_proto_hh
#define pep_proto_hh

//
// Protocol packet format for the Position Estimation Protocol (PEP).
//

#include "grid.hh"

// A PEP packet carries a bunch of pep_fixes. Each describes the
// probable distance to a different node with a known position.
struct pep_fix {
  unsigned long fix_id;         // IP address in network byte order.
  struct grid_location fix_loc; // Location of node id.
  unsigned long fix_d;          // Estimated meters from sender to id.
  // Ought to include a timestamp in case the fix moves. Like DSDV.
};

struct pep_proto {
  unsigned long id;        // The sender of this packet.
  int n_fixes;
#define pep_proto_fixes 5
  struct pep_fix fixes[pep_proto_fixes];
};

#endif

#ifndef pep_proto_hh
#define pep_proto_hh

//
// Protocol packet formats for the Position Estimation Protocol (PEP).
//

#include "grid.hh"
CLICK_DECLS

static const int pep_proto_fixes = 5;  // Max # of fixes in a packet.
static const int pep_max_hops = 10;    // Don't propagate farther than this.

// Intervals, in seconds:

static const int pep_update = 1;       // Broadcast updates this often.
static const int pep_stale = 5;        // Don't send fixes older than this.
static const int pep_purge = 100;      // Delete cache entries older than this.

// A PEP packet carries a bunch of pep_fixes. Each fix describes the
// probable distance to a different node with a known position.

struct pep_fix {
  unsigned fix_id;        // IP address.
  int fix_seq;            // fix's sequence number.
  grid_location fix_loc;  // Location of node fix_id.
  int fix_hops;           // # of hops to fix_id.
};

struct pep_proto {
  unsigned id;            // The sender of this packet.
  int n_fixes;
  struct pep_fix fixes[pep_proto_fixes];
};


// PEP region protocol.  regions are rectangles in lat/lon space, we
// ignore the fact that they may not be rectangles on the earth's flat
// surface.

struct pep_rgn_fix {
  unsigned fix_id;        // IP address.
  int fix_seq;            // fix's sequence number.
  grid_location fix_loc;  // location of region's lower left corner.
  grid_location fix_dim;  // region's height and width, stored as lat/lon respectively
  int fix_hops;           // # of hops to fix_id.
};

struct pep_rgn_proto {
  unsigned id;
  int n_fixes;
  struct pep_rgn_fix fixes[pep_proto_fixes];
};

CLICK_ENDDECLS
#endif

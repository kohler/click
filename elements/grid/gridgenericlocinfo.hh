#ifndef GRIDGENERICLOCINFO_HH
#define GRIDGENERICLOCINFO_HH

#include <click/element.hh>
#include "grid.hh"
CLICK_DECLS

class GridGenericLocInfo : public Element {
public:
  GridGenericLocInfo() { }
  virtual ~GridGenericLocInfo() { }

  // seq_no is incremented when location changes ``enough to make a
  // difference''
  virtual grid_location get_current_location(unsigned int *seq_no = 0) = 0;

  virtual unsigned int   seq_no()   = 0;
  virtual bool           loc_good() = 0;
  virtual unsigned short loc_err()  = 0;


};
CLICK_ENDDECLS
#endif

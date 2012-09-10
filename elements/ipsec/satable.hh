#ifndef CLICK_SATABLE_HH
#define CLICK_SATABLE_HH
#include <click/element.hh>
#include <click/ipaddress.hh>
#include <click/etheraddress.hh>
#include <click/bighashmap.hh>
#include <click/glue.hh>
#include "sadatatuple.hh"

CLICK_DECLS

class SATable : public Element { public:

  SATable() CLICK_COLD;
  ~SATable() CLICK_COLD;

  const char *class_name() const		{ return "SATable"; }
  String print_sa_data();
  int insert(SPI this_spi , SADataTuple SA_data) ;
  int remove(unsigned int spi);
  SADataTuple * lookup(SPI this_spi);

private:
  //Defines a click hashmap object the SA table in our case
  typedef HashMap<SPI,SADataTuple> STable;
  typedef STable::const_iterator SIter;
  STable _table;

};

CLICK_ENDDECLS
#endif

#ifndef SAVEIPFIELDS_HH
#define SAVEIPFIELDS_HH
#include "element.hh"

/*
 * =c
 * SaveIPFields()
 * =s
 * save IP header fields into annotations.
 * V<IP>
 * =d
 * Expects IP packets. Copies the IP header's TOS, TTL,
 * and offset fields into the Click packet annotation.
 *
 * These annotations are used by the IPEncap element.
 *
 * =a IPEncap
 */

class SaveIPFields : public Element {
  
 public:
  
  SaveIPFields();
  
  const char *class_name() const		{ return "SaveIPFields"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  SaveIPFields *clone() const;
  
  Packet *simple_action(Packet *);
  
};

#endif

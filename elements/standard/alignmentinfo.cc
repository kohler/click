// -*- c-basic-offset: 2; related-file-name: "../../include/click/standard/alignmentinfo.hh" -*-
/*
 * alignmentinfo.{cc,hh} -- element stores alignment information
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2010 Meraki, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include <click/standard/alignmentinfo.hh>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/router.hh>
#include <click/error.hh>
CLICK_DECLS

AlignmentInfo::AlignmentInfo()
{
}

int
AlignmentInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  // check for an earlier AlignmentInfo
  if (void *a = router()->attachment("AlignmentInfo"))
    return ((AlignmentInfo *)a)->configure(conf, errh);
  router()->set_attachment("AlignmentInfo", this);

  // this is the first AlignmentInfo; store all information here
  for (int i = 0; i < conf.size(); i++) {
    Vector<String> parts;
    cp_spacevec(conf[i], parts);

    if (parts.size() == 0)
      errh->warning("empty configuration argument %d", i);

    else if (Element *e = cp_element(parts[0], this, 0)) {
      int number = e->eindex();
      if (_elem_offset.size() <= number) {
	_elem_offset.resize(number + 1, -1);
	_elem_icount.resize(number + 1, -1);
      }
      // report an error if different AlignmentInfo is given
      int old_offset = _elem_offset[number];
      int old_icount = _elem_icount[number];
      if (parts.size() % 2 != 1)
	errh->error("expected %<ELEMENTNAME CHUNK OFFSET [CHUNK OFFSET...]%>");
      _elem_offset[number] = _chunks.size();
      _elem_icount[number] = (parts.size() - 1) / 2;
      for (int j = 1; j < parts.size() - 1; j += 2) {
	int32_t c = -1, o = -1;
	if (!IntArg().parse(parts[j], c))
	  errh->error("expected CHUNK");
	if (!IntArg().parse(parts[j+1], o))
	  errh->error("expected OFFSET");
	_chunks.push_back(c);
	_offsets.push_back(o);
      }
      // check for conflicting information on duplicate AlignmentInfo
      if (old_offset >= 0
	  && (old_icount != _elem_icount[number]
	      || memcmp(&_chunks[old_offset], &_chunks[_elem_offset[number]],
			old_icount * sizeof(int)) != 0
	      || memcmp(&_offsets[old_offset], &_offsets[_elem_offset[number]],
			old_icount * sizeof(int)) != 0))
	errh->error("conflicting AlignmentInfo for %<%s%>", parts[0].c_str());

    }
  }

  return 0;
}

bool
AlignmentInfo::query1(const Element *e, int port, int &chunk, int &offset) const
{
  int idx = e->eindex();
  if (idx < 0 || idx >= _elem_offset.size() || _elem_offset[idx] < 0
      || port >= _elem_icount[idx])
    return false;
  else {
    chunk = _chunks[ _elem_offset[idx] + port ];
    offset = _offsets[ _elem_offset[idx] + port ];
    return true;
  }
}

bool
AlignmentInfo::query(const Element *e, int port, int &chunk, int &offset)
{
  if (void *a = e->router()->attachment("AlignmentInfo"))
    return ((AlignmentInfo *)a)->query1(e, port, chunk, offset);
  else
    return false;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(AlignmentInfo)
ELEMENT_HEADER(<click/standard/alignmentinfo.hh>)

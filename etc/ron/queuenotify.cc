/*
 * queuenotify.{cc,hh} -- queue element
 * Alexander Yip
 *
 * Copyright (c) 2002 Massachusetts Institute of Technology
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
#include "queuenotify.hh"
#include <click/confparse.hh>
#include <click/error.hh>

QueueNotify::QueueNotify()
{
}


void *
QueueNotify::cast(const char *n)
{
  if (strcmp(n, "Storage") == 0)
    return (Storage *)this;
  else if (strcmp(n, "Queue") == 0)
    return (Element *)this;
  else if (strcmp(n, "QueueNotify") == 0)
    return (Element *)this;
  else
    return 0;
}


void
QueueNotify::subscribe_notification(NotifiedElement *e) {
  _subscribers.push_back(e);
}

void
QueueNotify::notify_subscribers(int signal) {
  int i;
  for(i=0; i<_subscribers.size(); i++) {
    _subscribers[i]->notify(signal);
  }
}

void
QueueNotify::push(int, Packet *packet)
{
  if (!size()) {
    NotifierQueue::push(0, packet);
    if (size()) notify_subscribers(DATAREADY);
  } else
    NotifierQueue::push(0, packet);
}


Packet *
QueueNotify::pull(int)
{
  Packet *p = deq();
  if (!size())
    notify_subscribers(NODATA);

  return p;
}


EXPORT_ELEMENT(QueueNotify)

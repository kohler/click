#ifndef CLICK_TOSIMDUMP_HH
#define CLICK_TOSIMDUMP_HH
#include <click/timer.hh>
#include <click/element.hh>
#include <click/task.hh>
#include <stdio.h>
CLICK_DECLS

/*****************************************************************************
 *  Copyright 2002, Univerity of Colorado at Boulder.                        *
 *                                                                           *
 *                        All Rights Reserved                                *
 *                                                                           *
 *  Permission to use, copy, modify, and distribute this software and its    *
 *  documentation for any purpose other than its incorporation into a        *
 *  commercial product is hereby granted without fee, provided that the      *
 *  above copyright notice appear in all copies and that both that           *
 *  copyright notice and this permission notice appear in supporting         *
 *  documentation, and that the name of the University not be used in        *
 *  advertising or publicity pertaining to distribution of the software      *
 *  without specific, written prior permission.                              *
 *                                                                           *
 *  UNIVERSITY OF COLORADO DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS      *
 *  SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND        *
 *  FITNESS FOR ANY PARTICULAR PURPOSE.  IN NO EVENT SHALL THE UNIVERSITY    *
 *  OF COLORADO BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL         *
 *  DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA       *
 *  OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER        *
 *  TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR         *
 *  PERFORMANCE OF THIS SOFTWARE.                                            *
 *                                                                           *
 ****************************************************************************/

/*
 * =c
 *
 * ToSimDump(FILENAME [, SNAPLEN, ENCAPTYPE])
 *
 * =s sinks
 *
 * writes packets to a tcpdump(1) file
 *
 * =d
 *
 * Writes incoming packets to FILENAME in `tcpdump -w' format. This file
 * can be read `tcpdump -r', or by FromDump on a later run.
 *
 * Writes at most SNAPLEN bytes of each packet to the file. The default
 * SNAPLEN is 2000. ENCAPTYPE specifies the first header each packet is
 * expected to have. This information is stored in the file header, and must
 * be correct or tcpdump won't be able to read the file correctly. It can be
 * `C<IP>' or `C<ETHER>'; default is C<ETHER>.
 *
 * This element is only available with simclick.
 *
 * =a
 *
 * tcpdump(1) */

class ToSimDump : public Element { public:
  
  ToSimDump();
  ~ToSimDump();
  
  const char *class_name() const		{ return "ToSimDump"; }
  const char *processing() const		{ return AGNOSTIC; }

  ToSimDump *clone() const;
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();

  Packet *simple_action(Packet *);

 private:
  
  String _filename;
  FILE *_fp;
  unsigned _snaplen;
  unsigned _encap_type;
  
  void write_packet(Packet *);
  
};

CLICK_ENDDECLS
#endif

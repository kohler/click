/*
 * archive.{cc,hh} -- deal with `ar' files
 * Eddie Kohler
 *
 * Copyright (c) 1999 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "archive.hh"
#include "error.hh"
#include "confparse.hh"
#include "straccum.hh"
#include "glue.hh"

/* `ar' file format:

   "!<arch>\n"
8    first header block
68   first file data
68+n second header block
     etc.
   
   header block format:
   
 0    char ar_name[16];		// Member file name, sometimes / terminated.
16    char ar_date[12];		// File date, decimal seconds since Epoch. 
28    char ar_uid[6],
34         ar_gid[6];		// User and group IDs, in ASCII decimal. 
40    char ar_mode[8];		// File mode, in ASCII octal. 
48    char ar_size[10];		// File size, in ASCII decimal. 
58    char ar_fmag[2];		// Always contains ARFMAG == "`\n".
*/

static int
read_int(const char *data, int max_len,
	 const char *type, ErrorHandler *errh, int base = 10)
{
  char buf[17];
  char *end;
  
  assert(max_len <= 16);
  memcpy(buf, data, max_len);
  buf[max_len] = 0;
  
  int result = strtol(buf, &end, base);
  if (end == buf)
    result = -1;
  if (*end && !isspace(*end))
    errh->warning("bad %s in archive", type);
  return result;
}

int
separate_ar_string(const String &s, Vector<ArchiveElement> &v,
		   ErrorHandler *errh = 0)
{
  if (!errh)
    errh = ErrorHandler::silent_handler();
  
  if (s.length() <= 8 || memcmp(s.data(), "!<arch>\n", 8) != 0)
    return errh->error("not an archive");
  
  const char *data = s.data();
  int len = s.length();
  int p = 8;

  ArchiveElement longname_ae;

  // loop over sections
  while (p+60 < len) {
    
    // check magic number
    if (data[p+58] != '`' || data[p+59] != '\n')
      return errh->error("bad archive: missing header magic number");
    
    int size;
    
    if (data[p+0] == '/' && data[p+1] == '/' && isspace(data[p+2])) {
      // GNUlike long name section
      if (longname_ae.data)
	errh->error("two long name sections in archive");
      
      size = read_int(data+p+48, 10, "size", errh);
      if (size < 0 || p+60+size > len)
	return errh->error("truncated archive");

      longname_ae.data = s.substring(p+60, size);
      
    } else {

      ArchiveElement ae;
      
      // read name
      int bsd_longname = 0;
      int j;
      if (data[p] == '/' && data[p+1] >= '0' && data[p+1] <= '9') {
	int offset = read_int(data+p+1, 15, "long name", errh);
	if (!longname_ae.data || offset < 0 || offset >= longname_ae.data.length())
	  errh->error("bad long name in archive");
	else {
	  const char *ndata = longname_ae.data.data();
	  int nlen = longname_ae.data.length();
	  for (j = offset; j < nlen && ndata[j] != '/' && !isspace(ndata[j]);
	       j++)
	    /* nada */;
	  ae.name = longname_ae.data.substring(offset, j - offset);
	}
      } else if (data[p+0] == '#' && data[p+1] == '1' && data[p+2] == '/'
		 && data[p+3] >= '0' && data[p+3] <= '9') {
	bsd_longname = read_int(data+p+3, 13, "long name", errh);
      } else {
	for (j = 0; j < 16 && data[p+j] != '/' && !isspace(data[p+j]); j++)
	  /* nada */;
	ae.name = s.substring(p, j);
      }

      // read date, uid, gid, mode, size
      ae.date = read_int(data+p+16, 12, "date", errh);
      ae.uid = read_int(data+p+28, 6, "uid", errh);
      ae.gid = read_int(data+p+34, 6, "gid", errh);
      ae.mode = read_int(data+p+40, 8, "mode", errh, 8);
      size = read_int(data+p+48, 10, "size", errh);
      if (size < 0 || p+60+size > len)
	return errh->error("truncated archive");

      // set data
      if (bsd_longname > 0) {
	if (size < bsd_longname)
	  return errh->error("bad long name in archive");
	ae.name = s.substring(p+60, bsd_longname);
	ae.data = s.substring(p+60+bsd_longname, size - bsd_longname);
      } else
	ae.data = s.substring(p+60, size);

      // append archive element
      v.push_back(ae);
    }
    
    p += 60 + size;
    if (size % 2 == 1)		// objects in archive are even # of bytes
      p++;
  }
  
  if (p != len)
    return errh->error("truncated archive");
  else
    return 0;
}

String
create_ar_string(const Vector<ArchiveElement> &v, ErrorHandler *errh = 0)
{
  if (!errh)
    errh = ErrorHandler::silent_handler();
  
  StringAccum sa;
  int want_size = 8;
  sa << "!<arch>\n";
  for (int i = 0; i < v.size(); i++) {
    const ArchiveElement &ae = v[i];

    // check name
    const char *ndata = ae.name.data();
    int nlen = ae.name.length();
    bool must_longname = false;
    for (int i = 0; i < nlen; i++)
      if (ndata[i] == '/') {
	errh->error("archive element name `%s' contains slash",
		    String(ae.name).cc());
	nlen = i;
	break;
      } else if (isspace(ndata[i]))
	must_longname = true;

    // write name, or nameish thing
    if ((nlen >= 3 && ndata[0] == '#' && ndata[1] == '1' && ndata[2] == '/')
	|| must_longname || nlen > 14) {
      if (char *x = sa.extend(16))
	sprintf(x, "#1/%-13u", (unsigned)nlen);
      must_longname = true;
    } else
      if (char *x = sa.extend(16))
	sprintf(x, "%-16.*s", nlen, ndata);

    // write other data
    int wrote_size = ae.data.length() + (must_longname ? nlen : 0);
    if (char *x = sa.extend(12 + 6 + 6 + 8 + 10 + 2))
      sprintf(x, "%-12u%-6u%-6u%-8o%-10u`\n", (unsigned)ae.date,
	      (unsigned)ae.uid, (unsigned)ae.gid, (unsigned)ae.mode,
	      (unsigned)wrote_size);

    if (must_longname)
      sa.push(ndata, nlen);
    sa << ae.data;

    want_size += 60 + wrote_size;
    if (wrote_size % 2 == 1) {
      // extend by an extra newline to keep object lengths even
      sa << '\n';
      want_size++;
    }
  }

  // check for memory errors
  if (want_size != sa.length()) {
    errh->error("out of memory");
    return String();
  } else
    return sa.take_string();
}

// generate Vector template instance
#include "vector.cc"
template class Vector<ArchiveElement>;

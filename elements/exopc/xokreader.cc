
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "xokreader.hh"
#include "glue.hh"
#include "error.hh"
#include "confparse.hh"
#include "bitvector.hh"
#include "straccum.hh"
#include <unistd.h>
#include <fcntl.h>


/* exokernel include stuff */
extern "C" {
#include <vos/cap.h>
#include <vos/net/fast_eth.h>
#include <dpf/dpf.h>
#include <dpf/dpf-internal.h>
extern int xok_sys_self_dpf_ref(unsigned int, unsigned int);
extern int xok_sys_self_dpf_delete(unsigned int, unsigned int);
extern int xok_sys_self_dpf_insert(unsigned int, unsigned int, void*, int);
extern int vos_prd_ring_id(int);
}

#define dprintf if (0) printf


xokReader::xokReader()
  : Element(0, 0), fd(-1)
{
  for(int i=0; i<MAX_DPF_FILTERS; i++)
    dpf_ids[i] = -1;
}


xokReader::xokReader(const xokReader &f)
  : Element(0, f.noutputs()), 
    fd(f.fd)
{
  for(int i=0; i<MAX_DPF_FILTERS; i++)
  {
    dpf_ids[i] = f.dpf_ids[i];
    if (dpf_ids[i] != -1) 
    {
      if (xok_sys_self_dpf_ref (CAP_USER, dpf_ids[i]) < 0)
        fprintf(stderr,"xokReader: could not reference filter %d\n",dpf_ids[i]);
    }
  }
}


xokReader::~xokReader()
{
  dprintf("in ~xokReader()\n");

  for(int i=0; i<MAX_DPF_FILTERS; i++)
  {
    if (dpf_ids[i] != -1)
    {
      if (xok_sys_self_dpf_delete (CAP_USER, dpf_ids[i]) < 0) 
        fprintf(stderr,"xokReader: could not remove filter %d\n",dpf_ids[i]);
    }
  }

  if (fd != -1) 
    close(fd);
}


xokReader *
xokReader::clone() const
{
  return new xokReader(*this);
}



//
// CONFIGURATION
//

int
xokReader::configure(const Vector<String> &conf, ErrorHandler *errh)
{
  Vector<String> args;
  cp_argvec(conf, args);

  if (args.size() < 2) 
  {
    fprintf(stderr,"xokReader: got %d arguments\n", args.size());
    errh->error("expecting at least 2 arguments: ring size and a classifier");
    return -1;
  }

  else if (args.size() > MAX_DPF_FILTERS+1)
  {
    fprintf(stderr,"xokReader: got %d arguments\n", args.size());
    errh->error
      ("expecting at most %d arguments: ring size and %d classifiers", 
       MAX_DPF_FILTERS+1, MAX_DPF_FILTERS);
    return -1;
  }

  add_output();
 
  const char *ringsz_s = args[0].data();
	          
  int ring_sz = atoi(ringsz_s);
  if (ring_sz < 1) {
    errh->error("given ring size too small");
    return -1;
  }

  struct dpf_ir dpf_filters[MAX_DPF_FILTERS];

  for(int argc=1; argc < args.size(); argc++)
  {
    const char *classifier_s = args[argc].data();
    int classifier_s_len = args[argc].length();

    dpf_begin (&dpf_filters[argc-1]);

    int i=0;
    while (i < classifier_s_len) {
      while (i < classifier_s_len && isspace(classifier_s[i]))
	i++;
      if (i >= classifier_s_len) break;
      if (!isdigit(classifier_s[i])) {
	errh->error("expected a digit");
	return -1;
      }
      
      int offset = 0;
      while (i < classifier_s_len && isdigit(classifier_s[i])) {
	offset *= 10;
	offset += classifier_s[i] - '0';
	i++;
      }
      
      if (i >= classifier_s_len || classifier_s[i] != '/') {
	errh->error("expected `/'");
	return -1;
      }
      i++;

      int mask = 0, value = 0, iter = 0;
      
      for (; i < classifier_s_len; i++) {
	int d = 0;
	int m = (classifier_s[i] == '?' ? 0 : 15);
	
	if (classifier_s[i] >= '0' && classifier_s[i] <= '9')
	  d = classifier_s[i] - '0';
	else if (classifier_s[i] >= 'a' && classifier_s[i] <= 'f')
	  d = classifier_s[i] - 'a' + 10;
	else if (classifier_s[i] >= 'A' && classifier_s[i] <= 'F')
	  d = classifier_s[i] - 'A' + 10;
	else if (classifier_s[i] != '?')
	  break;

        mask = mask + (m << (3-iter)*4);
	value = value + (d << (3-iter)*4);
	iter++;
	  
	if (iter == 4) 
	{
	  dpf_meq16(&dpf_filters[argc-1], offset, mask, htons(value));
	  mask = value = iter = 0;
	  offset += 2;
	}
      }
      
      if (iter == 2)
      {
	mask = mask >> 8;
	value = value >> 8;
	dpf_meq8(&dpf_filters[argc-1], offset, mask, value);
	mask = value = iter = 0;
	offset += 1;
      }
      
      else if (iter != 0)
	errh->warning("at offset %d: odd number of hex digits", offset);
    } // while i 
  } // for argc

  /* now we have both pkt ring and filters parsed */

  char pkrname[32];
  sprintf(pkrname,"/dev/pktring%d", ring_sz);
  fd = open(pkrname,0,0);

  if (fd < 0) {
    errh->error("cannot set packet ring");
    return -1;
  }

  int dpf_filter_inserted = 0;
  for (int i=0; i<args.size()-1; i++)
  {
    dpf_ids[i] = xok_sys_self_dpf_insert
      (CAP_USER,CAP_USER, &dpf_filters[i], vos_prd_ring_id(fd));
    dprintf("xokReader: insert filter number %d -> %d\n", i, dpf_ids[i]);

    if (dpf_ids[i] < 0) {
      errh->warning("cannot insert dpf filter %s", args[i+1].data());
    } 
    else dpf_filter_inserted++;
  }

  if (dpf_filter_inserted == 0)
  {
    errh->error("cannot insert any dpf filters");
    close(fd);
    return -1;
  }

  dprintf("xokReader: ring descriptor %d (%d buffers)\n", fd, ring_sz);
  add_select(fd, SELECT_READ);
  return 0;
}


void
xokReader::selected(int fd)
{
  char data[ETHER_MAX_LEN];

  int r = read(fd, data, ETHER_MAX_LEN); 
  if (r != ETHER_MAX_LEN)
    fprintf(stderr,"xokReader: corrupted packet on descriptor %d\n", fd); 
  else
  {
    Packet *p = Packet::make(data, ETHER_MAX_LEN);
    output(0).push(p);
  }
}

EXPORT_ELEMENT(xokReader)

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include "ipgwoptions.hh"
#include "click_ip.h"
#include "confparse.hh"
#include "error.hh"
#include "glue.hh"

IPGWOptions::IPGWOptions()
{
  _drops = 0;
  add_input();
  add_output();
}

IPGWOptions::~IPGWOptions()
{
}

int
IPGWOptions::configure(const String &conf, Router *router, ErrorHandler *errh)
{
  IPAddress a;

  if (cp_va_parse(conf, this, router, errh,
                  cpIPAddress, "local addr", &a,
		  0) < 0)
    return -1;
  _my_ip = a.in_addr();
  return 0;
}

void
IPGWOptions::notify_outputs(int n)
{
  // allow 2 outputs -- then packet is pushed onto 2d output instead of
  // dropped
  n = (n >= 2 ? 2 : 1);
  add_outputs(n - noutputs());
}

void
IPGWOptions::processing_vector(Vector<int> &in_v, int in_offset,
			      Vector<int> &out_v, int out_offset) const
{
  in_v[in_offset+0] = out_v[out_offset+0] = AGNOSTIC;
  if (noutputs() == 2)
    out_v[out_offset+1] = PUSH;
}

IPGWOptions *
IPGWOptions::clone() const
{
  return new IPGWOptions();
}

Packet *
IPGWOptions::handle_options(Packet *p)
{
  /* This is lame: should be lazier. */
  p = p->uniqueify();
  struct ip *ip = (struct ip *)p->data();
  unsigned hlen = ip->ip_hl << 2;

  u_char *oa = (u_char *) (ip + 1);
  int olen = hlen - sizeof(struct ip);
  int oi;
  int do_cksum = 0;
  int problem_offset = -1;

  for(oi = 0; oi < olen; ){
    u_int type = oa[oi];
    int xlen;
    if(type <= 1)
      xlen = 1;
    else
      xlen = oa[oi+1];
    if(oi + xlen > olen)
      break;
    if(type == IPOPT_EOL){
      /* end of option list */
      break;
    } else if(type == IPOPT_RR){
      /*
       * Record Route.
       * Apparently the pointer (oa[oi+2]) is 1-origin.
       */
      int p = oa[oi+2] - 1;
      if(p >= 3 && p+4 <= xlen){
        memcpy(oa + oi + p, &_my_ip, 4);
        oa[oi+2] += 4;
        do_cksum = 1;
      } else if(p != xlen){
        problem_offset = 20 + oi + 2;
        goto send_error;
      }
    } else if(type == IPOPT_TS){
      /*
       * Timestamp Option.
       * We can't do a good job with the pre-specified mode (flg=3),
       * since we don't know all our i/f addresses.
       */
      int p = oa[oi+2] - 1;
      int oflw = oa[oi+3] >> 4;
      int flg = oa[oi+3] & 0xf;
      int overflowed = 0;

      struct timeval tv;
      click_gettimeofday(&tv);
      int ms = htonl((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);

      if(p < 4){
        problem_offset = 20 + oi + 2;
        goto send_error;
      } else if(flg == 0){
        /* 32-bit timestamps only */
        if(p+4 <= xlen){
          memcpy(oa + oi + p, &ms, 4);
          oa[oi+2] += 4;
          do_cksum = 1;
        } else {
          overflowed = 1;
        }
      } else if(flg == 1){
        /* ip address followed by timestamp */
        if(p+8 <= xlen){
          memcpy(oa + oi + p, &_my_ip, 4);
          memcpy(oa + oi + p + 4, &ms, 4);
          oa[oi+2] += 8;
          do_cksum = 1;
        } else {
          overflowed = 1;
        }
      } else if(flg == 3){
        /* only if it's my address */
        if(p+8 <= xlen && memcmp(oa + oi + p, &_my_ip, 4) == 0){
          memcpy(oa + oi + p + 4, &ms, 4);
          oa[oi+2] += 8;
          do_cksum = 1;
        }
      }
      if(overflowed){
        if(oflw < 15){
          oa[oi+3] = ((oflw + 1) << 4) | flg;
          do_cksum = 1;
        } else {
          problem_offset = 20 + oi + 3;
          goto send_error;
        }
      }
    }
    oi += xlen;
  }

  if(do_cksum){
    ip->ip_sum = 0;
    ip->ip_sum = in_cksum(p->data(), hlen);
  }

  return(p);

 send_error:
  _drops++;
  if (noutputs() == 2){
    p->set_param_off_anno(problem_offset);
    output(1).push(p);
  } else {
    p->kill();
  }
  return 0;
}

static String
IPGWOptions_read_drops(Element *xf, void *)
{
  IPGWOptions *f = (IPGWOptions *)xf;
  return String(f->drops()) + "\n";
}

void
IPGWOptions::add_handlers(HandlerRegistry *fcr)
{
  Element::add_handlers(fcr);
  fcr->add_read("drops", IPGWOptions_read_drops, 0);
}

void
IPGWOptions::push(int, Packet *p)
{
  assert(p->length() >= sizeof(struct ip));
  
  struct ip *ip = (struct ip *)p->data();
  unsigned hlen = ip->ip_hl << 2;
  if (hlen <= sizeof(struct ip)
      || (p = handle_options(p)))
    output(0).push(p);
}

Packet *
IPGWOptions::pull(int)
{
  Packet *p = input(0).pull();
  if (p) {
    struct ip *ip = (struct ip *)p->data();
    unsigned hlen = ip->ip_hl << 2;
    if (hlen > sizeof(struct ip))
      p = handle_options(p);
  }
  return p;
}

EXPORT_ELEMENT(IPGWOptions)

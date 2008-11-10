#ifndef CLICK_PATH_HH
#define CLICK_PATH_HH
#include <click/straccum.hh>
#include <click/hashcode.hh>
CLICK_DECLS

typedef Vector<IPAddress> Path;

template <>
inline hashcode_t hashcode(const Path &p)
{
    hashcode_t h = 0;
    for (int x = 0; x < p.size(); x++)
	h ^= CLICK_NAME(hashcode)(p[x]);
    return h;
}

inline bool
operator==(const Path &p1, const Path &p2)
{
  if (p1.size() != p2.size()) {
    return false;
  }
  for (int x = 0; x < p1.size(); x++) {
    if (p1[x] != p2[x]) {
      return false;
    }
  }
  return true;
}

inline bool
operator!=(const Path &p1, const Path &p2)
{
  return (!(p1 == p2));
}

inline String path_to_string(const Path &p)
{
  StringAccum sa;
  for(int x = 0; x < p.size(); x++) {
    sa << p[x].unparse();
    if (x != p.size() - 1) {
      sa << " ";
    }
  }
  return sa.take_string();
}


inline Path reverse_path (const Path &p)
{
  Path rev;
  for (int x = p.size() - 1; x >= 0; x--) {
    rev.push_back(p[x]);
  }
  return rev;
}


inline int index_of(Path p, IPAddress ip) {
  for (int x = 0;  x < p.size(); x++) {
    if (p[x] == ip) {
      return x;
    }
  }

  return -1;
}

CLICK_ENDDECLS
#endif /* CLICK_PATH_HH */

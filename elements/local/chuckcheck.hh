#ifndef CLICK_CHUCKCHECK_HH
#define CLICK_CHUCKCHECK_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

ChuckCheck()

=s debugging

Collects information about packets

=d

ChuckCheck collects information about the timestamps and source addresses on
passing IP packets, and makes that information available via the `info'
handler. Packets must have their IP header annotations set.

=n

Warning: This element does not perform the locking required for multiprocessor
machines!!

=h info read-only

Returns a binary string represented the information collected on the last 4096
packets received. (If less than 4096 packets have been received, returns as
much information as it has.)

The string consists of a number of 4-byte unsigned integers in host byte
order. The first integer is N, the number of records in the rest of the
string. After this comes N information records. Each record looks like this:

    0              4              8              12
  +--------------+--------------+--------------+--------------+
  |  packet ID   |timestamp sec |timestamp usec| source addr  |
  +--------------+--------------+--------------+--------------+

The fields are:

=over 5

=item C<packet ID>

The identification number of this packet. The first packet ChuckCheck receives
gets ID number 0, and packets are sequentially numbered after that. The reader
can use packet ID numbers to differentiate new information from information it
has already processed.

=item C<timestamp sec>, C<timestamp usec>

The time when the packet arrived at the ChuckCheck element.

=item C<source addr>

The packet's IP source address.

=back

The records are returned in increasing, sequential order by packet ID.

*/

class ChuckCheck : public Element {

  struct Stat {
    Timestamp time;
    unsigned saddr;
  };

  enum { BUCKETS = 4096 };

  Stat _info[BUCKETS];
  unsigned _head;
  unsigned _tail;
  unsigned _head_id;

  static String read_handler(Element *, void *) CLICK_COLD;

  inline void count(Packet *);

 public:

  ChuckCheck() CLICK_COLD;
  ~ChuckCheck() CLICK_COLD;

  const char *class_name() const		{ return "ChuckCheck"; }
  const char *port_count() const		{ return PORTS_1_1; }
  void add_handlers() CLICK_COLD;

  int initialize(ErrorHandler *) CLICK_COLD;

  void push(int, Packet *);
  Packet *pull(int);

};

CLICK_ENDDECLS
#endif

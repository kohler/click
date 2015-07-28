// -*- c-basic-offset: 4 -*-
#ifndef CLICK_STORAGE_HH
#define CLICK_STORAGE_HH
#include <click/machine.hh>
#include <click/atomic.hh>
CLICK_DECLS
class Packet;

class Storage { public:

    typedef uint32_t index_type;
    typedef int32_t signed_index_type;
    static const index_type invalid_index = (index_type) -1;

    Storage()				: _head(0), _tail(0) { }

    operator bool() const		{ return _head != _tail; }
    bool empty() const			{ return _head == _tail; }
    int size() const;
    int size(index_type head, index_type tail) const;
    int capacity() const		{ return _capacity; }

    index_type head() const		{ return _head; }
    index_type tail() const		{ return _tail; }

    index_type next_i(index_type i) const {
	return (i!=_capacity ? i+1 : 0);
    }
    index_type prev_i(index_type i) const {
	return (i!=0 ? i-1 : _capacity);
    }

    // to be used with care
    void set_capacity(index_type c)	{ _capacity = c; }
    inline void set_head(index_type h); // acquire barrier (read pkt)
    inline void set_tail(index_type t); // release barrier (write pkt)
    inline void set_head_release(index_type h); // release barrier (LIFO)
    inline void set_tail_acquire(index_type t); // acquire barrier (LIFO)
    inline index_type reserve_tail_atomic();

    static inline void packet_memory_barrier(Packet* volatile& packet,
                                             volatile index_type& index)
        __attribute__((deprecated));
    inline void packet_memory_barrier(Packet* volatile& packet)
        __attribute__((deprecated));

  protected:
    index_type _capacity;

  private:
    volatile index_type _head;
    volatile index_type _tail;

};

inline int
Storage::size(index_type head, index_type tail) const
{
    index_type x = tail - head;
    return (signed_index_type(x) >= 0 ? x : _capacity + x + 1);
}

inline int
Storage::size() const
{
    return size(_head, _tail);
}

inline void
Storage::set_head(index_type h)
{
    click_read_fence();
    _head = h;
}

inline void
Storage::set_tail(index_type t)
{
    click_write_fence();
    _tail = t;
}

inline void
Storage::set_head_release(index_type h)
{
    click_write_fence();
    _head = h;
}

inline void
Storage::set_tail_acquire(index_type t)
{
    click_read_fence();
    _tail = t;
}

inline Storage::index_type
Storage::reserve_tail_atomic()
{
    index_type t, nt;
    do {
        t = _tail;
        nt = next_i(t);
        if (nt == _head)
            return invalid_index;
    } while (atomic_uint32_t::compare_swap(_tail, t, nt) != t);
    return t;
}

inline void
Storage::packet_memory_barrier(Packet* volatile& packet, volatile index_type& index)
{
    __asm__ volatile("" : : "m" (packet), "m" (index));
}

inline void
Storage::packet_memory_barrier(Packet * volatile &packet)
{
    __asm__ volatile("" : : "m" (packet), "m" (_head), "m" (_tail));
}

CLICK_ENDDECLS
#endif

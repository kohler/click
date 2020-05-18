// -*- c-basic-offset: 4; related-file-name: "../../lib/netmapdevice.cc" -*-
#ifndef CLICK_NETMAPDEVICE_HH
#define CLICK_NETMAPDEVICE_HH

#if HAVE_NETMAP && CLICK_USERLEVEL

#include <net/if.h>
#include <net/netmap.h>
#define NETMAP_WITH_LIBS
#include <net/netmap_user.h>

// XXX bug in netmap_user.h , the prototype should be available

#ifndef NETMAP_WITH_LIBS
typedef void (*nm_cb_t)(u_char *, const struct nm_pkthdr *, const u_char *d);
#endif

#include <click/error.hh>
#include <click/vector.hh>
#include <click/hashmap.hh>
#include <click/element.hh>
#include <click/args.hh>
#include <click/packet.hh>
#include <click/sync.hh>

CLICK_DECLS

#define NETMAP_PACKET_POOL_SIZE            2048
#define BUFFER_PTR(idx) reinterpret_cast<uint32_t *>(buf_start + idx * buf_size)
#define BUFFER_NEXT_LIST(idx) *(((uint32_t*)BUFFER_PTR(idx)) + 1)

/* a queue of netmap buffers, by index*/
class NetmapBufQ {
public:

    NetmapBufQ();
    ~NetmapBufQ();

    inline void expand();

    inline void insert(uint32_t idx);
    inline void insert_p(unsigned char *p);
    inline void insert_all(uint32_t idx, bool check_size);

    inline uint32_t extract();
    inline unsigned char * extract_p();

    inline int count_buffers(uint32_t idx);

    inline int count() const {
        return _count;
    };

    //Static functions
    static int static_initialize(struct nm_desc* nmd);
    static uint32_t static_cleanup();

    static void global_insert_all(uint32_t idx, int count);

    inline static unsigned int buffer_size() {
        return buf_size;
    }

    static void buffer_destructor(unsigned char *buf, size_t, void *) {
        NetmapBufQ::local_pool()->insert_p(buf);
    }

    inline static bool is_netmap_packet(Packet* p) {
        return (p->buffer_destructor() == buffer_destructor);
    }

    inline static bool is_valid_netmap_packet(Packet* p) {
            return ((p->buffer_destructor() == buffer_destructor) &&
                     p->buffer()>=buf_start && p->buffer() < buf_end);
    }

    inline static NetmapBufQ* local_pool() {
        return NetmapBufQ::netmap_buf_pools[click_current_cpu_id()];
    }

private :
    uint32_t _head;  /* index of first buffer */
    int _count; /* how many ? */

    //Static attributes (shared between all queues)
    static unsigned char *buf_start;   /* base address */
    static unsigned char *buf_end; /* error checking */
    static unsigned int buf_size;
    static uint32_t max_index; /* error checking */

    static Spinlock global_buffer_lock;
    //The global netmap buffer list is used to exchange batch of buffers between threads
    //The second uint32_t in the buffer is used to point to the next batch
    static uint32_t global_buffer_list;

    static int messagelimit;
    static NetmapBufQ** netmap_buf_pools;

}  __attribute__((aligned(64)));

/**
 * A Netmap interface
 */
class NetmapDevice {
public:
    int open(const String &ifname,
         bool always_error, ErrorHandler *errh);
    void initialize_rings_rx(int timestamp);
    void initialize_rings_tx();

    int receive(int cnt, int headroom, void (*)(WritablePacket *, int, const Timestamp &, void* arg),void* arg);
    int send_packet(Packet* p,bool allow_zc);


    void close(int fd);

    struct nm_desc *desc;

    static void static_cleanup();
    static int global_alloc;
    static struct nm_desc* some_nmd;
};

/*
 * Inline functions
 */

inline void NetmapBufQ::expand() {
    global_buffer_lock.acquire();
    if (global_buffer_list != 0) {
        //Transfer from global pool
        _head = global_buffer_list;
        global_buffer_list = BUFFER_NEXT_LIST(global_buffer_list);
        _count = NETMAP_PACKET_POOL_SIZE;
    } else {
        if (messagelimit < 5)
            click_chatter("No more netmap buffers !");
        messagelimit++;
    }
    global_buffer_lock.release();
}

/**
 * Insert a list of netmap buffers in the queue
 */
inline void NetmapBufQ::insert_all(uint32_t idx,bool check_size = false) {
    if (unlikely(idx >= max_index || idx == 0)) {
        click_chatter("Error : cannot insert index %d",idx);
        return;
    }

    uint32_t firstidx = idx;
    uint32_t *p;
    while (idx > 0) { //Go to the end of the passed list
        if (check_size) {
            insert(idx);
        } else {
            p = reinterpret_cast<uint32_t*>(buf_start +
                    idx * buf_size);
            idx = *p;
            _count++;
        }
    }

    //Add the current list at the end of this one
    *p = _head;
    _head = firstidx;
}

/**
 * Return the number of buffer inside a netmap buffer list
 */
int NetmapBufQ::count_buffers(uint32_t idx) {
    int count=0;
    while (idx != 0) {
        count++;
        idx = *BUFFER_PTR(idx);
    }
    return count;
}

inline void NetmapBufQ::insert(uint32_t idx) {
    assert(idx > 0 && idx < max_index);

    if (_count < NETMAP_PACKET_POOL_SIZE) {
        *BUFFER_PTR(idx) = _head;
        _head = idx;
        _count++;
    } else {
        assert(_count == NETMAP_PACKET_POOL_SIZE);
        global_buffer_lock.acquire();
        BUFFER_NEXT_LIST(_head) = global_buffer_list;
        global_buffer_list = _head;
        global_buffer_lock.release();
        _head = idx;
        *BUFFER_PTR(idx) = 0;
        _count = 1;
    }
}

inline void NetmapBufQ::insert_p(unsigned char* buf) {
    insert((buf - buf_start) / buf_size);
}

inline uint32_t NetmapBufQ::extract() {
    if (_count <= 0) {
        expand();
        if (_count == 0) return 0;
    }
    uint32_t idx;
    uint32_t *p;
    idx = _head;
    p  = reinterpret_cast<uint32_t *>(buf_start + idx * buf_size);

    _head = *p;
    _count--;
    return idx;
}

inline unsigned char* NetmapBufQ::extract_p() {
    uint32_t idx = extract();
    return (idx == 0) ? 0 : buf_start + idx * buf_size;
}

#endif

#endif

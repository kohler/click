// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_IPRW_MAPPING_HH
#define CLICK_IPRW_MAPPING_HH
#include <click/element.hh>
#include <click/timer.hh>
#include <click/hashtable.hh>
#include <click/ipflowid.hh>
#include <clicknet/ip.h>
#include "iprwpattern.hh"
CLICK_DECLS
class IPRewriterBase;
class IPRewriterFlow;
class IPRewriterHeap;

class IPRewriterEntry { public:

    typedef IPFlowID key_type;
    typedef const IPFlowID &key_const_reference;

    IPRewriterEntry() {
    }

    void initialize(const IPFlowID &flowid, uint32_t output, bool direction) {
	assert(output <= 0xFFFFFF);
	_flowid = flowid;
	_output = output;
	_direction = direction;
	_hashnext = 0;
    }

    const IPFlowID &flowid() const {
	return _flowid;
    }
    inline IPFlowID rewritten_flowid() const;

    bool direction() const {
	return _direction;
    }

    int output() const {
	return _output;
    }

    IPRewriterFlow *flow() {
	return reinterpret_cast<IPRewriterFlow *>(this - _direction);
    }
    const IPRewriterFlow *flow() const {
	return reinterpret_cast<const IPRewriterFlow *>(this - _direction);
    }

    key_const_reference hashkey() const {
	return _flowid;
    }

  private:

    IPFlowID _flowid;
    uint32_t _output : 24;
    uint8_t _direction;
    IPRewriterEntry *_hashnext;

    friend class HashContainer_adapter<IPRewriterEntry>;

};


class IPRewriterFlow { public:

    IPRewriterFlow(const IPFlowID &flowid, int output,
		   const IPFlowID &rewritten_flowid, int reply_output,
		   uint8_t ip_p, bool guaranteed, click_jiffies_t expiry_j,
		   IPRewriterBase *owner, int owner_input);

    IPRewriterEntry &entry(bool direction) {
	return _e[direction];
    }
    const IPRewriterEntry &entry(bool direction) const {
	return _e[direction];
    }

    click_jiffies_t expiry() const {
	return _sexpiry_j;
    }
    bool expired(click_jiffies_t now_j) const {
	return !click_jiffies_less(now_j, _sexpiry_j);
    }
    bool guaranteed() const {
	return _guaranteed;
    }

    void change_expiry(IPRewriterHeap *h, bool guaranteed,
		       click_jiffies_t expiry_j);
    void change_expiry(IPRewriterHeap *h, click_jiffies_t now_j,
		       const uint32_t timeouts[2]) {
	int timeout = timeouts[1] ? timeouts[1] : timeouts[0];
	change_expiry(h, !!timeouts[1], now_j + timeout);
    }

    enum {
	s_forward_done = 1, s_reply_done = 2,
	s_both_done = (s_forward_done | s_reply_done),
	s_forward_data = 4, s_reply_data = 8,
	s_both_data = (s_forward_data | s_reply_data)
    };
    bool both_done() const {
	return (_state & s_both_done) == s_both_done;
    }
    bool both_data() const {
	return (_state & s_both_data) == s_both_data;
    }

    uint8_t ip_p() const {
	return _ip_p;
    }

    IPRewriterBase *owner() const {
	return _owner;
    }
    int owner_input() const {
	return _owner_input;
    }

    uint8_t reply_anno() const {
	return _reply_anno;
    }
    void set_reply_anno(uint8_t reply_anno) {
	_reply_anno = reply_anno;
    }

    static inline void update_csum(uint16_t &csum, bool direction,
				   uint16_t csum_delta);

    void apply(WritablePacket *p, bool direction, unsigned annos);

    void unparse(StringAccum &sa, bool direction, click_jiffies_t now) const;
    void unparse_ports(StringAccum &sa, bool direction, click_jiffies_t now) const;

    struct less {
	less() {
	}
	bool operator()(IPRewriterFlow *a, IPRewriterFlow *b) {
	    return click_jiffies_less(a->expiry(), b->expiry());
	}
    };

    struct place {
	place(IPRewriterFlow **begin)
	    : _begin(begin) {
	}
	void operator()(IPRewriterFlow **it) {
	    (*it)->_place = it - _begin;
	}
      private:
	IPRewriterFlow **_begin;
    };

  protected:

    IPRewriterEntry _e[2];
    uint16_t _ip_csum_delta;
    uint16_t _udp_csum_delta;
    click_jiffies_t _sexpiry_j;
    size_t _place : 32;
    uint8_t _ip_p;
    uint8_t _state : 7;
    uint8_t _guaranteed : 1;
    uint8_t _tflags;
    uint8_t _reply_anno;
    IPRewriterBase *_owner;
    int _owner_input;

    friend class IPRewriterBase;
    friend class IPRewriterEntry;

  private:

    void destroy(IPRewriterHeap *heap);

};


inline IPFlowID
IPRewriterEntry::rewritten_flowid() const
{
    return (this + (_direction ? -1 : 1))->_flowid.reverse();
}

inline void
IPRewriterFlow::update_csum(uint16_t &csum, bool direction, uint16_t csum_delta)
{
    if (csum_delta)
	click_update_in_cksum(&csum, 0, direction ? csum_delta : ~csum_delta);
}

CLICK_ENDDECLS
#endif

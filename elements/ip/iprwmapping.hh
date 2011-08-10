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
class IPRewriterInput;

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

    IPRewriterFlow(IPRewriterInput *owner, const IPFlowID &flowid,
		   const IPFlowID &rewritten_flowid,
		   uint8_t ip_p, bool guaranteed, click_jiffies_t expiry_j);

    IPRewriterEntry &entry(bool direction) {
	return _e[direction];
    }
    const IPRewriterEntry &entry(bool direction) const {
	return _e[direction];
    }


    /** @brief Return the expiration time in absolute jiffies. */
    click_jiffies_t expiry() const {
	return _expiry_j;
    }

    /** @brief Test if the flow is expired as of @a now_j.
     * @param now_j current time in absolute jiffies */
    bool expired(click_jiffies_t now_j) const {
	return !click_jiffies_less(now_j, _expiry_j);
    }

    /** @brief Test if the flow is guaranteed. */
    bool guaranteed() const {
	return _guaranteed;
    }

    /** @brief Set expiration time to @a expiry_j.
     * @param h heap containing this flow
     * @param guaranteed whether the flow is guaranteed
     * @param expiry_j expiration time in absolute jiffies */
    void change_expiry(IPRewriterHeap *h, bool guaranteed,
		       click_jiffies_t expiry_j);

    /** @brief Set expiration time to a timeout after @a now_j.
     * @param h heap containing this flow
     * @param now_j current time in absolute jiffies
     * @param timeouts timeouts in jiffies: timeout[1] is guarantee, timeout[0] is best-effort
     *
     * If @a timeouts[1] is non-zero, then the flow becomes guaranteed, and
     * the guarantee will expire after @a timeouts[1] jiffies.  Otherwise, the
     * flow becomes best-effort, and will expire after @a timeouts[0]
     * jiffies. */
    void change_expiry_by_timeout(IPRewriterHeap *h, click_jiffies_t now_j,
				  const uint32_t timeouts[2]) {
	uint32_t timeout = timeouts[1] ? timeouts[1] : timeouts[0];
	change_expiry(h, !!timeouts[1], now_j + timeout);
    }

    uint8_t ip_p() const {
	return _ip_p;
    }

    IPRewriterInput *owner() const {
	return _owner;
    }

    uint8_t reply_anno() const {
	return _reply_anno;
    }
    void set_reply_anno(uint8_t reply_anno) {
	_reply_anno = reply_anno;
    }

    static inline void update_csum(uint16_t *csum, bool direction,
				   uint16_t csum_delta);

    void apply(WritablePacket *p, bool direction, unsigned annos);

    void unparse(StringAccum &sa, bool direction, click_jiffies_t now) const;
    void unparse_ports(StringAccum &sa, bool direction, click_jiffies_t now) const;

    struct heap_less {
	inline bool operator()(IPRewriterFlow *a, IPRewriterFlow *b) {
	    return click_jiffies_less(a->expiry(), b->expiry());
	}
    };
    struct heap_place {
	inline void operator()(IPRewriterFlow **begin, IPRewriterFlow **it) {
	    (*it)->_place = it - begin;
	}
    };

  protected:

    IPRewriterEntry _e[2];
    uint16_t _ip_csum_delta;
    uint16_t _udp_csum_delta;
    click_jiffies_t _expiry_j;
    size_t _place : 32;
    uint8_t _ip_p;
    uint8_t _tflags;
    bool _guaranteed;
    uint8_t _reply_anno;
    IPRewriterInput *_owner;

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
IPRewriterFlow::update_csum(uint16_t *csum, bool direction, uint16_t csum_delta)
{
    if (csum_delta)
	click_update_in_cksum(csum, 0, direction ? csum_delta : ~csum_delta);
}

CLICK_ENDDECLS
#endif

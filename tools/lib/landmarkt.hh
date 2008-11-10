#ifndef CLICKTOOL_LANDMARKT_HH
#define CLICKTOOL_LANDMARKT_HH 1
#include <click/string.hh>
#include <click/vector.hh>
class LandmarkSetT;

class LandmarkT { public:

    inline LandmarkT();
    inline LandmarkT(const String &filename, unsigned lineno);
    //inline LandmarkT(LandmarkSetT *lset, unsigned offset);
    inline LandmarkT(LandmarkSetT *lset, unsigned offset1, unsigned offset2);
    inline LandmarkT(const LandmarkT &o);
    inline ~LandmarkT();

    static void static_initialize();

    static const LandmarkT &empty_landmark();

    typedef String (LandmarkT::*unspecified_bool_type)() const;
    inline operator unspecified_bool_type() const;

    String str() const;
    operator String() const;
    String decorated_str() const;

    unsigned offset1() const		{ return _offset1; }
    unsigned offset2() const		{ return _offset2; }

    inline LandmarkT &operator=(const LandmarkT &o);

    enum { noffset = (unsigned) -1 };

  private:

    LandmarkSetT *_lset;
    unsigned _offset1;
    unsigned _offset2;

    static LandmarkT *empty;

};

class LandmarkSetT { public:

    LandmarkSetT();
    LandmarkSetT(const String &filename, unsigned lineno);

    inline void ref();
    inline void unref();

    String offset_to_string(unsigned offset) const;
    String offset_to_decorated_string(unsigned offset1, unsigned offset2) const;
    void new_line(unsigned offset, const String &filename, unsigned lineno);

    struct LandmarkInfo {
	unsigned end_offset;
	int filename;
	unsigned lineno;

	LandmarkInfo(unsigned o, int f, unsigned l)
	    : end_offset(o), filename(f), lineno(l) {
	}
    };

  private:

    int _refcount;
    Vector<LandmarkInfo> _linfo;
    Vector<String> _fnames;

    LandmarkSetT(const LandmarkSetT &);
    ~LandmarkSetT();
    LandmarkSetT &operator=(const LandmarkSetT &);

    static LandmarkSetT *the_empty_set;
    friend class LandmarkT;

};

inline void
LandmarkSetT::ref()
{
    assert(_refcount >= 1);
    ++_refcount;
}

inline void
LandmarkSetT::unref()
{
    assert(_refcount >= 1);
    if (--_refcount == 0)
	delete this;
}

inline
LandmarkT::LandmarkT()
    : _lset(empty->_lset), _offset1(noffset), _offset2(noffset)
{
    _lset->ref();
}

inline
LandmarkT::LandmarkT(const String &filename, unsigned lineno = 0)
    : _lset(new LandmarkSetT(filename, lineno)), _offset1(noffset), _offset2(noffset)
{
}

#if 0
inline
LandmarkT::LandmarkT(LandmarkSetT *lset, unsigned offset)
    : _lset(lset), _offset1(offset), _offset2(offset)
{
    _lset->ref();
}
#endif

inline
LandmarkT::LandmarkT(LandmarkSetT *lset, unsigned offset1, unsigned offset2)
    : _lset(lset), _offset1(offset1), _offset2(offset2)
{
    _lset->ref();
}

inline
LandmarkT::LandmarkT(const LandmarkT &lm)
    : _lset(lm._lset), _offset1(lm._offset1), _offset2(lm._offset2)
{
    _lset->ref();
}

inline
LandmarkT::~LandmarkT()
{
    _lset->unref();
}

inline const LandmarkT &
LandmarkT::empty_landmark()
{
    assert(empty);
    return *empty;
}

inline LandmarkT &
LandmarkT::operator=(const LandmarkT &lm)
{
    lm._lset->ref();
    _lset->unref();
    _lset = lm._lset;
    _offset1 = lm._offset1;
    _offset2 = lm._offset2;
    return *this;
}

inline
LandmarkT::operator unspecified_bool_type() const
{
    return _lset != empty->_lset ? &LandmarkT::str : 0;
}

inline String
LandmarkT::str() const
{
    return _lset->offset_to_string(_offset1);
}

inline
LandmarkT::operator String() const
{
    return _lset->offset_to_string(_offset1);
}

inline String
LandmarkT::decorated_str() const
{
    return _lset->offset_to_decorated_string(_offset1, _offset2);
}

#endif

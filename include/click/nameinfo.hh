// -*- c-basic-offset: 4; related-file-name: "../../lib/nameinfo.cc" -*-
#ifndef CLICK_NAMEINFO_HH
#define CLICK_NAMEINFO_HH
#include <click/args.hh>
#include <click/straccum.hh>
CLICK_DECLS
class Element;
class NameDB;
class ErrorHandler;

class NameInfo { public:

    /** @brief Construct a NameInfo element.
     *
     * Users never need to call this. */
    NameInfo();

    /** @brief Destroy a NameInfo object.
     *
     * Also destroys all NameDB objects installed on this NameInfo.
     *
     * Users never need to call this. */
    ~NameInfo();

    /** @brief Static initialization for NameInfo.
     *
     * Creates the global NameInfo used for databases unconnected to any
     * router.  Users never need to call this. */
    static void static_initialize();

    /** @brief Static cleanup for NameInfo.
     *
     * Destroys the global NameInfo used for databases unconnected to any
     * router.  Users never need to call this. */
    static void static_cleanup();

    /** @brief Known name database types. */
    enum DBType {
	T_NONE = 0,			///< Nonexistent names database
	T_SCHEDULEINFO = 0x00000001,	///< ScheduleInfo database
	T_ANNOTATION = 0x00000002,	///< Packet annotation database
	T_SCRIPT_INSN = 0x00000003,	///< Script instruction names database
	T_SIGNO = 0x00000004,		///< User-level signal names database
	T_SPINLOCK = 0x00000005,	///< Spinlock names database
	T_ETHERNET_ADDR = 0x01000001,	///< Ethernet address names database
	T_IP_ADDR = 0x04000001,		///< IP address names database
	T_IP_PREFIX = 0x04000002,	///< IP prefix names database
	T_IP_PROTO = 0x04000003,	///< IP protocol names database
	T_IPFILTER_TYPE = 0x04000004,	///< IPFilter instruction names database
	T_TCP_OPT = 0x04000005,		///< TCP option names database
	T_IPREWRITER_PATTERN = 0x04000006, ///< IPRewriterPattern database
	T_ICMP_TYPE = 0x04010000,	///< ICMP type names database
	T_ICMP_CODE = 0x04010100,	///< ICMP code names database
	T_IP_PORT = 0x04020000,		///< Starting point for IP per-protocol port names databases
	T_TCP_PORT = 0x04020006,	///< TCP port names database
	T_UDP_PORT = 0x04020011,	///< UDP port names database
	T_IP_FIELDNAME = 0x04030000,	///< Starting point for IP per-protocol field names databases
	T_ICMP_FIELDNAME = 0x04030001,	///< ICMP field names database
	T_TCP_FIELDNAME = 0x04030006,	///< TCP field names database
	T_UDP_FIELDNAME = 0x04030011,	///< UDP field names database
	T_IP6_ADDR = 0x06000001,	///< IPv6 address names database
	T_IP6_PREFIX = 0x06000002	///< IPv6 prefix names database
    };

    /** @brief Find or create a name database.
     * @param type database type
     * @param context compound element context
     * @param value_size size of values stored in database
     * @param create whether to create a DynamicNameDB if no database exists
     *
     * Returns an installed name database matching @a type and @a context.
     * (If @a context is non-null, then the database matches the implied
     * router and compound element context.  Otherwise, the database is the
     * unique global database for the given @a type.)  If @a create is true,
     * and no database exists exactly matching @a type and @a context, then a
     * DynamicNameDB is created and installed with that @a type, @a context,
     * and @a value_size.  Otherwise, the search bubbles up through the
     * prefixes of @a context until an installed database is found.
     *
     * Returns null if no installed database is found.  @a value_size must
     * match the value size in the returned database.
     *
     * Most users will use query() and define() directly, not call getdb().
     */
    static NameDB *getdb(uint32_t type, const Element *context,
			 size_t value_size, bool create);

    /** @brief Install a name database.
     * @param db name database
     * @param context compound element context
     *
     * Installs the given name database for the compound element context
     * implied by @a context.  (If @a context is non-null, then the database
     * is installed for the implied router and compound element context.  If
     * it is null, the database is installed globally.)  The query() and
     * define() operations apply only to installed databases.
     *
     * It is an error to install a database that has already been installed.
     * It is also an error to install a database for a compound element
     * context that already has a different database installed.  An installed
     * NameDB is automatically destroyed when its containing NameInfo is
     * destroyed (for example, when @a context's router is destroyed).
     */
    static void installdb(NameDB *db, const Element *context);

    /** @brief Uninstall a name database.
     * @param db name database
     *
     * Undoes the effects of installdb().  The given database will no longer
     * be used for query() and define() operations.
     */
    static void uninstalldb(NameDB *db);

    /** @brief Query installed databases for @a name.
     * @param type database type
     * @param context compound element context
     * @param name name to look up
     * @param value_store value storage
     * @param value_size size of value storage
     * @return true iff the query succeeded
     *
     * Queries all installed @a type databases that apply to the compound
     * element @a context, returning the most specific value matching @a name.
     * The value is stored in @a value_store.  The installed databases must
     * have the given @a value_size.
     */
    static bool query(uint32_t type, const Element *context,
		      const String &name, void *value_store, size_t value_size);

    /** @brief Query installed databases for @a name, returning a 32-bit integer value.
     * @param type database type
     * @param context compound element context
     * @param name name to look up
     * @param value_store value storage
     * @return true iff the query succeeded
     *
     * Queries all installed @a type databases that apply to the compound
     * element @a context, returning the most specific value matching @a name.
     * The value is stored in @a value_store.  The installed databases must
     * have a value size of 4.
     *
     * If no matching name is found, query_int checks whether @a name unparses
     * into a 32-bit integer value (for example, "30").  If so, *@a
     * value_store is set to the corresponding integer and true is returned.
     * Otherwise, false is returned.
     */
    static bool query_int(uint32_t type, const Element *context,
			  const String &name, int32_t *value_store);

    /** @overload */
    static bool query_int(uint32_t type, const Element *context,
			  const String &name, uint32_t *value_store);

    /** @brief Query installed databases for @a value.
     * @param type database type
     * @param context compound element context
     * @param value points to value to look up
     * @param value_size size of value
     * @return the name, or the empty string if the query failed
     *
     * Queries all installed @a type databases that apply to the compound
     * element @a context, returning the name in the most specific database
     * whose value matches @a value, or the empty string if the relevant
     * databases don't support reverse queries or no such value exists.  The
     * installed databases must have the given @a value_size.
     */
    static String revquery(uint32_t type, const Element *context,
			   const void *value, size_t value_size);

    /** @brief Query installed databases for a 32-bit integer @a value.
     * @param type database type
     * @param context compound element context
     * @param value value to look up
     * @return the name, or the empty string if the query failed
     *
     * Queries all installed @a type databases that apply to the compound
     * element @a context, returning the name in the most specific database
     * whose value matches @a value, or the empty string if the relevant
     * databases don't support reverse queries or no such value exists.  The
     * installed databases must have value size 4.
     */
    static inline String revquery_int(uint32_t type, const Element *context,
				      int32_t value);

    /** @brief Define @a name to equal @a value in the installed databases.
     * @param type database type
     * @param context compound element context
     * @param name name to define
     * @param value points to defined value
     * @param value_size size of value
     * @return true iff the name was defined
     *
     * Defines the given @a name to @a value in the installed @a type database
     * with compound element @a context.  If no database exists exactly
     * matching that @a type and @a context, a new DynamicNameDB is created
     * and installed with those values (and the given @a value_size).  A name
     * might not be defined if the existing database for that @a type and @a
     * context doesn't support definitions, or if no new database can be
     * created.  If any database exists, it must match the given @a
     * value_size.
     */
    static inline bool define(uint32_t type, const Element *context,
			      const String &name, const void *value, size_t value_size);

    /** @brief Define @a name to equal 32-bit integer @a value in the installed databases.
     * @param type database type
     * @param context compound element context
     * @param name name to define
     * @param value defined value
     * @return true iff the value was defined
     *
     * Defines the given @a name to @a value in the installed @a type database
     * with compound element @a context.  If no database exists exactly
     * matching that @a type and @a context, a new DynamicNameDB is created
     * and installed with those values (and value size 4).  A name might not
     * be defined if the existing database for that @a type and @a context
     * doesn't support definitions, or if no new database can be created.  If
     * any database exists, it must have value size 4.
     */
    static inline bool define_int(uint32_t type, const Element *context,
				  const String &name, int32_t value);

#if CLICK_NAMEDB_CHECK
    /** @cond never */
    void check(ErrorHandler *);
    static void check(const Element *, ErrorHandler *);
    /** @endcond never */
#endif

  private:

    Vector<NameDB *> _namedb_roots;
    Vector<NameDB *> _namedbs;

    inline NameDB *install_dynamic_sentinel() { return (NameDB *) this; }
    NameDB *namedb(uint32_t type, size_t size, const String &prefix, NameDB *installer);

#if CLICK_NAMEDB_CHECK
    uintptr_t _check_generation;
    void checkdb(NameDB *db, NameDB *parent, ErrorHandler *errh);
#endif

};

class NameDB { public:

    /** @brief Construct a database.
     * @param type database type
     * @param context database compound element context, as a String
     * @param value_size database value size
     *
     * @a value_size must be greater than 0. */
    inline NameDB(uint32_t type, const String &context, size_t value_size);

    /** @brief Destroy a database.
     *
     * Destroying an installed database automatically uninstalls it.
     * See NameInfo::uninstalldb(). */
    virtual ~NameDB() {
	NameInfo::uninstalldb(this);
    }

    /** @brief Return the database type. */
    uint32_t type() const {
	return _type;
    }

    /** @brief Return the database's compound element context as a string. */
    const String &context() const {
	return _context;
    }

    /** @brief Return the contextual parent database, if any.
     *
     * The contextual parent database is the unique database, for the same
     * router, with the same type(), whose context() is a prefix of this
     * database's context(), that has the longest context() of any matching
     * database.  If there is no such database returns null. */
    NameDB *context_parent() const {
	return _context_parent;
    }

    /** @brief Return the database's value size. */
    size_t value_size() const {
	return _value_size;
    }

    /** @brief Query this database for a given name.
     * @param name name to look up
     * @param value points to value storage
     * @param value_size size of value storage
     * @return true iff the query succeeded
     *
     * The @a value_size parameter must equal this database's value size. */
    virtual bool query(const String &name, void *value, size_t value_size) = 0;

    /** @brief Query this database for a given value.
     * @param value points to value to look up
     * @param value_size size of value storage
     * @return the name for the given value, or an empty string if the value
     * has not been defined
     *
     * The @a value_size parameter must equal this database's value size.
     * The default implementation always returns the empty string. */
    virtual String revquery(const void *value, size_t value_size);

    /** @brief Define a name in this database to a given value.
     * @param name name to define
     * @param value points to value to define
     * @param value_size size of value storage
     * @return true iff the name was defined
     *
     * The @a value_size parameter must equal this database's value size.
     * The default implementation always returns false. */
    virtual bool define(const String &name, const void *value, size_t value_size);

    /** @brief Define a name in this database to a 32-bit integer value.
     * @param name name to define
     * @param value value to define
     * @return true iff the name was defined
     *
     * The database's value size must equal 4.  The implementation is the same
     * as <code>define(name, &value, 4)</code>. */
    inline bool define_int(const String &name, int32_t value);

#if CLICK_NAMEDB_CHECK
    /** @cond never */
    virtual void check(ErrorHandler *);
    /** @endcond never */
#endif

  private:

    uint32_t _type;
    String _context;
    size_t _value_size;
    NameDB *_context_parent;
    NameDB *_context_sibling;
    NameDB *_context_child;
    NameInfo *_installed;

#if CLICK_NAMEDB_CHECK
    uintptr_t _check_generation;
#endif

    friend class NameInfo;

};

class StaticNameDB : public NameDB { public:

    struct Entry {
	const char *name;
	uint32_t value;
    };

    /** @brief Construct a static name database.
     * @param type database type
     * @param context database compound element context, as a String
     * @param entry pointer to static entry list
     * @param nentry number of entries
     *
     * The entry array specifies the contents of the database.  It must be
     * sorted by name: entry[i].name < entry[i+1].name for all 0 <= i <
     * nentry-1.  The entry array must also persist as long as the database is
     * in use; the database doesn't copy the entry array into its own memory,
     * but continues to use the array passed in.  The resulting database has
     * value_size() 4. */
    inline StaticNameDB(uint32_t type, const String &context,
			const Entry *entry, size_t nentry);

    /** @brief Query this database for a given name.
     * @param name name to look up
     * @param value points to value storage
     * @param value_size size of value storage
     * @return true iff the query succeeded
     *
     * The @a value_size parameter must equal 4. */
    bool query(const String &name, void *value, size_t value_size);

    /** @brief Query this database for a given value.
     * @param value points to value to look up
     * @param value_size size of value storage
     * @return the name for the given value, or an empty string if the value
     * has not been defined
     *
     * The @a value_size parameter must equal 4. */
    String revquery(const void *value, size_t value_size);

#if CLICK_NAMEDB_CHECK
    /** @cond never */
    void check(ErrorHandler *);
    /** @endcond never */
#endif

  private:

    const Entry *_entries;
    size_t _nentries;

};

class DynamicNameDB : public NameDB { public:

    /** @brief Construct a dynamic name database.
     * @param type database type
     * @param context database compound element context, as a String
     * @param value_size database value size
     *
     * @a value_size must be greater than 0.  The database is initially
     * empty. */
    inline DynamicNameDB(uint32_t type, const String &context, size_t value_size);

    /** @brief Query this database for a given name.
     * @param name name to look up
     * @param value points to value storage
     * @param value_size size of value storage
     * @return true iff the query succeeded
     *
     * The @a value_size parameter must equal this database's value size. */
    bool query(const String &name, void *value, size_t value_size);

    /** @brief Query this database for a given value.
     * @param value points to value to look up
     * @param value_size size of value storage
     * @return the name for the given value, or an empty string if the value
     * has not been defined
     *
     * The @a value_size parameter must equal this database's value size. */
    String revquery(const void *value, size_t value_size);

    /** @brief Define a name in this database to a given value.
     * @param name name to define
     * @param value points to value to define
     * @param value_size size of value storage
     * @return true iff the name was defined
     *
     * The @a value_size parameter must equal this database's value size. */
    bool define(const String &name, const void *value, size_t value_size);

#if CLICK_NAMEDB_CHECK
    /** @cond never */
    void check(ErrorHandler *);
    /** @endcond never */
#endif

  private:

    Vector<String> _names;
    StringAccum _values;
    int _sorted;

    void *find(const String &name, bool create);
    void sort();

};


inline
NameDB::NameDB(uint32_t type, const String &context, size_t vsize)
    : _type(type), _context(context), _value_size(vsize),
      _context_parent(0), _context_sibling(0), _context_child(0), _installed(0)
{
#if CLICK_NAMEDB_CHECK
    _check_generation = 0;
#endif
    assert(_value_size > 0);
}

inline
StaticNameDB::StaticNameDB(uint32_t type, const String &context, const Entry *entry, size_t nentry)
    : NameDB(type, context, sizeof(entry->value)), _entries(entry), _nentries(nentry)
{
}

inline
DynamicNameDB::DynamicNameDB(uint32_t type, const String &context, size_t vsize)
    : NameDB(type, context, vsize), _sorted(0)
{
}

inline String
NameInfo::revquery_int(uint32_t type, const Element *e, int32_t value)
{
    return revquery(type, e, &value, sizeof(value));
}

inline bool
NameInfo::define(uint32_t type, const Element *e, const String &name, const void *value, size_t vsize)
{
    if (NameDB *db = getdb(type, e, vsize, true))
	return db->define(name, value, vsize);
    else
	return false;
}

inline bool
NameInfo::define_int(uint32_t type, const Element *e, const String &name, const int32_t value)
{
    if (NameDB *db = getdb(type, e, sizeof(value), true))
	return db->define(name, &value, sizeof(value));
    else
	return false;
}

inline bool
NameDB::define_int(const String &name, const int32_t value)
{
    return define(name, &value, sizeof(value));
}


/** @class NamedIntArg
  @brief Parser class for named integers. */
struct NamedIntArg {
    NamedIntArg(uint32_t type)
	: _type(type) {
    }
    bool parse(const String &str, int &value, const ArgContext &args) {
	return NameInfo::query(_type, args.context(), str,
			       &value, sizeof(value))
	    || IntArg().parse(str, value, args);
    }
    int _type;
};

CLICK_ENDDECLS
#endif

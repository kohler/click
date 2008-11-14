// -*- c-basic-offset: 4; related-file-name: "../../lib/archive.cc" -*-
#ifndef CLICK_ARCHIVE_HH
#define CLICK_ARCHIVE_HH
#include <click/string.hh>
#include <click/vector.hh>
CLICK_DECLS
class ErrorHandler;

/** @file <click/archive.hh>
 * @brief Class for handling 'ar' archives. */

/** @class ArchiveElement
 * @brief Member of an 'ar' archive.
 *
 * The ArchiveElement class represents members of ar(1) archives.  Click uses
 * the simple ar(1) format for passing configurations with additional
 * information, such as compiled packages and element maps.  An archive member
 * consists of a name, data, and additional metadata.  Complete archives are
 * represented as vectors of ArchiveElement objects. */
struct ArchiveElement {

    String name;		///< Member name (no slashes allowed)
    int date;			///< Decimal seconds since the epoch (1/1/1970)
    int uid;			///< User ID of member
    int gid;			///< Group ID of member
    int mode;			///< File mode
    String data;		///< Member contents

    /** @brief Return true iff the member should be included in an archive. */
    bool live() const {
	return name;
    }

    /** @brief Kill a member, preventing it from being archived. */
    void kill() {
	name = String();
    }

    /** @brief Parse a string into a vector of ArchiveElement objects.
     * @param str input string
     * @param[out] ar archive elements
     * @param errh error message receiver
     * @return 0 on success, < 0 on failure */
    static int parse(const String &str, Vector<ArchiveElement> &ar,
		     ErrorHandler *errh = 0);

    /** @brief Unparse a vector of ArchiveElement objects into a string.
     * @param ar archive elements
     * @param errh error message receiver
     * @return archive string, suitable for parse() */
    static String unparse(const Vector<ArchiveElement> &ar,
			  ErrorHandler *errh = 0);

    /** @brief Locate an ArchiveElement in an archive by name.
     * @param ar archive elements
     * @param name element name
     * @return pointer to matching archive element, or 0 if none exists */
    static ArchiveElement *find(Vector<ArchiveElement> &ar, const String &name) {
	for (ArchiveElement *ae = ar.begin(); ae != ar.end(); ++ae)
	    if (ae->name == name)
		return ae;
	return 0;
    }

    /** @overload */
    static const ArchiveElement *find(const Vector<ArchiveElement> &ar, const String &name) {
	for (const ArchiveElement *ae = ar.begin(); ae != ar.end(); ++ae)
	    if (ae->name == name)
		return ae;
	return 0;
    }

};

CLICK_ENDDECLS
#endif

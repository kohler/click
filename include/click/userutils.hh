// -*- c-basic-offset: 2; related-file-name: "../../lib/userutils.cc" -*-
#ifndef CLICK_USERUTILS_HH
#define CLICK_USERUTILS_HH
#include <click/archive.hh>
#include <stdio.h>
CLICK_DECLS
class ErrorHandler;

bool glob_match(const String &string, const String &pattern);

String percent_substitute(const String &string, int format1, ...);

/** @brief strcmp() replacement that compares numbers numerically.
 * @return < 0 if @a a < @a b, > 0 if @a a > @a b, 0 if @a a == @a b
 *
 * Compares strings of digit characters like numbers, and other characters
 * using character-by-character comparison.  For example:
 * @code
 * assert(click_strcmp("a", "b") < 0);
 * assert(click_strcmp("a9", "a10") < 0);
 * assert(click_strcmp("a001", "a2") < 0);   // 1 < 2
 * assert(click_strcmp("a001", "a1") > 0);   // longer string of initial zeros
 * @endcode
 *
 * Two strings compare as equal only if they are character-by-character
 * equal. */
int click_strcmp(const String &a, const String &b);

/** @brief Portable replacement for sigaction().
 * @param signum signal number
 * @param handler signal action function
 * @param resethand true if the handler should be reset when the signal is
 * received, false otherwise
 *
 * Expands to either sigaction() or signal(). */
void click_signal(int signum, void (*handler)(int), bool resethand);

const char *filename_landmark(const char *, bool file_is_expr = false);

String file_string(FILE *, ErrorHandler * = 0);
String file_string(String, ErrorHandler * = 0);

String unique_tmpnam(const String &, ErrorHandler * = 0);
void remove_file_on_exit(const String &);
bool path_allows_default_path(String path);
String click_mktmpdir(ErrorHandler * = 0);

const char *clickpath();
void set_clickpath(const char *);

String clickpath_find_file(const String& filename, const char *subdir,
			   String default_path, ErrorHandler * = 0);
void clickpath_expand_path(const char* subdir, const String& default_path, Vector<String>&);

void parse_tabbed_lines(const String &, Vector<String> *, ...);

ArchiveElement init_archive_element(const String &, int);

String shell_quote(const String &, bool quote_tilde = false);
String shell_command_output_string(String command_line, const String &command_stdin, ErrorHandler *);

bool compressed_data(const unsigned char *, int);
FILE *open_uncompress_pipe(const String &filename, const unsigned char *, int, ErrorHandler *);

int compressed_filename(const String &filename);
FILE *open_compress_pipe(const String &filename, ErrorHandler *);

#if HAVE_DYNAMIC_LINKING
int clickdl_load_package(String, ErrorHandler *);
void clickdl_load_requirement(String, const Vector<ArchiveElement> *archive, ErrorHandler *);
#endif

CLICK_ENDDECLS
#endif

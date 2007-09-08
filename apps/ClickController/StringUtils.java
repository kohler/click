/*
 * StringUtils.java -- utilities class for manipulating Strings
 * Douglas S. J. De Couto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

import java.util.Vector;

/**
 * String utility routines.
 * 
 * @author Eddie Kohler
 */

public class StringUtils {
    
    /**
     * Convert a buffer of splitter-terminated strings into a Vector of Strings.
     */
    public static Vector split(char buf[], int offset, char terminator) {
	Vector v = new Vector();
	int pos = offset;
	while (pos < buf.length) {
	    int start = pos;
	    while (pos < buf.length && buf[pos] != terminator)
		pos++;
	    if (start < pos || pos < buf.length)
		v.addElement(new String(buf, start, pos - start));
	    pos++;
	}
	return v;
    }

    /**
     * Convert a buffer of splitter-terminated strings into a Vector of Strings.
     */
    public static Vector split(String s, int offset, char terminator) {
	Vector v = new Vector();
	int pos = offset;
	int len = s.length();
	while (pos < len) {
	    int start = pos;
	    while (pos < len && s.charAt(pos) != terminator)
		pos++;
	    if (start < pos || pos < len)
		v.addElement(s.substring(start, pos));
	    pos++;
	}
	return v;
    }

    /**
     * Split at whitespace.
     */
    public static Vector split(char buf[], int offset) {
	Vector v = new Vector();
	int pos = offset;
	while (pos < buf.length) {
	    while (pos < buf.length && buf[pos] <= ' ' && Character.isWhitespace(buf[pos]))
		pos++;
	    int start = pos;
	    while (pos < buf.length && (buf[pos] > ' ' || !Character.isWhitespace(buf[pos])))
		pos++;
	    if (start < pos || pos < buf.length)
		v.addElement(new String(buf, start, pos - start));
	    pos++;
	}
	return v;
    }

    /**
     * Split at whitespace.
     */
    public static Vector split(String s, int offset) {
	Vector v = new Vector();
	int pos = offset;
	int len = s.length();
	while (pos < len) {
	    while (pos < len && s.charAt(pos) <= ' ' && Character.isWhitespace(s.charAt(pos)))
		pos++;
	    int start = pos;
	    while (pos < len && (s.charAt(pos) > ' ' || !Character.isWhitespace(s.charAt(pos))))
		pos++;
	    if (start < pos || pos < len)
		v.addElement(s.substring(start, pos));
	    pos++;
	}
	return v;
    }

    /**
     * Split at first word.
     */
    public static Pair firstWord(String s) throws IllegalArgumentException {
	if (s.length() == 0 || Character.isWhitespace(s.charAt(0)))
	    throw new IllegalArgumentException("StringUtils.firstWord: does not begin with word");
	int pos = 0;
	int len = s.length();
	while (pos < len && (s.charAt(pos) > ' ' || !Character.isWhitespace(s.charAt(pos))))
	    pos++;
	int pos2 = pos;
	while (pos2 < len && s.charAt(pos2) <= ' ' && Character.isWhitespace(s.charAt(pos2)))
	    pos2++;
	return new Pair(s.substring(0, pos), s.substring(pos2));
    }

    /**
     * Split at first word.
     */
    public static Pair firstInteger(String s) throws IllegalArgumentException {
	Pair p = firstWord(s);
	int val = Integer.parseInt((String)p.fst);
	return new Pair(new Integer(val), p.snd);
    }

}

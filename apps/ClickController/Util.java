/*
 * Util.java -- extra stuff
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Further elaboration of this license, including a DISCLAIMER OF ANY
 * WARRANTY, EXPRESS OR IMPLIED, is provided in the LICENSE file, which is
 * also accessible at http://www.pdos.lcs.mit.edu/click/license.html
 */

import java.util.*;

public class Util {

    static public void fail(String msg) {
	System.err.println("FAILURE");
	System.err.println(msg);
	try { throw new Exception("FAILURE"); }
	catch (Exception ex) { ex.printStackTrace(); }
	System.exit(-1);
    }

    static public void assert(boolean b, String msg) {
	System.err.println("ASSERTION FAILED: " + msg);
	try { throw new Exception("ASSERTION FAILED"); }
	catch (Exception ex) { ex.printStackTrace(); }
	System.exit(-1);
    }
    
    static public void assert(boolean b) { 
	assert(b, "<no message>"); 
    }
    
    public static String printDouble(double d, int p) {
	int i = (int) d;
	StringBuffer s = new StringBuffer(Integer.toString((int) d) + ".");
	d = Math.abs(d - i);
	for (int j = 0; j < p; j++) {
	    d *= 10;
	    i = (int) d;
	    s.append(i);
	    d -= i;
	}
	return s.toString();
    }
    
    public static String printDouble(double d) { 
	return printDouble(d, 5); 
    }
    
  
    public static int _dbg_level = 1;
    
    public static void dbg(int level, String msg) {
	if (level <= _dbg_level) { 
	    System.out.println("DEBUG: " + msg);
	    System.out.flush();
	}
    }

    public static void dbg(String msg) { dbg(1, msg); }


    public static class IgnoreCaseStringComparator implements Comparator {
	public int compare(Object o1, Object o2) {
	    String s1 = (String) o1;
	    String s2 = (String) o2;
	    return s1.toLowerCase().compareTo(s2.toLowerCase());
	}
    }

}

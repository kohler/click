/*
 * Util.java -- extra stuff
 * Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
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

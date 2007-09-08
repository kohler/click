/*
 * Util.java -- extra stuff
 * Douglas S. J. De Couto
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

public class Util {

    static public void fail(String msg) {
	System.err.println("FAILURE: " + msg);
	try { throw new Exception("FAILURE"); }
	catch (Exception ex) { ex.printStackTrace(); }
	System.exit(-1);
    }

    static public void uassert(boolean b, String msg) {
        if (b) return;
        System.err.println("ASSERTION FAILED: " + msg);
	try { throw new Exception("ASSERTION FAILED"); }
	catch (Exception ex) { ex.printStackTrace(); }
	System.exit(-1);
    }
    
    static public void uassert(boolean b) { 
	uassert(b, "<no message>"); 
    }
    
    public static String printDouble(double d, int p) {
	String sgn = "";
	if (d < 0) {
	    sgn = "-";
	    d = -d;
	}
	int i = (int) Math.floor(d);
	StringBuffer s = new StringBuffer(sgn + Integer.toString(i) + ".");
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

}

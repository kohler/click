// Util.java
// Douglas S. J. De Couto
// 18 August 2000

import java.util.Vector;

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

  static public void assert(boolean b) { assert(b, "<no message>"); }

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

  public static String printDouble(double d) { return printDouble(d, 5); }

  
  public static int _dbg_level = 1;

  public static void dbg(int level, String msg) {
    if (level <= _dbg_level) { 
      System.out.println("DEBUG: " + msg);
      System.out.flush();
    }
  }

  public static void dbg(String msg) { dbg(1, msg); }

  public interface SortProc { 
    public boolean lessThanOrEqual(Object o1, Object o2);
  }

  public static class StringSortProc implements SortProc {
    StringSortProc() { }
    public boolean lessThanOrEqual(Object o1, Object o2) {
      String s1 = (String) o1;
      String s2 = (String) o2;
      return s1.compareTo(s2) <= 0;
    }
  }

  public static class IgnoreCaseStringSortProc implements SortProc {
    IgnoreCaseStringSortProc() { }
    public boolean lessThanOrEqual(Object o1, Object o2) {
      String s1 = (String) o1;
      String s2 = (String) o2;
      return s1.toLowerCase().compareTo(s2.toLowerCase()) <= 0;
    }
  }

  public static void sort(Vector a, SortProc f) {
    quickSort(a, 0, a.size() - 1, f);
  }

  public static void quickSort(Vector a, int p, int r, SortProc f) {
    // straight out of CLR
    if (p < r) {
      int q = partition(a, p, r, f);
      quickSort(a, p, q, f);
      quickSort(a, q + 1, r, f);
    }
  }

  public static void swap(Vector v, int i, int j) {
	Object t = v.elementAt(i);
	v.setElementAt(v.elementAt(j), i);
	v.setElementAt(t, j);
  }

  public static int partition(Vector a, int p, int r, SortProc f) {
    Object x = a.elementAt(p);
    int i = p - 1;
    int j = r + 1;
    while (true) {
      do j--; 
      while (!f.lessThanOrEqual(a.elementAt(j), x));
      do i++; 
      while (!f.lessThanOrEqual(x, a.elementAt(i)));
      if (i < j) 
	swap(a, i, j);
      else
	return j;
    }	
  }    
}

  

/*
 * RouterTreeModel.java -- a JTree model for element handlers
 * Douglas S. J. De Couto, Eddie Kohler
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology.
 *
 * This software is being provided by the copyright holders under the GNU
 * General Public License, either version 2 or, at your discretion, any later
 * version. For more information, see the `COPYRIGHT' file in the source
 * distribution.
 */

import javax.swing.*;
import javax.swing.tree.*;
import javax.swing.event.*;
import java.util.*;

public class RouterTreeModel extends DefaultTreeModel {

    ControlSocket _cs;
    Vector _dhl;
  
    static class HandlerUserObject {
	ControlSocket.HandlerInfo _hinfo;
	String _name;
	HandlerUserObject(ControlSocket.HandlerInfo hinfo, String name) {
	    _hinfo = hinfo;
	    _name = name;
	}
	HandlerUserObject(String el, String name, String text) {
	    _hinfo = new ControlSocket.HandlerInfo(el, name);
	    _hinfo.canRead = true;
	    _name = text;
	}
	public String toString() {
	    return (_name == null ? _hinfo.toString() : _name);
	}
    }

    private static class HandlerComparator implements Comparator {
	public int compare(Object o1, Object o2) {
	    ControlSocket.HandlerInfo h1 = (ControlSocket.HandlerInfo) o1;
	    ControlSocket.HandlerInfo h2 = (ControlSocket.HandlerInfo) o2;
	    return h1.handlerName.toLowerCase().compareTo(h2.handlerName.toLowerCase());
	}
    }

    public RouterTreeModel(ControlSocket cs) {
      super(new DefaultMutableTreeNode(cs == null ? "No connection" : cs.socketName()));
      _dhl = new Vector();
      _cs = cs;
      DefaultMutableTreeNode root = (DefaultMutableTreeNode) getRoot();

      if (cs == null)
	  return;
      
      try {
	  MutableTreeNode node = new DefaultMutableTreeNode
	      (new HandlerUserObject(null, "config", "Configuration"));
	  int nodePos = 0;
	  insertNodeInto(node, root, nodePos++);
	  
	  node = new DefaultMutableTreeNode
	      (new HandlerUserObject(null, "flatconfig", "Flat Configuration"));
	  insertNodeInto(node, root, nodePos++);
	  
	  Vector v = _cs.getConfigElementNames();
	  Collections.sort(v, new Util.IgnoreCaseStringComparator());
	  
	  Comparator handlerComparator = new HandlerComparator();
	  for (int i = 0; i < v.size(); i++) {
	      String elname = (String) v.elementAt(i);
	      DefaultMutableTreeNode elnode = new DefaultMutableTreeNode(elname);
	      Vector vh = _cs.getElementHandlers(elname);
	      Collections.sort(vh, handlerComparator);
	      for (int j = 0; j < vh.size(); j++) {
		  ControlSocket.HandlerInfo hi = (ControlSocket.HandlerInfo) vh.elementAt(j);
		  DefaultMutableTreeNode hnode = new DefaultMutableTreeNode(hi);
		  insertNodeInto(hnode, elnode, j);
	      }
	      insertNodeInto(elnode, root, nodePos++);
	  }
	  
	  insertNodeInto(new DefaultMutableTreeNode
	    (new HandlerUserObject(null, "version", "Click version")),
			 root, nodePos++);

	  insertNodeInto(new DefaultMutableTreeNode
	    (new HandlerUserObject(null, "requirements", "Router requirements")),
			 root, nodePos++);

	  insertNodeInto(new DefaultMutableTreeNode
	    (new HandlerUserObject(null, "packages", "Installed packages")),
			 root, nodePos++);

	  insertNodeInto(new DefaultMutableTreeNode
	    (new HandlerUserObject(null, "classes", "Known element classes")),
			 root, nodePos++);
	  
      } catch (Throwable ex) {
	  ex.printStackTrace();
	  Util.fail("fucked");
      }
  }

  public interface DisplayHandlerListener {
    public void displayHandler(String h, String data);
  }

  public void installDisplayHandlerListener(DisplayHandlerListener l) {
    _dhl.addElement(l);
  }

}

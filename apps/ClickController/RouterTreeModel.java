/*
 * RouterTreeModel.java -- a JTree model for element handlers
 * Douglas S. J. De Couto, Eddie Kohler
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

import javax.swing.tree.*;
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
	  Collections.sort(v, String.CASE_INSENSITIVE_ORDER);
	  
	  Comparator handlerComparator = new HandlerComparator();
	  for (int i = 0; i < v.size(); i++) {
	      // The element might not actually exist!
	      try {
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
	      } catch (ClickException e) {
	      }
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

// RouterTreeModel.java
// Douglas S. J. De Couto
// 22 August 2000

import javax.swing.*;
import javax.swing.tree.*;
import javax.swing.event.*;
import java.util.Vector;

public class HandlerSelector implements TreeSelectionListener {

    ControlSocket _cs;
    JLabel _label;
    JTextArea _result;

    HandlerSelector(ControlSocket cs, JLabel label, JTextArea result) {
      _cs = cs;
      _label = label;
      _result = result;
    }

    public void valueChanged(TreeSelectionEvent e) {
      TreePath path = e.getNewLeadSelectionPath();
      if (path == null) {
	return;
      }
      
      DefaultMutableTreeNode node = (DefaultMutableTreeNode)path.getLastPathComponent();
      Object o = node.getUserObject();
      
      ControlSocket.HandlerInfo hi = null;
      if (o instanceof ControlSocket.HandlerInfo)
	hi = (ControlSocket.HandlerInfo) o;
      else if (o instanceof RouterTreeModel.HandlerUserObject)
	hi = ((RouterTreeModel.HandlerUserObject) o)._hinfo;
      
      if (hi != null && hi.can_read) {
	try {
	  String s = _cs.readString(hi);
	  if (hi.element != null)
	      _label.setText(hi.element + "." + hi.name);
	  else
	      _label.setText(hi.name);
	  _result.setText(s);
	} catch (Throwable t) {
	  _result.setText(t.toString());
	}
      }
    }
}

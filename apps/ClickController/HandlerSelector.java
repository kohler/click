// HandlerSelector.java
// Eddie Kohler

import javax.swing.*;
import javax.swing.tree.*;
import javax.swing.event.*;
import java.util.Vector;

public class HandlerSelector implements TreeSelectionListener {

    ClickController _controller;

    HandlerSelector(ClickController controller) {
	_controller = controller;
    }

    public void valueChanged(TreeSelectionEvent e) {
	TreePath path = e.getNewLeadSelectionPath();
	if (path == null)
	    return;
	
	DefaultMutableTreeNode node = (DefaultMutableTreeNode)path.getLastPathComponent();
	Object o = node.getUserObject();
	if (o == null)
	    return;
	
	ControlSocket.HandlerInfo hi = null;
	if (o instanceof ControlSocket.HandlerInfo)
	    hi = (ControlSocket.HandlerInfo) o;
	else if (o instanceof RouterTreeModel.HandlerUserObject)
	    hi = ((RouterTreeModel.HandlerUserObject) o)._hinfo;
	if (hi == null)
	    return;
	_controller.selectHandler(hi);
    }

}

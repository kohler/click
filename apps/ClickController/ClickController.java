/*
 * ClickController.java -- main ClickController program
 * Eddie Kohler, Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
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

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.tree.*;
import javax.swing.event.*;
import java.net.InetAddress;
import java.util.Vector;

class ClickController extends JPanel {
    
    private JFrame _frame;
    private JMenuItem _closeItem;
    private JLabel _statusLine;

    private JPanel _infoPanel;
    private JLabel _handlerLabel;
    private JTextArea _handlerText;
    private JButton _changeButton;
    private ControlSocket.HandlerInfo _selectedHandler;

    private ControlSocket _cs;

    private static Vector controllers = new Vector();

    
    public ClickController controller() {
	return this;
    }

    public void enableClose() {
	if (_closeItem != null)
	    _closeItem.setEnabled(countControllers() > 1
				  || !firstController().empty());
    }

    private static int countControllers() {
	synchronized (controllers) {
	    return controllers.size();
	}
    }

    private static void addController(ClickController cntr) {
	synchronized (controllers) {
	    if (!controllers.contains(cntr)) {
		controllers.addElement(cntr);
		for (int i = 0; i < controllers.size() - 1; i++)
		    ((ClickController) controllers.elementAt(i)).enableClose();
	    }
	}
    }

    private static void removeController(ClickController cntr) {
	synchronized (controllers) {
	    if (controllers.removeElement(cntr)) {
		for (int i = 0; i < controllers.size(); i++)
		    ((ClickController) controllers.elementAt(i)).enableClose();
		if (controllers.size() == 0)
		    System.exit(0);
	    }
	}
    }

    private static ClickController firstController() {
	synchronized (controllers) {
	    return (ClickController) controllers.elementAt(0);
	}
    }

    public boolean isApplet() {
	return false;
    }

    public boolean empty() {
	return _cs == null;
    }

    /**
     * Returns the frame instance
     */
    public JFrame getFrame() {
	return _frame;
    }


    class OpenAction extends AbstractAction {
	ClickController _cntr;
	OpenAction(ClickController cntr) {
	    _cntr = cntr;
	}
        public void actionPerformed(ActionEvent e) {
	    new NewConnectionDialog(_cntr);
        }
    }

    public JMenuBar createMenus() {
	JMenuItem mi;
	JMenuBar menuBar = new JMenuBar();

	JMenu connMenu = (JMenu) menuBar.add(new JMenu("Connections"));
        connMenu.setMnemonic('C');

        mi = createMenuItem(connMenu, "Open...", 'O', new OpenAction(this));
	mi.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_O, Event.CTRL_MASK));

	mi = createMenuItem(connMenu, "Refresh", 'R', new AbstractAction() {
		public void actionPerformed(ActionEvent e) {
		    setControlSocket(_cs);
		}
	    });
	mi.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_R, Event.CTRL_MASK));

        _closeItem = createMenuItem(connMenu, "Close", 'C', new AbstractAction() {
		public void actionPerformed(ActionEvent e) {
		    if (countControllers() > 1) {
			getFrame().dispose();
			removeController(controller());
		    } else {
			setControlSocket(null);
			setStatusLine("Not connected");
			enableClose();
		    }
		}

	    });
	_closeItem.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_W, Event.CTRL_MASK));

	if (!isApplet()) {
	    connMenu.addSeparator();
	    mi = createMenuItem(connMenu, "Exit", 'x', new AbstractAction() {
		    public void actionPerformed(ActionEvent e) {
			System.exit(0);
		    }
		});
	    mi.setAccelerator(KeyStroke.getKeyStroke(KeyEvent.VK_Q, Event.CTRL_MASK));
	}

	return menuBar;
    }

    /**
     * Creates a generic menu item
     */
    public JMenuItem createMenuItem(JMenu menu, String label, char mnemonic,
				    Action action) {
        JMenuItem mi = (JMenuItem) menu.add(new JMenuItem(label));
	mi.setMnemonic(mnemonic);
	mi.addActionListener(action);
	if(action == null) {
	    mi.setEnabled(false);
	}
	return mi;
    }

    
    private static class CloseWindowAdapter extends WindowAdapter {
	private ClickController _cntr;
	public CloseWindowAdapter(ClickController cntr) {
	    _cntr = cntr;
	}
	public void windowClosing(WindowEvent e) {
	    _cntr.doClose();
	}
    }

    public static void main(String argv[]) {
	ClickController cntr = newWindow();
	if (argv.length >= 2)
	    cntr.connectTo(argv[0], argv[1]);
	else
	    cntr.setStatusLine("Not connected");
    }
    

    public ClickController(JFrame frame) {
	_frame = frame;
	_cs = null;
	addController(this);

	setLayout(new BorderLayout());

	JMenuBar menuBar = createMenus();
	add(menuBar, BorderLayout.NORTH);
	
	_infoPanel = new JPanel(new BorderLayout());
	add(_infoPanel, BorderLayout.CENTER);

	_statusLine = new JLabel("");
	add(_statusLine, BorderLayout.SOUTH);
    }




    private class HandlerSelectAction implements TreeSelectionListener {

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
	    selectHandler(hi);
	}
	
    }

    private class ChangeButtonAction extends AbstractAction {

	public void actionPerformed(ActionEvent event) {
	    if (_selectedHandler != null && _selectedHandler.canWrite) {
		try {
		    _cs.write(_selectedHandler, _handlerText.getText());
		    if (_selectedHandler.canRead)
			_handlerText.setText(_cs.readString(_selectedHandler));
		    else
			_handlerText.setText("");
		} catch (ClickException e) {
		    JOptionPane.showMessageDialog(getFrame(), e.getMessage(), "Write Handler Error", JOptionPane.ERROR_MESSAGE);
		} catch (java.io.IOException e) {
		    JOptionPane.showMessageDialog(getFrame(), e.getMessage(), "Write Handler Network Error", JOptionPane.ERROR_MESSAGE);
		}
	    }
	}

    }

    public void setControlSocket(ControlSocket cs) {
	_cs = cs;
	_selectedHandler = null;
	_infoPanel.removeAll();

	if (cs != null) {
	    JPanel result_panel = new JPanel();
	    result_panel.setLayout(new BorderLayout());

	    JPanel buttonPanel = new JPanel();
	    buttonPanel.setBorder(new javax.swing.border.EmptyBorder(3, 5, 2, 5));
	    buttonPanel.setLayout(new BorderLayout());
	    result_panel.add(buttonPanel, BorderLayout.NORTH);
	    
	    _handlerLabel = new JLabel(" ");
	    buttonPanel.add(_handlerLabel, BorderLayout.CENTER);
	    
	    _changeButton = new JButton("Change");
	    buttonPanel.add(_changeButton, BorderLayout.EAST);
	    _changeButton.setEnabled(false);

	    _handlerText = new JTextArea(" ");
	    _handlerText.setMargin(new Insets(0, 3, 0, 3));
	    JScrollPane result_scroll = new JScrollPane
		(_handlerText, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, 
		 JScrollPane.HORIZONTAL_SCROLLBAR_AS_NEEDED);
	    result_scroll.setPreferredSize(new Dimension(280, 250));
	    result_panel.add(result_scroll, BorderLayout.CENTER);
	    
	    JTree rtree = new JTree(new RouterTreeModel(cs));
	    _changeButton.addActionListener(new ChangeButtonAction());
	    rtree.addTreeSelectionListener(new HandlerSelectAction());
	    JScrollPane router_scroll = new JScrollPane(rtree);
	    
	    JSplitPane split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT,
					      router_scroll, result_panel);
	    _infoPanel.add(split, BorderLayout.CENTER);
	} else {
	    _infoPanel.repaint();
	    _handlerLabel = null;
	    _handlerText = null;
	    _changeButton = null;
	}

	getFrame().getRootPane().validate();
    }

    void selectHandler(ControlSocket.HandlerInfo hinfo) {
	_selectedHandler = hinfo;
	if (hinfo == null) {
	    _handlerLabel.setText("");
	    _handlerText.setText("");
	    _handlerText.setEditable(false);
	    _changeButton.setEnabled(false);
	    return;
	}
	
	boolean writeEnabled = hinfo.canWrite;
	_handlerLabel.setText(hinfo.getDescription());
	if (hinfo.canRead) {
	    try {
		String s = _cs.readString(hinfo);
		_handlerText.setText(s);
		_handlerText.setCaretPosition(0);
	    } catch (Throwable t) {
		_handlerText.setText(t.toString());
		writeEnabled = false;
	    }
	} else {
	    _handlerText.setText("");
	}
	_handlerText.setEditable(writeEnabled);
	_changeButton.setEnabled(writeEnabled);
    }

    public Dimension getPreferredSize() {
	return new Dimension(500, 320);
    }

    public void setStatusLine(String s) {
	_statusLine.setText(s);
    }

    public void doClose() {
	removeController(this);
    }
    
    public static ClickController newWindow() {
	JFrame frame = new JFrame("Click Controller");
	ClickController cntr = new ClickController(frame);
	frame.getContentPane().add(cntr, BorderLayout.CENTER);
	frame.addWindowListener(new CloseWindowAdapter(cntr));
	cntr.enableClose();
	frame.pack();
	frame.show();
	return cntr;
    }

    public void connectTo(String hostname, String portname) {
	ControlSocket cs = null;
	String statusLine = "Not connected";
	
	try {
	    InetAddress host_inet = InetAddress.getByName(hostname);
	    int port = Integer.parseInt(portname);
	    cs = new ControlSocket(host_inet, port);
	    statusLine = "Connected to " + hostname + ":" + port;
	} catch (java.net.UnknownHostException ex) {
	    statusLine = "Connection error: no such host `" + hostname + "'";
	} catch (NumberFormatException ex) {
	    statusLine = "Connection error: port number not an integer";
	} catch (Throwable ex) {
	    statusLine = "Connection error: " + ex.getMessage();
	}
	
	setControlSocket(cs);
	enableClose();
	setStatusLine(statusLine);
    }

}

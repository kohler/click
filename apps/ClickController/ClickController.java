import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.tree.*;
import javax.swing.event.*;
import java.net.InetAddress;
import java.util.Vector;

class ClickController extends JPanel {
    
    private JFrame _frame;
    private JPanel _infoPanel;
    private JMenuItem _closeItem;
    private JTextField _statusLine;
    private boolean _empty;

    private static Vector controllers = new Vector();

    private static final int BAD_PORT = -92138491;
    
    
    public ClickController controller() {
	return this;
    }

    public void enableClose() {
	if (_closeItem != null)
	    _closeItem.setEnabled(countControllers() > 1);
    }

    private static int countControllers() {
	synchronized (controllers) {
	    return controllers.size();
	}
    }

    private static void addController(ClickController cntr) {
	synchronized (controllers) {
	    if (!controllers.contains(cntr)) {
		controllers.add(cntr);
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

    public boolean isApplet() {
	return false;
    }

    public boolean empty() {
	return _empty;
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
        _closeItem = createMenuItem(connMenu, "Close", 'C', new AbstractAction() {
		public void actionPerformed(ActionEvent e) {
		    getFrame().dispose();
		    removeController(controller());
		}

	    });
	enableClose();
	if (!isApplet()) {
	    connMenu.addSeparator();
	    mi = createMenuItem(connMenu, "Exit", 'x', new AbstractAction() {
		    public void actionPerformed(ActionEvent e) {
			System.exit(0);
		    }
		});
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
	String host = null;
	if (argv.length > 0)
	    host = argv[0];
	
	int port = 7777;
	if (argv.length > 1) {
	    try {
		port = Integer.parseInt(argv[1]);
	    } catch (Throwable t) {
		port = BAD_PORT;
	    }
	}
	
	ControlSocket cs = null;
	String statusLine;
	try {
	    InetAddress host_inet = InetAddress.getByName(host);
	    cs = new ControlSocket(host_inet, port);
	    statusLine = "Connected to " + host + ":" + port;
	} catch (Throwable ex) {
	    if (port == BAD_PORT) {
		statusLine = "Connection error: port number not an integer";
	    } else {
		statusLine = "Connection error: " + ex.getMessage();
	    }
	}
	newWindow(cs, statusLine);
    }
    

    public ClickController(JFrame frame) {
	_frame = frame;
	_empty = true;
	addController(this);

	setLayout(new BorderLayout());
	_statusLine = new JTextField("");

	JPanel top = new JPanel();
	top.setLayout(new BorderLayout());
	add(top, BorderLayout.NORTH);
	
	JMenuBar menuBar = createMenus();
	top.add(menuBar, BorderLayout.NORTH);
	
	_infoPanel = new JPanel(new BorderLayout());
	top.add(_infoPanel, BorderLayout.CENTER);

	_statusLine.setEditable(false);
	add(_statusLine, BorderLayout.SOUTH);

	setControlSocket(null);
    }


    public void setControlSocket(ControlSocket cs) {
	_empty = (cs == null);
	_infoPanel.removeAll();
	if (cs == null) {
	    getFrame().pack();
	    return;
	}

	JPanel result_panel = new JPanel(new BorderLayout());
	JLabel label = new JLabel(" ");
	result_panel.add(label, BorderLayout.NORTH);

	JTextArea result_area = new JTextArea("");
	JScrollPane result_scroll = new JScrollPane
	    (result_area, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, 
	     JScrollPane.HORIZONTAL_SCROLLBAR_AS_NEEDED);
	result_panel.add(result_scroll, BorderLayout.CENTER);

	JTree rtree = new JTree(new RouterTreeModel(cs));
	rtree.addTreeSelectionListener(new HandlerSelector(cs, label, result_area));
	JScrollPane router_scroll = new JScrollPane(rtree);
	
	JSplitPane split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT,
					  router_scroll, result_panel);
	_infoPanel.add(split, BorderLayout.CENTER);
	getFrame().pack();
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
    
    public static void newWindow(ControlSocket cs, String statusLine) {
	JFrame frame = new JFrame("Click Controller");
	ClickController cntr = new ClickController(frame);
	frame.getContentPane().add(cntr, BorderLayout.CENTER);
	frame.addWindowListener(new CloseWindowAdapter(cntr));
	cntr.setControlSocket(cs);
	cntr.setStatusLine(statusLine);
	frame.pack();
	frame.setVisible(true);
    }

}

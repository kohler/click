import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.tree.*;
import javax.swing.event.*;
import java.net.InetAddress;
import java.util.Vector;

class ClickController extends JPanel {
    
    private JFrame _frame;
    private JTextField _statusLine;
    
    public boolean isApplet() {
	return false;
    }

    /**
     * Returns the frame instance
     */
    public JFrame getFrame() {
	return _frame;
    }


    class OpenAction extends AbstractAction {
        public void actionPerformed(ActionEvent e) {
	    System.exit(0);
        }
    }
    
    class ExitAction extends AbstractAction {
        protected ExitAction() {
            super("ExitAction");
        }
        public void actionPerformed(ActionEvent e) {
	    System.exit(0);
        }
    }
    
    public JMenuBar createMenus() {
	JMenuItem mi;
	JMenuBar menuBar = new JMenuBar();

	JMenu connMenu = (JMenu) menuBar.add(new JMenu("Connections"));
        connMenu.setMnemonic('C');
        mi = createMenuItem(connMenu, "Open", 'O', new AbstractAction() {
		public void actionPerformed(ActionEvent e) {
		    new NewConnectionDialog(getFrame());
		}
	    });
        mi = createMenuItem(connMenu, "Close", 'C', null);
	if (!isApplet()) {
	    connMenu.addSeparator();
	    mi = createMenuItem(connMenu, "Exit", 'x', new ExitAction());
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


    public static void main(String argv[]) {
	String host = null;
	if (argv.length > 0)
	    host = argv[0];
	
	int port = 7777;
	if (argv.length > 1) 
	    port = Integer.parseInt(argv[1]);
	
	JFrame frame = new JFrame("Click Controller");
	ClickController mw = new ClickController(frame, host, port);
	frame.getContentPane().add(mw, BorderLayout.CENTER);
	frame.addWindowListener(new WindowAdapter() {
		public void windowClosing(WindowEvent e) {
		    System.exit(0);
		}
	    });
	frame.pack();
	frame.setVisible(true);
    }
    

    ClickController(JFrame frame, String host, int port) {
	_frame = frame;
	setLayout(new BorderLayout());
	_statusLine = new JTextField("");

	JPanel top = new JPanel();
	top.setLayout(new BorderLayout());
	add(top, BorderLayout.NORTH);
	
	JMenuBar menuBar = createMenus();
	top.add(menuBar, BorderLayout.NORTH);
      
	JPanel result_panel = new JPanel(new BorderLayout());
	JLabel label = new JLabel(" ");
	result_panel.add(label, BorderLayout.NORTH);

	JTextArea result_area = new JTextArea("");
	JScrollPane result_scroll = new JScrollPane
	    (result_area, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED, 
	     JScrollPane.HORIZONTAL_SCROLLBAR_AS_NEEDED);
	result_panel.add(result_scroll, BorderLayout.CENTER);

	ControlSocket cs = null;
	try {
	    InetAddress host_inet = InetAddress.getByName(host);
	    cs = new ControlSocket(host_inet, port);
	    _statusLine.setText("Connected to " + host + ":" + port);
	} catch (Throwable ex) {
	    _statusLine.setText("Connection error: " + ex.getMessage());
	}
	JTree rtree = new JTree(new RouterTreeModel(cs));
	if (cs != null)
	    rtree.addTreeSelectionListener(new HandlerSelector(cs, label, result_area));
	JScrollPane router_scroll = new JScrollPane(rtree);
	
	JSplitPane split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT,
					  router_scroll, result_panel);
	add(split, BorderLayout.CENTER);

	_statusLine.setEditable(false);
	add(_statusLine, BorderLayout.SOUTH); 
    }

}

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.tree.*;
import javax.swing.event.*;
import java.net.InetAddress;
import java.util.Vector;

class NewConnectionDialog extends JDialog {
    
    private ClickController _cntr;
    private JTextField _hostname;
    private JTextField _port;

    class DoneAction extends AbstractAction {
	JDialog dialog;
	boolean ok;
        protected DoneAction(JDialog dialog, boolean ok) {
            super("DoneAction");
	    this.dialog = dialog;
	    this.ok = ok;
        }
        public void actionPerformed(ActionEvent e) {
	    if (ok) {
		String hostname = _hostname.getText().trim();
		String portstr = _port.getText().trim();
		ControlSocket cs = null;
		String statusLine;
		try {
		    int port = Integer.parseInt(portstr);
		    InetAddress host_inet = InetAddress.getByName(hostname);
		    cs = new ControlSocket(host_inet, port);
		    statusLine = "Connected to " + hostname + ":" + port;
		} catch (Throwable t) {
		    statusLine = "Connection error: " + t.getMessage();
		}
		if (_cntr.empty()) {
		    _cntr.setControlSocket(cs);
		    _cntr.setStatusLine(statusLine);
		} else {
		    ClickController.newWindow(cs, statusLine);
		    _cntr.enableClose();
		}
	    }
	    dialog.dispose();
	}
    }

    public NewConnectionDialog(ClickController cntr) {
	super(cntr.getFrame(), "New Connection", true);
	_cntr = cntr;
	
	getContentPane().setLayout(new BorderLayout());
	
	
	JPanel p = new JPanel() {
	    Insets insets = new Insets(0,0,10,0);
	    public Insets getInsets() {
		return insets;
	    }
	};

	JLabel label = new JLabel("Hostname:");
	_hostname = new JTextField("");

	Dimension HGAP5 = new Dimension(5, 1);
	p.setLayout(new BoxLayout(p, BoxLayout.X_AXIS));
	p.add(Box.createRigidArea(HGAP5));
	p.add(label, BorderLayout.WEST);
	p.add(Box.createRigidArea(HGAP5));
	p.add(_hostname, BorderLayout.CENTER);
	p.add(Box.createRigidArea(HGAP5));

	getContentPane().add(p, BorderLayout.NORTH);


	p = new JPanel() {
	    Insets insets = new Insets(0,0,10,0);
	    public Insets getInsets() {
		return insets;
	    }
	};

	label = new JLabel("Port:");
	_port = new JTextField("7777");

	p.setLayout(new BoxLayout(p, BoxLayout.X_AXIS));
	p.add(Box.createRigidArea(HGAP5));
	p.add(label, BorderLayout.WEST);
	p.add(Box.createRigidArea(HGAP5));
	p.add(_port, BorderLayout.CENTER);
	p.add(Box.createRigidArea(HGAP5));

	getContentPane().add(p, BorderLayout.CENTER);


	JPanel buttonpanel = new JPanel();
	buttonpanel.setOpaque(false);
	JButton button = (JButton) buttonpanel.add(new JButton("OK"));
	button.addActionListener(new DoneAction(this, true));
	button = (JButton) buttonpanel.add(new JButton("Cancel"));
	button.addActionListener(new DoneAction(this, false));
	
	getContentPane().add(buttonpanel, BorderLayout.SOUTH);

	pack();
	show();
    }

}

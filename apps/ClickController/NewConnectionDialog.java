/*
 * NewConnectionDialog.java -- open a new connection to a ControlSocket
 * Eddie Kohler
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

import java.awt.*;
import java.awt.event.*;
import javax.swing.*;

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
		ClickController cntr = _cntr;
		if (!_cntr.empty())
		    cntr = ClickController.newWindow();
		cntr.connectTo(_hostname.getText().trim(), _port.getText().trim());
	    }
	    dialog.dispose();
	}
    }

    public NewConnectionDialog(ClickController cntr) {
	super(cntr.getFrame(), "New Connection", true);
	_cntr = cntr;
	
	getContentPane().setLayout(new BorderLayout());
	
	
	JPanel p = new JPanel() {
	    Insets insets = new Insets(10,0,10,0);
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
	setLocationRelativeTo(_cntr.getFrame().getContentPane());
	show();
    }

}

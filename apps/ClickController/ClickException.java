/*
 * ClickException.java -- Click-related exceptions
 * Eddie Kohler, Douglas S. J. De Couto
 *
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2001 International Computer Science Institute
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

/**
 * Click-related exceptions.
 * 
 * @author Eddie Kohler, Douglas S. J. De Couto
 */

public class ClickException extends Exception {

    ClickException() {
	super("Click error");
    }
    ClickException(String s) {
	super(s);
    }

    static public class NoSuchElementException extends ClickException {
	NoSuchElementException() { 
	    super("No such element"); 
	}
	NoSuchElementException(String s) { 
	    super("No such element `" + s + "'"); 
	}
    }
    
    static public class HandlerException extends ClickException {
	private String _handlerId;
	HandlerException(String handlerId, String description) {
	    super(description);
	    _handlerId = handlerId;
	}
	public String getHandlerId() {
	    return _handlerId;
	}
    }

    static public class NoSuchHandlerException extends HandlerException {
	NoSuchHandlerException() { 
	    super("", "No such handler"); 
	}
	NoSuchHandlerException(String hid) {
	    super(hid, "No such handler `" + hid + "'"); 
	}
    }

    static public class PermissionDeniedException extends HandlerException {
	PermissionDeniedException() {
	    super("", "Permission denied calling handler"); 
	}
	PermissionDeniedException(String hid) {
	    super(hid, "Permission denied calling handler `" + hid + "'"); 
	}
    }

    static public class HandlerErrorException extends HandlerException {
	HandlerErrorException() { 
	    super("", "Unspecified error calling handler"); 
	}
	HandlerErrorException(String hid, String description) { 
	    super(hid, description);
	}
    }

    static public class HandlerFormatException extends HandlerException {
	HandlerFormatException() {
	    super("", "Unspecified error calling handler"); 
	}
	HandlerFormatException(String hid) { 
	    super(hid, "Bad format in handler `" + hid + "'"); 
	}
    }

}

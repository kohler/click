/*
 * Pair.java -- utilities class for manipulating Strings
 * Eddie Kohler
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
 * Utility routines.
 * 
 * @author Eddie Kohler
 */

public class Pair {

    public Object fst;
    public Object snd;

    public Pair() {
	fst = null;
	snd = null;
    }

    public Pair(Object a, Object b) {
	fst = a;
	snd = b;
    }

}

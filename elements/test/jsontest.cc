// -*- c-basic-offset: 4 -*-
/*
 * jsontest.{cc,hh} -- regression test element for Json
 * Eddie Kohler
 *
 * Copyright (c) 2012 Eddie Kohler
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

#include <click/config.h>
#include "jsontest.hh"
#include <click/error.hh>
#include "elements/json/json.hh"
CLICK_DECLS

JsonTest::JsonTest()
{
}

#define CHECK(x) if (!(x)) return errh->error("%s:%d: test %<%s%> failed", __FILE__, __LINE__, #x);

int
JsonTest::initialize(ErrorHandler *errh)
{
    Json j;
    CHECK(j.empty());
    CHECK(!j);

    j = Json::make_object();
    CHECK(j.empty());
    CHECK(j);

    j.set("foo", "bar");
    CHECK(j["foo"]);
    CHECK(j["foo"].to_s() == "bar");
    CHECK(j.size() == 1);

    j.set("baz", "flim");
    CHECK(j.size() == 2);
    CHECK(j.unparse() == "{\"foo\":\"bar\",\"baz\":\"flim\"}");

    j.erase("foo");
    CHECK(j.size() == 1);

    j.assign_parse("2");
    CHECK(j == 2);

    j.assign_parse("null");
    CHECK(j == Json());

    j.assign_parse("\"a\"");
    CHECK(j == "a");

    j.assign_parse("[1,2]");
    CHECK(j.unparse() == "[1,2]");

    j.assign_parse("[[[]],{\"a\":{}}]");
    CHECK(j.unparse() == "[[[]],{\"a\":{}}]");

    j = Json::parse("{\"x22\":{\n\
      \"git-revision\":\"ebbd3d4767847300f552b181a10bda57a926f554M\",\n\
      \"time\":\"Tue Feb  7 20:20:33 2012\",\n\
      \"machine\":\"rtshanks-laptop\",\n\
      \"cores\":2,\n\
      \"runs\":[\"x22\\/rw2\\/mb\\/0\"]\n\
    },\n\
    \"x23\":{\n\
      \"git-revision\":\"ebbd3d4767847300f552b181a10bda57a926f554M\",\n\
      \"time\":\"Tue Feb  7 20:31:05 2012\",\n\
      \"machine\":\"rtshanks-laptop\",\n\
      \"cores\":2,\n\
      \"runs\":[\"x23\\/rw2\\/mb\\/0\",\"x23\\/rw1\\/mb\\/0\",\"x23\\/rw3\\/mb\\/0\"]\n\
    },\n\
    \"x24\":{\n\
      \"git-revision\":\"62e9970ca8ae9c6eebf2d71b7065ea694fb25282M\",\n\
      \"time\":\"Sat Feb 11 15:54:01 2012\",\n\
      \"machine\":\"rtshanks-laptop\",\n\
      \"cores\":2,\n\
      \"runs\":[\"x24\\/rw1\\/b\\/0\"]\n\
    },\"b\":\"c\",\"c\":\"d\"}");
    CHECK(j["x22"]["time"] == "Tue Feb  7 20:20:33 2012");
    CHECK(j["x22"]["cores"] == 2);
    {
	Json::object_iterator it = j.obegin();
	CHECK(it.key() == "x22");
	++it;
	CHECK(it.key() == "x23");
	++it;
	CHECK(it.key() == "x24");
	++it;
	CHECK(it.key() == "b");
	++it;
	CHECK(it.key() == "c");
    }

    {
	Json jcopy = j;
	CHECK(j.size() == 5);
	int count = 0;
	for (Json::object_iterator it = jcopy.begin(); it != jcopy.end(); ++it) {
	    it->second = Json();
	    ++count;
	}
	CHECK(!jcopy["x22"]);
	CHECK(j["x22"]["cores"] == 2);
	CHECK(count == jcopy.size());
    }

    CHECK(!j["x49"]);
    CHECK(j.size() == 5);
    CHECK(!j["x49"]["45"][2]["a"]);
    CHECK(j.size() == 5);
    j["x49"]["45"][2]["a"] = 1;
    CHECK(j.size() == 6);
    CHECK(j["x22"].is_object() && j.get("x22").is_object());
    CHECK(j["x23"].is_object() && j.get("x23").is_object());
    CHECK(j["b"].is_string() && j.get("b").is_string());
    CHECK(j["x49"].is_object() && j.get("x49").is_object());
    CHECK(j["x49"]["45"].is_array());
    CHECK(j["x49"]["45"].size() == 3 && j["x49"]["45"][2].is_object());

    j = Json::make_object();
    j["a"] = j["b"];
    CHECK(j.size() == 1);
    CHECK(j["a"].is_null());
    CHECK(j.count("a") == 1);

    j = Json::make_object();
    Json k = Json::make_array();
    j["a"] = k[2];
    CHECK(j.size() == 1);
    CHECK(k.size() == 0);
    CHECK(j["a"].is_null());
    CHECK(j.count("a") == 1);

    j = Json(1);
    CHECK(j.get("a").is_null());

    j.assign_parse("{\"a\":1,\"b\":true,\"c\":\"\"}");
    {
	int i = 0;
	bool b = false;
	String s;
	CHECK(j.get("a", i));
	CHECK(i == 1);
	CHECK(j.get("a", i).get("b", b));
	CHECK(b == true);
	CHECK(!j.get("a", s).status());
	CHECK(!j.get("a", s).status(b));
	CHECK(b == false);
	CHECK(j.get("a", k));
	CHECK(k == Json(1));
	CHECK(!j.get("cc", k));
    }

    j["a"] = Json(5);
    j.set("a", Json(5));
    j["b"] = Json::parse("[]");

    {
	Json j1 = Json::make_object(), j2 = Json::make_object();
	j1.set("a", j2); // stores a COPY of j2 in j1
	j2.set("b", 1);
	CHECK(j1.unparse() == "{\"a\":{}}");
	CHECK(j2.unparse() == "{\"b\":1}");
    }

    {
	Json j = Json::parse("{\"a\":true}");
	if (j["foo"]["bar"])
	    CHECK(false);
	CHECK(j.unparse() == "{\"a\":true}");
	j["foo"]["bar"] = true;
	CHECK(j.unparse() == "{\"a\":true,\"foo\":{\"bar\":true}}");
	//j["a"]["2"] = false;
	//CHECK(j.unparse() == "{\"a\":{\"2\":false},\"foo\":{\"bar\":true}}");
    }

    errh->message("All tests pass!");
    return 0;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(Json)
EXPORT_ELEMENT(JsonTest)

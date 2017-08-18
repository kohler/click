/*
* ipcounter.cc -- Counter Element utilizing HashTable_RCU
* Neil McGlohon

* Copyright (c) 2017 Cisco Meraki
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
*
*/

#include <click/config.h>
#include "ipcounter.hh"
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/args.hh>
#include <click/handlercall.hh>
CLICK_DECLS


IPCounter::IPCounter(){
	count = 0;

	size_t i = 64;
	this->IPPacketTable_RCU = new HashTable_RCU<String,int>(i);
}

IPCounter::~IPCounter(){
	delete(this->IPPacketTable_RCU);
}

void
IPCounter::reset()
{
	count = 0;
	// IPPacketTable.clear();
}

int
IPCounter::configure(Vector<String> &, ErrorHandler *)
{
	return 0;
}

int
IPCounter::initialize(ErrorHandler *)
{
	reset();
	return 0; 
}

enum{ GROW, GROWLONG, SHRINK };
enum{ TESTALL, STATEDUMP, KEYS, EMPTY, BUCKETS, ITERATOR, INSERT, DEFAULT_VALUE, RESIZE, SWAP, CLEAR, COPYCONST, ASSIGNMENT};


String
IPCounter::read_handler(Element *e, void *thunk)
{
	IPCounter *c = (IPCounter *) e;
	String output = String("");

	bool allTestsPassed = true;
	String testkey = String("test");
	int testvalue = 1;

	bool testall = false;

	switch((intptr_t)thunk)
	{

		case STATEDUMP:
		{
			output += "Total Count: " + String(c->count) + "\n";
			output += "Num Buckets: " + String(c->IPPacketTable_RCU->bucket_count()) + "\n";

			Vector<String> keys = c->IPPacketTable_RCU->get_keys();
			int size = c->IPPacketTable_RCU->size();
			for(int i = 0; i < size; i++)
			{
				output += keys[i] + " " + String(c->IPPacketTable_RCU->unsafe_get(keys[i])) +"\n";
			}
			break;
		}

		case CLEAR:
		{
			output += "clear()\n";

			c->IPPacketTable_RCU->clear();

			c->count = 0;

			output += "\tItems Cleared.";

			break;
		}

		case TESTALL:
		{
			testall = true;
		}

		case KEYS:
		{
			output += "size() and get_keys(): \n";

			Vector<String> keys = c->IPPacketTable_RCU->get_keys();
			int size = c->IPPacketTable_RCU->size();
			output += "\tSize() = " + String(size) + "\n";
			for(int i = 0; i < size; i++)
			{
				output += "\t" + keys[i] + " " + String(c->IPPacketTable_RCU->unsafe_get(keys[i])) +"\n";
			}
			if(!testall)
				break;
		}

		case EMPTY:
		{
			output += "empty(): \n";
			bool isEmpty = c->IPPacketTable_RCU->empty();
			output += "\tempty() = " + String(isEmpty) + "\n";
			if(!testall)
				break;
		}

		case BUCKETS:
		{
			output += "bucket_count() and bucket_size()\n";
			size_t bcount = c->IPPacketTable_RCU->bucket_count();

			size_t minBucketSize = 999999999;
			size_t maxBucketSize = 0;

			for(size_t i = 0; i < bcount; i++)
			{
				size_t bsize = c->IPPacketTable_RCU->bucket_size(i);
				if(bsize < minBucketSize)
					minBucketSize = bsize;
				if(bsize > maxBucketSize)
					maxBucketSize = bsize;
			}


			output += "\t" + String(bcount) + " buckets; min/max size = " + String(minBucketSize) + "/" + String(maxBucketSize) + "\n";

			if(!testall)
				break;
		}

		case ITERATOR:
		{
			output += "Iterator begin() and end()\n";

			size_t iters = 0;

			for(auto it = c->IPPacketTable_RCU->begin(); it.live(); it++)
			{
				iters++;
			}
			output += "\t" + String(iters) + " iterations\n";

			output += "Iterator find(), find_insert(), erase(iterator)\n";

			auto findinserttest = c->IPPacketTable_RCU->find_insert(testkey,testvalue);


			auto findtest = c->IPPacketTable_RCU->find(testkey);

			allTestsPassed = allTestsPassed && findtest.live();
			if(!allTestsPassed)
				output += "FAILED - FIND() or LIVE()";

			c->IPPacketTable_RCU->erase(findtest);

			allTestsPassed = allTestsPassed && (!(c->IPPacketTable_RCU->find(testkey).live()));
			if(!allTestsPassed)
				output += "FAILED - ERASE()";
			
			output += "\n";

			if(!testall)
				break;
		}

		case INSERT:
		{
			output += "insert(), unsafe_get(), erase(key) and count() test\n";

			c->IPPacketTable_RCU->insert(testkey,testvalue);

			int thecount = c->IPPacketTable_RCU->count(testkey);

			output += "\tInserted 'test' == " + String(thecount) +"\n";

			allTestsPassed = allTestsPassed && thecount;
			if(!allTestsPassed)
				output += "FAILED - INSERT or COUNT";

			int getval = c->IPPacketTable_RCU->unsafe_get(testkey);

			output += "\tValue is " + String(getval) + "\n";

			allTestsPassed = allTestsPassed && (getval == testvalue);
			if(!allTestsPassed)
				output += "FAILED - GET()";

			c->IPPacketTable_RCU->erase(testkey);
			if(!testall)
				break;
		}

		case DEFAULT_VALUE:
		{
			output += "Default Value unsafe_get()\n";

			String badkey = "not_in_table";
			int getval = c->IPPacketTable_RCU->unsafe_get(badkey);

			output += "\tValue is " + String(getval) + "\n";

			allTestsPassed = allTestsPassed && (getval == c->IPPacketTable_RCU->get_default_value());
			if(!allTestsPassed)
				output += "FAILED - DEFAULT_VALUE";

			if(!testall)
				break;
		}

		case RESIZE:
		{
			output += "resizing operations\n";

			unsigned long orig = (unsigned long) c->IPPacketTable_RCU->bucket_count();
			unsigned long num1 = 256;
			unsigned long num2 = 32;

			c->IPPacketTable_RCU->grow(num1);
			c->IPPacketTable_RCU->shrink(num2);
			c->IPPacketTable_RCU->grow_long(num1);
			c->IPPacketTable_RCU->resize(orig);

			output += "done.\n";

			output += "\n";
			
			if(!testall)
				break;
		}

		case SWAP:
		{
			output += "swap()\n";

			HashTable_RCU<String,int> newTable = HashTable_RCU<String,int>();

			newTable.insert(testkey,testvalue);


			c->IPPacketTable_RCU->swap(newTable);

			output += "Num Buckets: " + String(c->IPPacketTable_RCU->bucket_count()) + "\n";

			Vector<String> keys = c->IPPacketTable_RCU->get_keys();
			int size = c->IPPacketTable_RCU->size();
			for(int i = 0; i < size; i++)
			{
				output += keys[i] + " " + String(c->IPPacketTable_RCU->unsafe_get(keys[i])) +"\n";
			}

			bool passed = false;
			if(c->IPPacketTable_RCU->find(testkey))
			{
				passed = true;
				c->IPPacketTable_RCU->erase(testkey);
			}

			c->IPPacketTable_RCU->swap(newTable);

			if(passed)
				output += "\tswap passed\n";
			else
				output += "\tswap failed\n";


			if(!testall)
				break;
		}

		case COPYCONST:
		{
			output += "copy const\n";

			HashTable_RCU<String,int> *copy = new HashTable_RCU<String,int>(*(c->IPPacketTable_RCU));

			output += "Num Buckets: " + String(copy->bucket_count()) + "\n";

			Vector<String> keys = copy->get_keys();
			int size = copy->size();
			for(int i = 0; i < size; i++)
			{
				output += keys[i] + " " + String(copy->unsafe_get(keys[i])) +"\n";
			}

			delete(copy);

			if(!testall)
				break;
		}

		case ASSIGNMENT:
		{
			output += "Assignment operator\n";

			HashTable_RCU<String,int> *copy = new HashTable_RCU<String,int>(*(c->IPPacketTable_RCU));

			HashTable_RCU<String,int> *otherCopy = new HashTable_RCU<String,int>();

			*otherCopy = *copy;

			delete(copy);


			output += "Num Buckets: " + String(otherCopy->bucket_count()) + "\n";

			Vector<String> keys = otherCopy->get_keys();
			int size = otherCopy->size();
			for(int i = 0; i < size; i++)
			{
				output += keys[i] + " " + String(otherCopy->unsafe_get(keys[i])) +"\n";
			}

			delete(otherCopy);

			if(!testall)
				break;
		}


		break;

	}

	return output;

}

int
IPCounter::write_handler(const String &arg, Element *e, void *thunk, ErrorHandler *errh)
{
	click_dump_stack();
	(void) arg;
	IPCounter *c = (IPCounter *) e;
	switch((intptr_t)thunk){
		case GROW:
		{
			unsigned long num = 256;
			c->IPPacketTable_RCU->grow(num);
			break;
		}
		case GROWLONG:
		{
			unsigned long num = 256;
			c->IPPacketTable_RCU->grow_long(num);
			break;
		}
		case SHRINK:
		{	
			unsigned long num = 32;
			c->IPPacketTable_RCU->shrink(num);
			break;
		}
		default:
		{
			return errh->error("Invalid Option");
		}
	}
	return 0;
}

Packet *
IPCounter::simple_action(Packet *p)
{
	this->count++;

	WritablePacket *q = p->uniqueify();
	click_ip* ip = q->ip_header();
	in_addr theAddr = ip->ip_src;

	IPAddress ipAddr = IPAddress(theAddr);

	String ipName = ipAddr.unparse();


	int value = this->IPPacketTable_RCU->unsafe_get(ipName);
	if(value == -1)
		value = 0;
	this->IPPacketTable_RCU->set(ipName,(value+1));

	// this->IPPacketTable[ipName]++;

	return p;
}


void
IPCounter::add_handlers()
{
	add_read_handler("alltest", read_handler, TESTALL, 0x0020);
	add_read_handler("stateDump", read_handler, STATEDUMP, 0x0020);
	add_read_handler("testkeys", read_handler, KEYS, 0x0020);
	add_read_handler("testempty", read_handler, EMPTY, 0x0020);
	add_read_handler("testbuckets", read_handler, BUCKETS, 0x0020);
	add_read_handler("testiterator", read_handler, ITERATOR, 0x0020);
	add_read_handler("testinsert", read_handler, INSERT, 0x0020);
	add_read_handler("testdefault", read_handler, DEFAULT_VALUE, 0x0020);
	add_read_handler("testresize", read_handler, RESIZE, 0x0020);
	add_read_handler("testswap", read_handler, SWAP, 0x0020);
	add_read_handler("testcopyconst", read_handler, COPYCONST);
	add_read_handler("testassignment", read_handler, ASSIGNMENT, 0x0020);
	add_read_handler("clear", read_handler, CLEAR, 0x0020);
	add_write_handler("grow", write_handler, GROW, 0x0020);
	add_write_handler("growexclusive", write_handler, GROW);
	add_write_handler("shrinkexclusive", write_handler, SHRINK);
	add_write_handler("growlong", write_handler, GROWLONG,0x0020);
	add_write_handler("shrink", write_handler, SHRINK,0x0020);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(IPCounter)
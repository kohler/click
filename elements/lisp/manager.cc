/*
 * manager.{cc,hh} -- FMCC element deciding VM migrations (FMCCManager)
 * Franck Messaoudi
 *
 * Copyright (c) 2014-2026 Franck Messaoudi
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
#include "manager.hh"
#include <click/args.hh>
#include <click/confparse.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include "lispcommon.hh"
#include <clicknet/udp.h>
#include <clicknet/ip.h>

CLICK_DECLS
/********************************************************************/

FMCCManager::FMCCManager()
{
    _number_mig = 0;
    _rgstmsg_received = _valid_rgstmsg = 0;
    _auto = true;
}

/********************************************************************/

FMCCManager::~FMCCManager()
{
}

/********************************************************************/

int
FMCCManager::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	    .read_mp("DC_SRC", _my_addr1)
	    .read_mp("DC_DST", _my_addr2)
	    .read_mp("VM", _vm_addr)
	    .read_mp("Mbase", ElementCastArg("FMCCDatabase"), _mbase)
	    .read_mp("MIGRATOR", ElementCastArg("FMCCMigrator"), _migrator)
	    .read("AUTO", _auto)
	    .complete()
	< 0)
	return -1;

    return 0;
}

/********************************************************************/
void
FMCCManager::decide(Decision *dec)
{
    dec->decision = true;
    dec->dc_src_rloc = _my_addr1;
    dec->dc_dst_rloc = _my_addr2;
    dec->vm_addr = _vm_addr;
    dec->mask = 32;

    //}
    // }
}

/********************************************************************/

void
FMCCManager::handle_register(Packet *p)
{
    ++_rgstmsg_received;
    click_ip *iph = lisp_ip_header(p);
    click_udp *udph = lisp_udp_header(iph);
    click_lisp_map_register *mrgst_msg = (click_lisp_map_register *)(udph + 1);
    if (p->end_data() < reinterpret_cast<const uint8_t *>(mrgst_msg + 1))
	return;
    // records follow the authentication data
    Record *rec1 = (Record *)(reinterpret_cast<uint8_t *>(mrgst_msg + 1)
	+ ntohs(mrgst_msg->auth_data_len));
    if (p->end_data() < reinterpret_cast<const uint8_t *>(rec1) + 16)
	return;

    IPAddress dc_etr = iph->ip_src;
    uint16_t version = lisp_record_version(rec1);
    int indice;
    bool migr = true; // no record: no migration
    int rslt = _mbase->ms_lookup(dc_etr, version, &indice);
    if (rslt == ERR) {
	click_chatter("%p{element}: authentication error", this);
	return;
    }

    if (rslt == EQL)
	return;

    uint8_t rec_cnt = LISP_CTRL_COUNT(ntohl(mrgst_msg->type_flags));
    ++_valid_rgstmsg;
    _mbase->ms_register(rec_cnt, rec1, indice, &migr, p->end_data());
    if (_auto && migr == false) {
	Decision dec;
	decide(&dec);
	if (dec.decision == true) {
	    _migrator->send_migrate(&dec);
	    ++_number_mig;
	}
    }
    return;
}

/*******************************************************************/

void
FMCCManager::push(int port, Packet *p)
{
    if (!port)
	handle_register(p);

    p->kill();
}

/*******************************************************************/

void
FMCCManager::add_handlers()
{
    add_data_handlers("rgstreceived ", Handler::OP_READ, &_rgstmsg_received);
    add_data_handlers(
	"registers", Handler::OP_READ, &_rgstmsg_received); // Click-style name
    add_data_handlers("rgstvalid ", Handler::OP_READ, &_valid_rgstmsg);
    add_data_handlers("registers_valid", Handler::OP_READ,
	&_valid_rgstmsg); // Click-style name
    add_data_handlers("migrations", Handler::OP_READ, &_number_mig);
    add_write_handler("migrate", write_handler, 0);
}

// "migrate VM ADDR FROM RLOC TO RLOC [MASK LEN]": order a migration now.
// This separates the mechanism (FMCCMigrator) from the decision policy, which
// can then live outside Click (scripts, an MDP controller over ControlSocket).
int
FMCCManager::write_handler(
    const String &str, Element *e, void *, ErrorHandler *errh)
{
    FMCCManager *m = static_cast<FMCCManager *>(e);
    Decision dec;
    dec.decision = true;
    dec.mask = 32;
    Vector<String> w;
    cp_spacevec(str, w);
    bool have_vm = false, have_from = false, have_to = false;
    for (int i = 0; i + 1 < w.size(); i += 2) {
	String k = w[i].upper();
	bool ok;
	if (k == "VM")
	    ok = have_vm = IPAddressArg().parse(w[i + 1], dec.vm_addr, m);
	else if (k == "FROM")
	    ok = have_from = IPAddressArg().parse(w[i + 1], dec.dc_src_rloc, m);
	else if (k == "TO")
	    ok = have_to = IPAddressArg().parse(w[i + 1], dec.dc_dst_rloc, m);
	else if (k == "MASK")
	    ok = IntArg().parse(w[i + 1], dec.mask) && dec.mask <= 32;
	else
	    ok = false;
	if (!ok)
	    return errh->error("bad %<%s %s%>", w[i].c_str(), w[i + 1].c_str());
    }
    if (!have_vm || !have_from || !have_to || (w.size() & 1))
	return errh->error(
	    "syntax: migrate VM ADDR FROM RLOC TO RLOC [MASK LEN]");
    m->_migrator->send_migrate(&dec);
    ++m->_number_mig;
    return 0;
}

/********************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(FMCCManager)
ELEMENT_REQUIRES(FMCCDatabase)
ELEMENT_REQUIRES(FMCCMigrator)
ELEMENT_MT_SAFE(FMCCManager)

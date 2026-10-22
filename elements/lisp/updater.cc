/*
 * updater.{cc,hh} -- xTR element applying FMCC Map-Migrate orders
 * (LISPMigrationUpdater)
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
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/glue.hh>

#include <clicknet/udp.h>
#include <clicknet/ip.h>
#include "lispcommon.hh"
#include "updater.hh"

CLICK_DECLS
/******************************************************/

LISPMigrationUpdater::LISPMigrationUpdater()
{
    _mv_in = _mv_out = _smr_sent = _auth_failures = 0;
    _ttl = 600;
}

LISPMigrationUpdater::~LISPMigrationUpdater()
{
}
/*****************************************************/

int
LISPMigrationUpdater::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
	    .read_mp("SADR", _my_addr)
	    .read_mp("BASE", ElementCastArg("LISPEtrDatabase"), _base)
	    .read_mp("Transmit", ElementCastArg("LISPVMCounter"), _transmit)
	    .read_mp("REGISTER", ElementCastArg("LISPMapRegistrar"), _register)
	    .read("KEY", _key)
	    .complete()
	< 0)
	return -1;
    _hdr.set(_my_addr, IPAddress(), 250); // SMRs: from this xTR, TTL 250

    return 0;
}

/********************************************************************/

/************************in migration**************************/

/***********************************************************************************/

/***********************************************************************************/

Packet *
LISPMigrationUpdater::make_smr(IPAddress itr_addr, uint16_t mask, IPAddress eid)
{
    WritablePacket *q
	= Packet::make(LISPIPHeader::headers + sizeof(click_lisp_map_request));
    if (!q)
	return 0;
    memset(q->data(), 0, q->length()); // Packet::make() does not zero memory
    click_lisp_map_request *mrh = reinterpret_cast<click_lisp_map_request *>(
	q->data() + LISPIPHeader::headers);
    // an SMR is a Map-Request: it goes to the ITR's control port
    _hdr.fill(q, itr_addr, click_random(), htons(LISP_CONTROL_PORT));

    // set lisp message

    mrh->type_flags = htonl(((uint32_t)LISP_MAP_REQUEST << LISP_CTRL_TYPE_SHIFT)
	| LISP_MREQ_F_S | 1); // SMR, IRC = 0, one record
    mrh->nonce1 = click_random();
    mrh->nonce2 = click_random();

    mrh->source_eid_afi = htons(LISP_AFI_IP);
    map_rqst_set_source_eid(mrh, eid);

    mrh->itr_rloc_afi = htons(LISP_AFI_IP);
    mrh->itr_rloc = _my_addr;

    mrh->rec_reserved = 0;
    mrh->rec_eid_mask_len = mask;
    mrh->rec_eid_afi = htons(LISP_AFI_IP);
    mrh->rec_eid_prefix = eid;

    LISPIPHeader::finish_udp(q); // UDP checksum last
    _smr_sent++;
    return q;
}

/***********************************************************************************/

void
LISPMigrationUpdater::send_smrs(
    uint16_t mask, IPAddress eid, const Vector<IPAddress> &itrs)
{
    for (int i = 0; i < itrs.size(); ++i) {
	if (Packet *p = make_smr(itrs[i], mask, eid))
	    output(0).push(p);
    }
}

/***********************************************************************************/

/***********************************************************************************/

void
LISPMigrationUpdater::push(int port, Packet *p)
{
    if (!port) {
	_mv_in++;
	click_ip *iph = lisp_ip_header(p);
	click_udp *udph = lisp_udp_header(iph);
	click_lisp_map_migrate *mig = (click_lisp_map_migrate *)(udph + 1);
	const uint8_t *msg = reinterpret_cast<const uint8_t *>(mig);
	size_t len = p->end_data() > msg ? p->end_data() - msg : 0;
	uint16_t aut_len = len >= sizeof(click_lisp_map_migrate)
	    ? ntohs(mig->auth_data_len)
	    : 0;
	if (len < sizeof(click_lisp_map_migrate) + aut_len
		+ sizeof(click_lisp_map_migrate_body)) {
	    click_chatter("%p{element}: truncated Map-Migrate", this);
	    p->kill();
	    return;
	}
	// with KEY, only Map-Migrate messages signed by the FMCC are obeyed
	if (_key
	    && (ntohs(mig->key_id) != LISP_KEY_ID_HMAC_SHA_256_128
		|| aut_len != LISP_AUTH_LEN_HMAC_SHA_256_128
		|| !lisp_auth_verify(
		    msg, len, sizeof(click_lisp_map_migrate), _key))) {
	    ++_auth_failures;
	    click_chatter(
		"%p{element}: Map-Migrate rejected (bad authentication)", this);
	    p->kill();
	    return;
	}
	const click_lisp_map_migrate_body *vm_mess
	    = reinterpret_cast<const click_lisp_map_migrate_body *>(
		msg + sizeof(click_lisp_map_migrate) + aut_len);

	IPAddress _vm_adr = vm_mess->vm_addr;
	EtherAddress _vm_mac = EtherAddress();
	_ip = vm_mess->ddc_addr;
	if (vm_mess->ddc_addr == _my_addr) {
	    _transmit->insert(_vm_mac, _vm_adr);
	    _base->insert(_vm_adr, 32,
		_ttl); // migrated VM: host EID, same /32 as the SMR

	} else {
	    uint16_t mask = 32;
	    ++_mv_out;
	    _transmit->remove_vm(_vm_adr);
	    Vector<IPAddress> itrs
		= _base->remove_host(_vm_adr); // copy of the ITR list
	    send_smrs(mask, _vm_adr, itrs);
	}

	_register->send_register();
    }
    p->kill();
}

/***********************************************************************************/

void
LISPMigrationUpdater::add_handlers()
{
    add_data_handlers("in machines ", Handler::OP_READ, &_mv_in);
    add_data_handlers("vms_in", Handler::OP_READ, &_mv_in); // Click-style name
    add_data_handlers("out machines ", Handler::OP_READ, &_mv_out);
    add_data_handlers(
	"vms_out", Handler::OP_READ, &_mv_out); // Click-style name
    add_data_handlers("sent smr ", Handler::OP_READ, &_smr_sent);
    add_data_handlers("smrs", Handler::OP_READ, &_smr_sent); // Click-style name
    add_data_handlers("auth_failures", Handler::OP_READ, &_auth_failures);
    add_data_handlers("adresse ", Handler::OP_READ, &_ip);
    add_data_handlers("last_dc", Handler::OP_READ, &_ip); // Click-style name
}

/***********************************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(LISPMigrationUpdater)
ELEMENT_REQUIRES(LISPMapRegistrar)
ELEMENT_REQUIRES(LISPVMCounter)
ELEMENT_REQUIRES(LISPEtrDatabase LISPCommon)
ELEMENT_MT_SAFE(LISPMigrationUpdater)

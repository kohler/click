/*
 * migrator.{cc,hh} -- FMCC element sending Map-Migrate orders (FMCCMigrator)
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
#include "migrator.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/args.hh>
#include <click/bitvector.hh>
#include <click/straccum.hh>
#include <click/router.hh>
#include <click/ipaddress.hh>
#include <clicknet/udp.h>
#include <clicknet/ip.h>
#include "lispcommon.hh"
CLICK_DECLS

/**************************************************************************************/

FMCCMigrator::FMCCMigrator()
{
    _notif_received = _migration_sent = 0;
}

/**************************************************************************************/

FMCCMigrator::~FMCCMigrator()
{
}

/**************************************************************************************/

int
FMCCMigrator::configure(Vector<String> &conf, ErrorHandler *errh)
{
    Args args(conf, this, errh);
    args.read_mp("Mbase", ElementCastArg("FMCCDatabase"), _mbase);
    _hdr.read_args(args); // PROTO, SRC, DST and the IP keywords
    args.read("KEY", _key);
    if (args.complete() < 0 || _hdr.finish_configure(this, errh) < 0)
	return -1;

    return 0;
}

/**************************************************************************************/

int
FMCCMigrator::initialize(ErrorHandler *)
{
    return 0;
}

/**************************************************************************************/

/**************************************************************************************/

void
FMCCMigrator::send_migrate(Decision *dec)
{
    uint16_t aut_len = _key ? LISP_AUTH_LEN_HMAC_SHA_256_128 : 0;
    WritablePacket *q
	= Packet::make(LISPIPHeader::headers + sizeof(click_lisp_map_migrate)
	    + aut_len + sizeof(click_lisp_map_migrate_body));
    if (!q) {
	click_chatter("%p{element}: can't make Map-Migrate packet", this);
	return;
    }
    memset(q->data(), 0, q->length()); // Packet::make() does not zero memory
    click_lisp_map_migrate *mig_vm = reinterpret_cast<click_lisp_map_migrate *>(
	q->data() + LISPIPHeader::headers);
    // IP/UDP: to the source data center's xTR (DST, or DST_ANNO)
    _hdr.fill(q, IPAddress(), click_random(), htons(LISP_CONTROL_PORT));

    // set lisp message:
    mig_vm->type_flags
	= htonl(((uint32_t)LISP_SHARED_EXTENSION << LISP_CTRL_TYPE_SHIFT)
	    | (LISP_FMCC_MIGRATE_SUBTYPE << LISP_CTRL_SUBTYPE_SHIFT)
	    | 1); // one record
    mig_vm->nonce1 = click_random();
    mig_vm->nonce2 = click_random();
    mig_vm->key_id
	= htons(_key ? LISP_KEY_ID_HMAC_SHA_256_128 : LISP_KEY_ID_NONE);
    mig_vm->auth_data_len = htons(aut_len);
    click_lisp_map_migrate_body *body
	= reinterpret_cast<click_lisp_map_migrate_body *>(
	    reinterpret_cast<uint8_t *>(mig_vm + 1) + aut_len);
    body->mask = htons(dec->mask);
    body->eid_afi = htons(LISP_AFI_IP);
    body->vm_addr = dec->vm_addr;
    body->unused = 0;
    body->loc_afi = htons(LISP_AFI_IP);
    body->ddc_addr = dec->dc_dst_rloc;
    if (_key)
	lisp_auth_sign(reinterpret_cast<uint8_t *>(mig_vm),
	    q->end_data() - reinterpret_cast<uint8_t *>(mig_vm),
	    sizeof(click_lisp_map_migrate), _key);

    // update the Table of the FMCC
    bool mg = true;
    LISPIPHeader::finish_udp(q); // UDP checksum last
    ++_migration_sent;
    output(0).push(q);
    _mbase->update_with_mig(
	mg, dec->vm_addr, dec->dc_src_rloc, dec->dc_dst_rloc);
}

/**************************************************************************************/

void
FMCCMigrator::handle_notify(Packet *p)
{
    ++_notif_received;
    click_ip *iph = lisp_ip_header(p);
    click_udp *udph = lisp_udp_header(iph);
    (void)udph;
    // the packet is freed by push()
    return;
}

/**************************************************************************************/

void
FMCCMigrator::push(int port, Packet *p)
{
    if (!port)
	handle_notify(p);
    p->kill();
}

/**************************************************************************************/

void
FMCCMigrator::add_handlers()
{
    add_data_handlers("Map_notify count", Handler::OP_READ, &_notif_received);
    add_data_handlers(
	"notifies", Handler::OP_READ, &_notif_received); // Click-style name
    add_data_handlers(
	"Map_Migration count", Handler::OP_READ, &_migration_sent);
    add_data_handlers(
	"migrations", Handler::OP_READ, &_migration_sent); // Click-style name
}

/**************************************************************************************/

CLICK_ENDDECLS
EXPORT_ELEMENT(FMCCMigrator)
ELEMENT_REQUIRES(FMCCDatabase LISPCommon)
ELEMENT_MT_SAFE(FMCCMigrator)

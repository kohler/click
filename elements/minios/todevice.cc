/*
 *          ClickOS
 *
 *   file: todevice.cc
 *
 *          NEC Europe Ltd. PROPRIETARY INFORMATION
 *
 * This software is supplied under the terms of a license agreement
 * or nondisclosure agreement with NEC Europe Ltd. and may not be
 * copied or disclosed except in accordance with the terms of that
 * agreement. The software and its source code contain valuable trade
 * secrets and confidential information which have to be maintained in
 * confidence.
 * Any unauthorized publication, transfer to third parties or duplication
 * of the object or source code - either totally or in part – is
 * prohibited.
 *
 *      Copyright (c) 2014 NEC Europe Ltd. All Rights Reserved.
 *
 * Authors: Joao Martins <joao.martins@neclab.eu>
 *          Filipe Manco <filipe.manco@neclab.eu>
 *
 * NEC Europe Ltd. DISCLAIMS ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE AND THE WARRANTY AGAINST LATENT
 * DEFECTS, WITH RESPECT TO THE PROGRAM AND THE ACCOMPANYING
 * DOCUMENTATION.
 *
 * No Liability For Consequential Damages IN NO EVENT SHALL NEC Europe
 * Ltd., NEC Corporation OR ANY OF ITS SUBSIDIARIES BE LIABLE FOR ANY
 * DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION, DAMAGES FOR LOSS
 * OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF INFORMATION, OR
 * OTHER PECUNIARY LOSS AND INDIRECT, CONSEQUENTIAL, INCIDENTAL,
 * ECONOMIC OR PUNITIVE DAMAGES) ARISING OUT OF THE USE OF OR INABILITY
 * TO USE THIS PROGRAM, EVEN IF NEC Europe Ltd. HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 *     THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */
#include "todevice.hh"

#include <click/args.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/task.hh>
#include <stdio.h>

extern "C" {
#include <netfront.h>
}


CLICK_DECLS

ToDevice::ToDevice()
	: _vifid(0), _burstsize(1), _count(0), _task(this), _dev(NULL)
{
}

ToDevice::~ToDevice()
{
}

int
ToDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (Args(conf, this, errh)
			.read_p("DEVID", IntArg(), _vifid)
			.read_p("BURST", IntArg(), _burstsize)
			.complete() < 0)
		return -1;

	if (_vifid < 0)
		return errh->error("Interface id must be >= 0");

    if (_burstsize < 1)
        return errh->error("Burst size must be >= 1");

	return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
	char nodename[256];
	snprintf(nodename, sizeof(nodename), "device/vif/%d", _vifid);

	_dev = init_netfront(nodename, NULL, NULL, NULL);
	if (!_dev)
		return errh->error("Unable to initialize netfront for device %d (%s)", _vifid, nodename);

    /* TODO: should probably move this to configure */
	void *&used = router()->force_attachment("device_writer_" + String(nodename));
	if (used)
		return errh->error("Duplicate writer for device `%d'", _vifid);

	used = this;

	if (input_is_pull(0)) {
		ScheduleInfo::initialize_task(this, &_task, errh);
	}

	return 0;
}

void
ToDevice::cleanup(CleanupStage stage)
{
	shutdown_netfront(_dev);
	_dev = NULL;
}

void
ToDevice::push(int port, Packet *p)
{
    netfront_xmit(_dev, (unsigned char*) p->data(), p->length());
    checked_output_push(0, p);
}

bool
ToDevice::run_task(Task *)
{
	int c;
	Packet* p;

	for (c = 0; c < _burstsize; c++) {
		p = input(0).pull();
		if (!p)
			break;

		netfront_xmit(_dev, (unsigned char*) p->data(), p->length());
		checked_output_push(0, p);
	}

	/* TODO: should only fast_reschedule when there is more work to do? */
	_task.fast_reschedule();

	return c > 0;
}

void
ToDevice::add_handlers()
{
    add_read_handler("count", read_handler, 0);
    add_write_handler("reset_counts", reset_counts, 0, Handler::BUTTON);
}

String
ToDevice::read_handler(Element* e, void *thunk)
{
    return String(static_cast<ToDevice*>(e)->_count);
}

int
ToDevice::reset_counts(const String &, Element *e, void *, ErrorHandler *)
{
    static_cast<ToDevice*>(e)->_count = 0;
    return 0;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(ToDevice)

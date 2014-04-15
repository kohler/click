/*
 *          ClickOS
 *
 *   file: fromdevice.hh
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
#ifndef CLICK_FROMDEVICE_HH
#define CLICK_FROMDEVICE_HH

#include <click/config.h>
#include <click/deque.hh>
#include <click/element.hh>
#include <click/error.hh>
#include <click/task.hh>

extern "C" {
#include <netfront.h>
}

CLICK_DECLS

/*
=title FromDevice.minios

=c

FromDevice(DEVID)

=s netdevices

reads packets from network device (mini-os)

=d

This manual page describes the mini-os version of the FromDevice
element. For the Linux kernel module element, read the FromDevice(n) manual
page.

Reads packets from the kernel that were received on the network controller
named DEVID. This element enqueues packet in the interrupt context to be afterwards
handled in the router.

=back

=e

  FromDevice(0) -> ...

=n

=h count read-only

Returns the number of packets read by the device.

=h reset_counts write-only

Resets "count" to zero.

=a ToDevice.minios, ToDevice.u, FromDump, ToDump, KernelFilter, FromDevice(n) */

class FromDevice : public Element {
public:
    FromDevice();
    ~FromDevice();

    const char *class_name() const { return "FromDevice"; }
    const char *port_count() const { return "0/1"; }
    const char *processing() const { return "/h"; }
    int configure_phase() const { return CONFIGURE_PHASE_FIRST; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    void add_handlers();

    bool run_task(Task *);

private:
    int _vifid;
    int _count;
    Task _task;
    Deque<Packet*> _deque;
    struct netfront_dev* _dev;

    static void rx_handler(unsigned char* data, int len, void* e);
    static void pkt_destructor(unsigned char* data, long unsigned int lenght, void* arg) {};

    static String read_handler(Element* e, void *thunk);
    static int reset_counts(const String &, Element *e, void *, ErrorHandler *);
};

CLICK_ENDDECLS
#endif

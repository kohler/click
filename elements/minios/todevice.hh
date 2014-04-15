/*
 *          ClickOS
 *
 *   file: todevice.hh
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
#ifndef CLICK_TODEVICE_HH
#define CLICK_TODEVICE_HH

#include <click/config.h>
#include <click/element.hh>
#include <click/error.hh>
#include <click/task.hh>

extern "C" {
#include <netfront.h>
}

CLICK_DECLS

/*
 * =title ToDevice.minios
 * =c
 * ToDevice(DEVID)
 * =s netdevices
 * sends packets to network device (mini-os)
 * =d
 *
 * This manual page describes the mini-os version of the ToDevice element.
 * For the Linux kernel module element, read the ToDevice(n) manual page.
 *
 * Pushes packets to a named device or 
 * Pulls packets and sends them out the named device.
 *
 * =back
 *
 * This element is only available at mini-os.
 *
 * =n
 *
 * Packets sent via ToDevice should already have a link-level
 * header prepended. This means that ARP processing,
 * for example, must already have been done.
 *
 * FromDevice receives packets sent by a ToDevice element for the same
 * device.
 *
 * Packets that are written successfully are sent on output 0, if it exists.
 * =a
 * ToDevice.minios, FromDevice.u, FromDump, ToDump, KernelTun, ToDevice(n) */

class ToDevice : public Element {
public:
    ToDevice();
    ~ToDevice();

    const char *class_name() const { return "ToDevice"; }
    const char *port_count() const { return "1/0-1"; }
    const char *processing() const { return "a/h"; }
    int configure_phase() const { return CONFIGURE_PHASE_FIRST; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
    void cleanup(CleanupStage);

    void add_handlers();

    bool run_task(Task *);
    void push(int, Packet *p);

private:
    int _vifid;
    int _burstsize;
    int _count;
    Task _task;
    struct netfront_dev* _dev;

    static String read_handler(Element* e, void *thunk);
    static int reset_counts(const String &, Element *e, void *, ErrorHandler *);
};

CLICK_ENDDECLS
#endif

#ifndef CLICK_FTPPORTMAPPER_HH
#define CLICK_FTPPORTMAPPER_HH
#include "elements/tcpudp/tcprewriter.hh"
CLICK_DECLS

/*
 * =c
 * FTPPortMapper(CONTROL_REWRITER, DATA_REWRITER, DATA_REWRITER_INPUT)
 * =s nat
 * manipulates IPRewriter for FTP
 * =d
 *
 * Expects FTP control packets. Watches packets for PORT commands and installs
 * corresponding mappings into the specified IPRewriter. This makes FTP
 * possible through a NAT-like IPRewriter setup.
 *
 * CONTROL_REWRITER and DATA_REWRITER are the names of IPRewriter-like
 * elements. CONTROL_REWRITER must be a TCPRewriter element, through which the
 * FTP control packets are passed. Packets from FTPPortMapper must pass
 * downstream through CONTROL_REWRITER. DATA_REWRITER can be any
 * IPRewriter-like element; packets from the FTP data port must pass through
 * DATA_REWRITER. CONTROL_REWRITER and DATA_REWRITER might be the same
 * element.
 *
 * DATA_REWRITER_INPUT is a valid input port number for DATA_REWRITER.  When a
 * control connection opens a new data port, a mapping is installed in
 * DATA_REWRITER as if a data packet had arrived on DATA_REWRITER_INPUT.
 * Usually DATA_REWRITER_INPUT refers to a pattern specification; see
 * L<IPRewriter> for more information.
 *
 * In summary: Assume that an FTP packet with source address and port
 * 1.0.0.2:6587 and destination address and port 2.0.0.2:21 contains a command
 * `PORT 1,0,0,2,3,9' (that is, 1.0.0.2:777). Furthermore assume that the
 * pattern corresponding to DATA_REWRITER_INPUT is `1.0.0.1 9000-14000 -
 * -'. Then FTPPortMapper performs the following actions:
 *
 * =over 3
 *
 * =item *
 *
 * Creates a new mapping using the DATA_REWRITER_INPUT pattern. Say it returns
 * 9000 as the new source port.  This installs the following mappings into the
 * rewriter:
 *
 * =over 3
 *
 * =item 1.
 * (1.0.0.2, 777, 2.0.0.2, 20) => (1.0.0.1, 9000, 2.0.0.2, 20)
 *
 * =item 2.
 * (2.0.0.2, 20, 1.0.0.1, 9000) => (2.0.0.2, 20, 1.0.0.2, 777)
 *
 * =back
 *
 * =item *
 *
 * Rewrites the PORT command to `PORT 1,0,0,1,35,40' (that is,
 * 1.0.0.1:9000).
 *
 * =item *
 * Updates the packet's IP and TCP checksums.
 *
 * =item *
 * Updates the downstream CONTROL_REWRITER to reflect the change in
 * sequence numbers introduced by the new PORT command. (The modified packet
 * containing the new PORT command will likely have a different length than
 * the original packet, so some sequence number patching is required.)
 *
 * =item *
 * Does I<not> change the control packet header's addresses or port numbers.
 *
 * =back
 *
 * For a PORT command to be recognized, it must be completely contained within
 * one packet, and it must be the first command in the packet. This is usually
 * the case. Also, the destination FTP data port is always assumed to be one
 * less than the destination FTP control port, which is read as the packet's
 * destination port number. This is also usually the case.
 *
 * =a
 * IPRewriter, TCPRewriter, IPRewriterPatterns
 *
 * L<RFC 959, File Transfer Protocol (FTP)|http://www.ietf.org/rfc/rfc0959.txt>
 */

class FTPPortMapper : public Element { public:

    FTPPortMapper() CLICK_COLD;
    ~FTPPortMapper() CLICK_COLD;

    const char *class_name() const	{ return "FTPPortMapper"; }
    const char *port_count() const	{ return PORTS_1_1; }

    int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
    bool can_live_reconfigure() const	{ return true; }
    int initialize(ErrorHandler *) CLICK_COLD;

    Packet *simple_action(Packet *);

  private:

    TCPRewriter *_control_rewriter;
    IPRewriterBase *_data_rewriter;
    int _data_rewriter_input;

};

CLICK_ENDDECLS
#endif

#ifndef FTPPORTMAPPER_HH
#define FTPPORTMAPPER_HH
#include "elements/ip/tcprewriter.hh"

/*
 * =c
 * FTPPortMapper(CONTROL_REWRITER, DATA_REWRITER, PATTERN FOUTPUT ROUTPUT)
 * =s TCP
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
 * PATTERN is a pattern specification -- either a pattern name (see
 * L<IPRewriterPatterns>) or a `SADDR SPORT DADDR DPORT' quadruple. See
 * L<IPRewriter> for more information. The address and port specified in the
 * PORT command correspond to `SADDR' and `SPORT' in the pattern. If a new
 * SADDR and SPORT are chosen, then the PORT command is rewritten to reflect
 * the new SADDR and SPORT.
 *
 * In summary: Assume that an FTP packet with source address and port
 * 1.0.0.2:6587 and destination address and port 2.0.0.2:21 contains a command
 * `PORT 1,0,0,2,3,9' (that is, 1.0.0.2:777). Furthermore assume that the
 * PATTERN is `1.0.0.1 9000-14000 - -'. Then FTPPortMapper performs the
 * following actions:
 *
 * =over 3
 *
 * =item *
 *
 * Creates a new mapping using the PATTERN. Say it returns 9000 as the new
 * source port.
 *
 * =item *
 *
 * Installs the following mappings into the rewriter:
 *
 * =over 3
 *
 * =item 1.
 * (1.0.0.2, 777, 2.0.0.2, 20) => (1.0.0.1, 9000, 2.0.0.2, 20) with output
 * port FOUTPUT.
 *
 * =item 2.
 * (2.0.0.2, 20, 1.0.0.1, 9000) => (2.0.0.2, 20, 1.0.0.2, 777) with output
 * port ROUTPUT.
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

class FTPPortMapper : public Element {

  TCPRewriter *_control_rewriter;
  IPRw *_data_rewriter;
  IPRw::Pattern *_pattern;
  int _forward_port;
  int _reverse_port;
  
 public:

  FTPPortMapper();
  ~FTPPortMapper();

  const char *class_name() const	{ return "FTPPortMapper"; }
  
  FTPPortMapper *clone() const		{ return new FTPPortMapper; }
  int configure(const Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void uninitialize();
  
  Packet *simple_action(Packet *);
  
};

#endif

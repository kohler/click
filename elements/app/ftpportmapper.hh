#ifndef FTPPORTMAPPER_HH
#define FTPPORTMAPPER_HH
#include "elements/ip/iprewriter.hh"

/*
 * =c
 * FTPPortMapper(REWRITER_ELEMENT, PATTERN FOUTPUT ROUTPUT)
 * =d
 *
 * Expects FTP control packets. Watches packets for PORT commands and installs
 * corresponding mappings into the specified IPRewriter. This makes FTP
 * possible through a NAT-like IPRewriter setup.
 *
 * REWRITER_ELEMENT is the name of an IPRewriter element. PATTERN is a pattern
 * specification -- either a pattern name (see IPRewriterPatterns(n)) or a
 * `SADDR SPORT DADDR DPORT' quadruple. See IPRewriter(n) for more
 * information. The address and port specified in the PORT command correspond
 * to `SADDR' and `SPORT' in the pattern. If a new SADDR and SPORT are chosen,
 * then the PORT command is rewritten to reflect the new SADDR and SPORT.
 *
 * In summary: Assume that an FTP packet with source address and port
 * 1.0.0.2:6587 and destination address and port 2.0.0.2:21 contains a command
 * `PORT 1,0,0,2,3,9' (that is, 1.0.0.2:777). Furthermore assume that the
 * PATTERN is `1.0.0.1 9000-14000 - -'. Then FTPPortMapper performs the
 * following actions:
 *
 * (*) Creates a new mapping using the PATTERN. Say it returns 9000 as the
 * new source port.
 *
 * (*) Installs the following mappings into the rewriter:
 *
 * (  1.) (1.0.0.2, 777, 2.0.0.2, 20) => (1.0.0.1, 9000, 2.0.0.2, 20) with output
 * port FOUTPUT.
 *
 * (  2.) (2.0.0.2, 20, 1.0.0.1, 9000) => (2.0.0.2, 20, 1.0.0.2, 777) with output
 * port ROUTPUT.
 *
 * (*) Rewrites the PORT command to `PORT 1,0,0,1,35,40' (that is,
 * 1.0.0.1:9000).
 *
 * (*) Updates the packet's IP and TCP checksums.
 *
 * (*) Does <i>not</i> rewrite the packet header's addresses or port numbers.
 *
 * For a PORT command to be recognized, it must be completely contained within
 * one packet, and it must be the first command in the packet. This is usually
 * the case. Also, the destination FTP data port is always assumed to be one
 * less than the destination FTP control port, which is read as the packet's
 * destination port number. This is also usually the case.
 *
 * =a IPRewriter IPRewriterPatterns */

class FTPPortMapper : public Element {

  IPRewriter *_rewriter;
  IPRewriter::Pattern *_pattern;
  int _forward_port;
  int _reverse_port;
  
 public:

  FTPPortMapper();

  const char *class_name() const	{ return "FTPPortMapper"; }
  
  FTPPortMapper *clone() const		{ return new FTPPortMapper; }
  int configure(const Vector<String> &, ErrorHandler *);
  void uninitialize();
  
  Packet *simple_action(Packet *);
  
};

#endif

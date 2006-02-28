#include <click/config.h>
#include <click/confparse.hh>
#include <click/package.hh>
#include "simpacketanalyzer.hh"
#include <click/packet_anno.hh>
CLICK_DECLS


SimPacketAnalyzer::SimPacketAnalyzer()
{
}


SimPacketAnalyzer::~SimPacketAnalyzer()
{
}


CLICK_ENDDECLS
ELEMENT_REQUIRES(ns)
ELEMENT_PROVIDES(SimPacketAnalyzer)

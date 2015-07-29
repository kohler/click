elementclass srcr_ett {
  $srcr_ip, $srcr_nm, $wireless_mac, $debug, $probes |


arp :: ARPTable();
lt :: LinkTable(IP $srcr_ip);


es :: ETTStat(ETHTYPE 0x0641, 
	      ETH $wireless_mac, 
	      IP $srcr_ip, 
	      PERIOD 30000,
	      TAU 300000,
	      ARP arp,
	      PROBES $probes,
	      ETT metric,
	      RT rates);

metric :: ETTMetric(LT lt);

forwarder :: SRForwarder(ETHTYPE 0x0643, 
			      IP $srcr_ip, 
			      ETH $wireless_mac, 
			      ARP arp, 
			      LT lt);

querier :: SRQuerier(ETH $wireless_mac, 
		     SR forwarder,
		     LT lt, 
		     ROUTE_DAMPENING true,
		     TIME_BEFORE_SWITCH 5,
		     DEBUG $debug);

query_forwarder :: MetricFlood(ETHTYPE 0x0644,
			       IP $srcr_ip, 
			       ETH $wireless_mac, 
			       LT lt, 
			       ARP arp,
			       DEBUG $debug);

query_responder :: SRQueryResponder(ETHTYPE 0x0645,
				    IP $srcr_ip, 
				    ETH $wireless_mac, 
				    LT lt, 
				    ARP arp,
				    DEBUG $debug);


input [0]
-> ncl :: Classifier(
	12/0643 , //data
	12/0644 , //queries
	12/0645 , //replies
	12/0641 , //ett probes
	);

ncl[0] -> CheckSRHeader() /*-> PrintSR(--from_other_$srcr_ip)*/ -> [0] forwarder;
ncl[1] -> CheckSRHeader() /*-> PrintSR(--recv_query_$srcr_ip)*/ -> query_forwarder;
ncl[2] -> CheckSRHeader() -> query_responder;
ncl[3] -> es;


query_responder -> SetSRChecksum -> [0] output;
query_forwarder -> SetSRChecksum /*-> PrintSR(--frwd_query_$srcr_ip)*/ -> [0] output;
query_forwarder [1] -> query_responder;
querier [1] -> [1] query_forwarder;

es -> SetTimestamp() -> [1] output;

forwarder[0] 
/*-> PrintSR(--frwd_other_$srcr_ip)*/
-> data_ck :: SetSRChecksum() 
-> [2] output;

forwarder[1] //ip packets to me
-> StripSRHeader()
-> CheckIPHeader(CHECKSUM false)
-> counter_outgoing :: IPAddressCounter(USE_SRC true)
/*-> IPPrint(--to___me____$srcr_ip)*/
-> [3] output;


input [1] 
/*-> IPPrint(--from_me____$srcr_ip)*/
-> SetTimestamp()
-> counter_incoming :: IPAddressCounter(USE_DST true)
-> querier
/*-> PrintSR(--to___other_$srcr_ip)*/
-> data_ck
 
}

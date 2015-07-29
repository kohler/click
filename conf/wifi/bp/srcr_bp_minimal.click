elementclass srcr_ett {
  $srcr_ip, $srcr_nm, $wireless_mac, $debug, $probes, $enhanced |


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

bps :: BPStat(ETHTYPE 0x0642,
              ETH $wireless_mac,
              IP $srcr_ip,
              PERIOD 1000,
              BPDATA bpdata,
              DQ data_q,
              LT lt,
              RT rates,
              ENHANCED true); //Nodes broadcast distances, to avoid circles. If BP is not Enhanced, these distances are not included in the metric.

metric :: ETTMetric(LT lt);

forwarder :: BPForwarder(ETHTYPE 0x0643, 
			      IP $srcr_ip, 
			      ETH $wireless_mac, 
			      ARP arp, 
			      LT lt,
			      DQ data_q);

querier :: BPQuerier(ETH $wireless_mac, 
                     IP $srcr_ip,
		     SR forwarder,
		     LT lt, 
		     DQ data_q, 
		     ROUTE_DAMPENING true,
		     TIME_BEFORE_SWITCH 1, //5,
		     DEBUG $debug);

query_forwarder :: BPMetricFlood(ETHTYPE 0x0644,
			       IP $srcr_ip, 
			       ETH $wireless_mac, 
			       LT lt, 
			       ARP arp,
			       DEBUG $debug);

query_responder :: BPQueryResponder(ETHTYPE 0x0645,
				    IP $srcr_ip, 
				    ETH $wireless_mac, 
				    LT lt, 
				    ARP arp,
				    DQ data_q,
				    DEBUG $debug);


input [0]
-> ncl :: Classifier(
	12/0643 , //data
	12/0644 , //queries
	12/0645 , //replies
	12/0641 , //ett probes
	12/0642 , //bp probes
	);
  
ncl[0] -> CheckSRHeader() /*-> PrintSR(--from_other_$srcr_ip)*/ -> [0] forwarder;
ncl[1] -> CheckSRHeader() /*-> PrintSR(--recv_query_$srcr_ip)*/ -> query_forwarder;
ncl[2] -> CheckSRHeader() -> query_responder;
ncl[3] -> es;
ncl[4] -> bps;


query_responder -> SetSRChecksum -> [0] output;
query_forwarder -> SetSRChecksum /*-> PrintSR(--frwd_query_$srcr_ip)*/ -> [0] output;
query_forwarder [1] -> query_responder;
querier [1] -> [1] query_forwarder;

es  -> SetTimestamp() -> [1] output;
bps -> SetTimestamp() -> [4] output;

forwarder [0] 
/*-> PrintSR(--frwd_other_$srcr_ip)*/ 
-> [1] querier;

forwarder [1]
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
-> SetSRChecksum
-> [2] output;

}

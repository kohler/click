
elementclass srcr_etx {
  $srcr_ip, $srcr_nm, $wireless_mac, $gateway, $probes|


arp :: ARPTable();
lt :: LinkTable(IP $srcr_ip);


gw :: GatewaySelector(ETHTYPE 0x0a01,
			   IP $srcr_ip,
			   ETH $wireless_mac,
			   LT lt,
			   ARP arp,
			   PERIOD 15,
			   GW $gateway);


gw -> SetSRChecksum -> [0] output;

set_gw :: SetGateway(SEL gw);


es :: ETTStat(ETHTYPE 0x0a04, 
	      ETH $wireless_mac, 
	      IP $srcr_ip, 
	      PERIOD 30000,
	      TAU 300000,
	      ARP arp,
	      PROBES $probes,
	      ETX metric,
	      RT rates);


metric :: TXCountMetric(LT lt);


forwarder :: SRForwarder(ETHTYPE 0x0a02, 
			      IP $srcr_ip, 
			      ETH $wireless_mac, 
			      ARP arp, 
			      LT lt);


querier :: SRQuerier(ETH $wireless_mac, 
		     SR forwarder,
		     LT lt, 
		     ROUTE_DAMPENING true,
		     TIME_BEFORE_SWITCH 5,
		     DEBUG true);

 query_forwarder :: MetricFlood(ETHTYPE 0x0a05,
				IP $srcr_ip, 
				ETH $wireless_mac, 
				LT lt, 
				ARP arp,
				DEBUG true);

 query_responder :: SRQueryResponder(ETHTYPE 0x0a03,
					 IP $srcr_ip, 
					 ETH $wireless_mac, 
					 LT lt, 
					 ARP arp,
					 DEBUG true);


query_responder -> SetSRChecksum -> [0] output;
query_forwarder -> SetSRChecksum -> [0] output;
query_forwarder [1] -> query_responder;

data_ck :: SetSRChecksum() 

input [1] 
-> SetTimestamp()
-> counter_incoming :: IPAddressCounter(USE_DST true)
-> host_cl :: IPClassifier(dst net $srcr_ip mask $srcr_nm,
				-)
-> querier
-> data_ck;


host_cl [1] -> [0] set_gw [0] -> querier;

forwarder[0] 
  -> dt ::DecIPTTL
  -> data_ck
  -> [2] output;


dt[1] 
-> Print(ttl-error) 
-> ICMPError($srcr_ip, timeexceeded, 0) 
-> querier;


// queries
querier [1] -> [1] query_forwarder;
es -> SetTimestamp() -> [1] output;


forwarder[1] //ip packets to me
  -> StripSRHeader()
  -> CheckIPHeader()
  -> from_gw_cl :: IPClassifier(src net $srcr_ip mask $srcr_nm,
				-)
  -> counter_outgoing :: IPAddressCounter(USE_SRC true)
  -> [3] output;

from_gw_cl [1] -> [1] set_gw [1] -> [3] output;

 input [0]
   -> ncl :: Classifier(
			12/0a02 , //srcr_forwarder
			12/0a05 , //srcr
			12/0a03 , 
			12/0a04 , //srcr_es
			12/0a01 , //srcr_gw
			);
 
 
 ncl[0] -> CheckSRHeader() -> [0] forwarder;
 ncl[1] -> CheckSRHeader() -> query_forwarder;
 ncl[2] -> CheckSRHeader() -> query_responder; 
 ncl[3] -> es;
 ncl[4] -> CheckSRHeader() -> gw;
 
}





elementclass srcr_ett {
  $srcr_ip, $srcr_nm, $wireless_mac, $gateway, $probes|


arp :: ARPTable();
lt :: LinkTable(IP $srcr_ip);


gw :: GatewaySelector(ETHTYPE 0x092c,
		      IP $srcr_ip,
		      ETH $wireless_mac,
		      LT lt,
		      ARP arp,
		      PERIOD 15,
		      GW $gateway);


gw -> SetSRChecksum -> [0] output;

set_gw :: SetGateway(SEL gw);

gw_reply ::  SR1GatewayResponder(SEL gw, 
				 ETHTYPE 0x0945,
				 IP $srcr_ip,
				 ETH $wireless_mac,
				 ARP arp,
				 DEBUG false,
				 LT lt,
				 PERIOD 15);


gw_reply -> SetSRChecksum -> [1] output;



es :: ETTStat(ETHTYPE 0x0941, 
	      ETH $wireless_mac, 
	      IP $srcr_ip, 
	      PERIOD 30000,
	      TAU 300000,
	      ARP arp,
	      PROBES $probes,
	      ETT metric,
	      RT rates);


metric :: ETTMetric(LT lt);


forwarder :: SRForwarder(ETHTYPE 0x0943, 
			      IP $srcr_ip, 
			      ETH $wireless_mac, 
			      ARP arp, 
			      LT lt);


querier :: SRQuerier(ETH $wireless_mac, 
		     SR forwarder,
		     LT lt, 
		     ROUTE_DAMPENING true,
		     TIME_BEFORE_SWITCH 5,
		     DEBUG false);


query_forwarder :: MetricFlood(ETHTYPE 0x0944,
			       IP $srcr_ip, 
			       ETH $wireless_mac, 
			       LT lt, 
			       ARP arp,
			       DEBUG false);

query_responder :: SRQueryResponder(ETHTYPE 0x0945,
				    IP $srcr_ip, 
				    ETH $wireless_mac, 
				    LT lt, 
				    ARP arp,
				    DEBUG false);


query_responder -> SetSRChecksum -> [0] output;
query_forwarder -> SetSRChecksum -> [0] output;
query_forwarder [1] -> query_responder;

data_ck :: SetSRChecksum() 

input [1] 
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
-> ICMPError($srcr_ip, timeexceeded, 0) 
-> querier;


// queries
querier [1] -> [1] query_forwarder;
es -> [1] output;


forwarder[1] //ip packets to me
  -> StripSRHeader()
  -> CheckIPHeader(CHECKSUM false)
  -> from_gw_cl :: IPClassifier(src net $srcr_ip mask $srcr_nm,
				-)
  -> [3] output;

from_gw_cl [1] -> [1] set_gw [1] -> [3] output;

 input [0]
   -> ncl :: Classifier(
			12/0943 , //srcr_forwarder
			12/0944 , //srcr
			12/0945 , //replies
			12/0941 , //srcr_es
			12/092c , //srcr_gw
			);
 
 
 ncl[0] -> CheckSRHeader() -> [0] forwarder;
 ncl[1] -> CheckSRHeader() -> query_forwarder
 ncl[2] -> CheckSRHeader() -> query_responder;
 ncl[3] -> es;
 ncl[4] -> CheckSRHeader() -> gw;
}



elementclass sr2 {
  $sr2_ip, $sr2_nm, $wireless_mac, $gateway, $probes|


arp :: ARPTable();
lt :: LinkTable(IP $sr2_ip);


gw :: SR2GatewaySelector(ETHTYPE 0x062c,
		      IP $sr2_ip,
		      ETH $wireless_mac,
		      LT lt,
		      ARP arp,
		      PERIOD 15,
		      GW $gateway);


gw -> SR2SetChecksum -> [0] output;

set_gw :: SR2SetGateway(SEL gw);


es :: SR2ETTStat(ETHTYPE 0x0641, 
	      ETH $wireless_mac, 
	      IP $sr2_ip, 
	      PERIOD 30000,
	      TAU 300000,
	      ARP arp,
	      PROBES $probes,
	      ETT metric,
	      RT rates);


metric :: SR2ETTMetric(LT lt);


forwarder :: SR2Forwarder(ETHTYPE 0x0643, 
			      IP $sr2_ip, 
			      ETH $wireless_mac, 
			      ARP arp, 
			      LT lt);


querier :: SR2Querier(ETH $wireless_mac, 
		     SR forwarder,
		     LT lt, 
		     ROUTE_DAMPENING true,
		     TIME_BEFORE_SWITCH 5,
		     DEBUG false);


query_forwarder :: SR2MetricFlood(ETHTYPE 0x0644,
			       IP $sr2_ip, 
			       ETH $wireless_mac, 
			       LT lt, 
			       ARP arp,
			       DEBUG false);

query_responder :: SR2QueryResponder(ETHTYPE 0x0645,
				    IP $sr2_ip, 
				    ETH $wireless_mac, 
				    LT lt, 
				    ARP arp,
				    DEBUG false);


gw_reply ::  SR2GatewayResponder(SEL gw, 
				 ETHTYPE 0x0645,
				 IP $sr2_ip,
				 ETH $wireless_mac,
				 ARP arp,
				 DEBUG false,
				 LT lt,
				 PERIOD 15);


gw_reply -> SR2SetChecksum -> [1] output;

query_responder -> SR2SetChecksum -> [0] output;
query_forwarder -> SR2SetChecksum -> [0] output;
query_forwarder [1] -> query_responder;

data_ck :: SR2SetChecksum() 

input [1] 
-> host_cl :: IPClassifier(dst net $sr2_ip mask $sr2_nm,
				-)
-> querier
-> data_ck;


host_cl [1] -> [0] set_gw [0] -> querier;

forwarder[0] 
  -> dt ::DecIPTTL
  -> data_ck
  -> [2] output;


dt[1] 
-> ICMPError($sr2_ip, timeexceeded, 0) 
-> querier;


// queries
querier [1] -> [1] query_forwarder;
es -> SetTimestamp() -> [1] output;


forwarder[1] //ip packets to me
  -> SR2StripHeader()
  -> CheckIPHeader(CHECKSUM false)
  -> from_gw_cl :: IPClassifier(src net $sr2_ip mask $sr2_nm,
				-)
  -> [3] output;

from_gw_cl [1] -> [1] set_gw [1] -> [3] output;

 input [0]
   -> ncl :: Classifier(
			12/0643 , //sr2_forwarder
			12/0644 , //sr2
			12/0645 , //replies
			12/0641 , //sr2_es
			12/062c , //sr2_gw
			);
 
 
 ncl[0] -> SR2CheckHeader() -> [0] forwarder;
 ncl[1] -> SR2CheckHeader() -> query_forwarder
 ncl[2] -> SR2CheckHeader() -> query_responder;
 ncl[3] -> es;
 ncl[4] -> SR2CheckHeader() -> gw;
 
}


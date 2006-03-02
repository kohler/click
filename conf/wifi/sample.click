# 1 "/tmp/unaligned.click"
rates :: AvailableRates(DEFAULT 2 4 11 12 18 22 24 36 48 72 96 108,
00:09:5B:F7:D6:D3 2 4 11 12 18 22 24 36 48 72 96 108);
# 282 "/tmp/unaligned.click"
control :: ControlSocket("TCP", 7777);
# 283 "/tmp/unaligned.click"
chatter :: ChatterSocket("TCP", 7778);
# 321 "/tmp/unaligned.click"
sched :: PrioSched;
# 322 "/tmp/unaligned.click"
set_power :: SetTXPower(POWER 60);
# 323 "/tmp/unaligned.click"
athdesc_encap :: AthdescEncap;
# 327 "/tmp/unaligned.click"
route_q :: FullNoteQueue(10);
# 330 "/tmp/unaligned.click"
data_q :: FullNoteQueue(10);
# 331 "/tmp/unaligned.click"
data_static_rate :: SetTXRate(RATE 2);
# 332 "/tmp/unaligned.click"
data_madwifi_rate :: MadwifiRate(OFFSET 4,
			       ALT_RATE true,
			       RT rates,
			       ACTIVE true);
# 339 "/tmp/unaligned.click"
Idle@12 :: Idle;
# 343 "/tmp/unaligned.click"
route_encap :: WifiEncap(0x0, 00:00:00:00:00:00);
# 345 "/tmp/unaligned.click"
data_encap :: WifiEncap(0x0, 00:00:00:00:00:00);
# 355 "/tmp/unaligned.click"
srcr1_cl :: IPClassifier(dst net 10.0.0.0/8, -);
# 357 "/tmp/unaligned.click"
ap_to_srcr1 :: SRDestCache;
# 366 "/tmp/unaligned.click"
srcr1_cl2 :: IPClassifier(src net 10.0.0.0/8, -);
# 379 "/tmp/unaligned.click"
srcr2_cl :: IPClassifier(dst net 10.0.0.0/8, -);
# 381 "/tmp/unaligned.click"
ap_to_srcr2 :: SRDestCache;
# 390 "/tmp/unaligned.click"
srcr2_cl2 :: IPClassifier(src net 10.0.0.0/8, -);
# 401 "/tmp/unaligned.click"
athdesc_decap :: AthdescDecap;
# 402 "/tmp/unaligned.click"
phyerr_filter :: FilterPhyErr;
# 403 "/tmp/unaligned.click"
Classifier@27 :: Classifier(0/08%0c);
# 404 "/tmp/unaligned.click"
tx_filter :: FilterTX;
# 405 "/tmp/unaligned.click"
dupe :: WifiDupeFilter;
# 406 "/tmp/unaligned.click"
WifiDecap@30 :: WifiDecap;
# 407 "/tmp/unaligned.click"
HostEtherFilter@31 :: HostEtherFilter(00:09:5B:F7:D6:D3, DROP_OTHER true, DROP_OWN true);
# 408 "/tmp/unaligned.click"
ncl :: Classifier(12/09??, 12/06??);
# 313 "/tmp/unaligned.click"
sniff_dev/from_dev :: FromDevice(ath1, 
			 PROMISC false);
# 316 "/tmp/unaligned.click"
sniff_dev/to_dev :: ToDevice(ath1);
# 9 "/tmp/unaligned.click"
srcr1/arp :: ARPTable;
# 10 "/tmp/unaligned.click"
srcr1/lt :: LinkTable(IP 5.247.214.211);
# 13 "/tmp/unaligned.click"
srcr1/gw :: GatewaySelector(ETHTYPE 0x092c,
		      IP 5.247.214.211,
		      ETH 00:09:5B:F7:D6:D3,
		      LT lt,
		      ARP arp,
		      PERIOD 15,
		      GW false);
# 22 "/tmp/unaligned.click"
srcr1/SetSRChecksum@4 :: SetSRChecksum;
# 24 "/tmp/unaligned.click"
srcr1/set_gw :: SetGateway(SEL gw);
# 26 "/tmp/unaligned.click"
srcr1/gw_reply :: SR1GatewayResponder(SEL gw, 
				 ETHTYPE 0x0945,
				 IP 5.247.214.211,
				 ETH 00:09:5B:F7:D6:D3,
				 ARP arp,
				 DEBUG false,
				 LT lt,
				 PERIOD 15);
# 40 "/tmp/unaligned.click"
srcr1/es :: ETTStat(ETHTYPE 0x0941, 
	      ETH 00:09:5B:F7:D6:D3, 
	      IP 5.247.214.211, 
	      PERIOD 30000,
	      TAU 300000,
	      ARP arp,
	      PROBES "2 60 2 1500 4 1500 11 1500 22 1500",
	      ETT metric,
	      RT rates);
# 51 "/tmp/unaligned.click"
srcr1/metric :: ETTMetric(LT lt);
# 54 "/tmp/unaligned.click"
srcr1/forwarder :: SRForwarder(ETHTYPE 0x0943, 
			      IP 5.247.214.211, 
			      ETH 00:09:5B:F7:D6:D3, 
			      ARP arp, 
			      LT lt);
# 61 "/tmp/unaligned.click"
srcr1/querier :: SRQuerier(ETH 00:09:5B:F7:D6:D3, 
		     SR forwarder,
		     LT lt, 
		     ROUTE_DAMPENING true,
		     TIME_BEFORE_SWITCH 5,
		     DEBUG true);
# 69 "/tmp/unaligned.click"
srcr1/query_forwarder :: MetricFlood(ETHTYPE 0x0944,
			       IP 5.247.214.211, 
			       ETH 00:09:5B:F7:D6:D3, 
			       LT lt, 
			       ARP arp,
			       DEBUG false);
# 76 "/tmp/unaligned.click"
srcr1/query_responder :: SRQueryResponder(ETHTYPE 0x0945,
				    IP 5.247.214.211, 
				    ETH 00:09:5B:F7:D6:D3, 
				    LT lt, 
				    ARP arp,
				    DEBUG true);
# 84 "/tmp/unaligned.click"
srcr1/SetSRChecksum@13 :: SetSRChecksum;
# 85 "/tmp/unaligned.click"
srcr1/SetSRChecksum@14 :: SetSRChecksum;
# 85 "/tmp/unaligned.click"
srcr1/PrintSR@15 :: PrintSR(forwarding);
# 88 "/tmp/unaligned.click"
srcr1/data_ck :: SetSRChecksum;
# 91 "/tmp/unaligned.click"
srcr1/host_cl :: IPClassifier(dst net 5.247.214.211 mask 255.0.0.0,
				-);
# 100 "/tmp/unaligned.click"
srcr1/dt :: DecIPTTL;
# 106 "/tmp/unaligned.click"
srcr1/Print@19 :: Print(ttl-error);
# 107 "/tmp/unaligned.click"
srcr1/ICMPError@20 :: ICMPError(5.247.214.211, timeexceeded, 0);
# 117 "/tmp/unaligned.click"
srcr1/StripSRHeader@21 :: StripSRHeader;
# 118 "/tmp/unaligned.click"
srcr1/CheckIPHeader@22 :: CheckIPHeader(CHECKSUM false);
# 119 "/tmp/unaligned.click"
srcr1/from_gw_cl :: IPClassifier(src net 5.247.214.211 mask 255.0.0.0,
				-);
# 126 "/tmp/unaligned.click"
srcr1/ncl :: Classifier(
			12/0943 , //srcr_forwarder
			12/0944 , //srcr
			12/0945 , //replies
			12/0941 , //srcr_es
			12/092c , //srcr_gw
			);
# 135 "/tmp/unaligned.click"
srcr1/CheckSRHeader@25 :: CheckSRHeader;
# 136 "/tmp/unaligned.click"
srcr1/CheckSRHeader@26 :: CheckSRHeader;
# 136 "/tmp/unaligned.click"
srcr1/PrintSR@27 :: PrintSR(query);
# 137 "/tmp/unaligned.click"
srcr1/CheckSRHeader@28 :: CheckSRHeader;
# 139 "/tmp/unaligned.click"
srcr1/CheckSRHeader@29 :: CheckSRHeader;
# 300 "/tmp/unaligned.click"
srcr1_host/KernelTun@1 :: KernelTun(5.247.214.211/255.0.0.0, MTU 1500, DEV_NAME srcr1);
# 301 "/tmp/unaligned.click"
srcr1_host/MarkIPHeader@2 :: MarkIPHeader(0);
# 302 "/tmp/unaligned.click"
srcr1_host/CheckIPHeader@3 :: CheckIPHeader(CHECKSUM false);
# 148 "/tmp/unaligned.click"
srcr2/arp :: ARPTable;
# 149 "/tmp/unaligned.click"
srcr2/lt :: LinkTable(IP 6.247.214.211);
# 152 "/tmp/unaligned.click"
srcr2/gw :: SR2GatewaySelector(ETHTYPE 0x062c,
		      IP 6.247.214.211,
		      ETH 00:09:5B:F7:D6:D3,
		      LT lt,
		      ARP arp,
		      PERIOD 15,
		      GW false);
# 161 "/tmp/unaligned.click"
srcr2/SR2SetChecksum@4 :: SR2SetChecksum;
# 163 "/tmp/unaligned.click"
srcr2/set_gw :: SR2SetGateway(SEL gw);
# 166 "/tmp/unaligned.click"
srcr2/es :: SR2ETTStat(ETHTYPE 0x0641, 
	      ETH 00:09:5B:F7:D6:D3, 
	      IP 6.247.214.211, 
	      PERIOD 30000,
	      TAU 300000,
	      ARP arp,
	      PROBES "2 60 2 1500 4 1500 11 1500 22 1500",
	      ETT metric,
	      RT rates);
# 177 "/tmp/unaligned.click"
srcr2/metric :: SR2ETTMetric(LT lt);
# 180 "/tmp/unaligned.click"
srcr2/forwarder :: SR2Forwarder(ETHTYPE 0x0643, 
			      IP 6.247.214.211, 
			      ETH 00:09:5B:F7:D6:D3, 
			      ARP arp, 
			      LT lt);
# 187 "/tmp/unaligned.click"
srcr2/querier :: SR2Querier(ETH 00:09:5B:F7:D6:D3, 
		     SR forwarder,
		     LT lt, 
		     ROUTE_DAMPENING true,
		     TIME_BEFORE_SWITCH 5,
		     DEBUG true);
# 195 "/tmp/unaligned.click"
srcr2/query_forwarder :: SR2MetricFlood(ETHTYPE 0x0644,
			       IP 6.247.214.211, 
			       ETH 00:09:5B:F7:D6:D3, 
			       LT lt, 
			       ARP arp,
			       DEBUG false);
# 202 "/tmp/unaligned.click"
srcr2/query_responder :: SR2QueryResponder(ETHTYPE 0x0645,
				    IP 6.247.214.211, 
				    ETH 00:09:5B:F7:D6:D3, 
				    LT lt, 
				    ARP arp,
				    DEBUG true);
# 210 "/tmp/unaligned.click"
srcr2/gw_reply :: SR2GatewayResponder(SEL gw, 
				 ETHTYPE 0x0945,
				 IP $srcr_ip,
				 ETH 00:09:5B:F7:D6:D3,
				 ARP arp,
				 DEBUG false,
				 LT lt,
				 PERIOD 15);
# 222 "/tmp/unaligned.click"
srcr2/SR2SetChecksum@13 :: SR2SetChecksum;
# 223 "/tmp/unaligned.click"
srcr2/SR2SetChecksum@14 :: SR2SetChecksum;
# 223 "/tmp/unaligned.click"
srcr2/SR2Print@15 :: SR2Print(forwarding);
# 226 "/tmp/unaligned.click"
srcr2/data_ck :: SR2SetChecksum;
# 229 "/tmp/unaligned.click"
srcr2/host_cl :: IPClassifier(dst net 6.247.214.211 mask 255.0.0.0,
				-);
# 238 "/tmp/unaligned.click"
srcr2/dt :: DecIPTTL;
# 244 "/tmp/unaligned.click"
srcr2/Print@19 :: Print(ttl-error);
# 245 "/tmp/unaligned.click"
srcr2/ICMPError@20 :: ICMPError(6.247.214.211, timeexceeded, 0);
# 251 "/tmp/unaligned.click"
srcr2/SetTimestamp@21 :: SetTimestamp;
# 255 "/tmp/unaligned.click"
srcr2/SR2StripHeader@22 :: SR2StripHeader;
# 256 "/tmp/unaligned.click"
srcr2/CheckIPHeader@23 :: CheckIPHeader(CHECKSUM false);
# 257 "/tmp/unaligned.click"
srcr2/from_gw_cl :: IPClassifier(src net 6.247.214.211 mask 255.0.0.0,
				-);
# 264 "/tmp/unaligned.click"
srcr2/ncl :: Classifier(
			12/0643 , //sr2_forwarder
			12/0644 , //sr2
			12/0645 , //replies
			12/0641 , //sr2_es
			12/062c , //sr2_gw
			);
# 273 "/tmp/unaligned.click"
srcr2/SR2CheckHeader@26 :: SR2CheckHeader;
# 274 "/tmp/unaligned.click"
srcr2/SR2CheckHeader@27 :: SR2CheckHeader;
# 274 "/tmp/unaligned.click"
srcr2/PrintSR@28 :: PrintSR(query);
# 275 "/tmp/unaligned.click"
srcr2/SR2CheckHeader@29 :: SR2CheckHeader;
# 277 "/tmp/unaligned.click"
srcr2/SR2CheckHeader@30 :: SR2CheckHeader;
# 300 "/tmp/unaligned.click"
srcr2_host/KernelTun@1 :: KernelTun(6.247.214.211/255.0.0.0, MTU 1500, DEV_NAME srcr2);
# 301 "/tmp/unaligned.click"
srcr2_host/MarkIPHeader@2 :: MarkIPHeader(0);
# 302 "/tmp/unaligned.click"
srcr2_host/CheckIPHeader@3 :: CheckIPHeader(CHECKSUM false);
# 0 "<click-align>"
Align@click_align@105 :: Align(4, 0);
# 0 "<click-align>"
Align@click_align@115 :: Align(4, 0);
# 0 "<click-align>"
AlignmentInfo@click_align@97 :: AlignmentInfo(rates,
  control,
  chatter,
  sched  1 0  4 2,
  set_power  1 0,
  athdesc_encap  1 0,
  route_q  1 0,
  data_q  4 2,
  data_static_rate  4 2,
  data_madwifi_rate  4 2  4 2,
  Idle@12,
  route_encap  1 0,
  data_encap  4 2,
  srcr1_cl  4 0,
  ap_to_srcr1  4 0  4 0,
  srcr1_cl2  4 0,
  srcr2_cl  4 0,
  ap_to_srcr2  4 0  4 0,
  srcr2_cl2  4 0,
  athdesc_decap  4 2,
  phyerr_filter  4 2,
  Classifier@27  4 2,
  tx_filter  4 2,
  dupe  4 2,
  WifiDecap@30  4 2,
  HostEtherFilter@31  4 2,
  ncl  4 2,
  sniff_dev/from_dev,
  sniff_dev/to_dev  1 0,
  srcr1/arp,
  srcr1/lt,
  srcr1/gw  4 2,
  srcr1/SetSRChecksum@4  4 2,
  srcr1/set_gw  4 0  4 0,
  srcr1/gw_reply,
  srcr1/es  4 2,
  srcr1/metric,
  srcr1/forwarder  4 0,
  srcr1/querier  4 0,
  srcr1/query_forwarder  4 2  4 0,
  srcr1/query_responder  2 0,
  srcr1/SetSRChecksum@13  2 0,
  srcr1/SetSRChecksum@14  2 0,
  srcr1/PrintSR@15  4 2,
  srcr1/data_ck  4 0,
  srcr1/host_cl  4 0,
  srcr1/dt  4 0,
  srcr1/Print@19  4 0,
  srcr1/ICMPError@20  4 0,
  srcr1/StripSRHeader@21  4 0,
  srcr1/CheckIPHeader@22  4 0,
  srcr1/from_gw_cl  4 0,
  srcr1/ncl  4 2,
  srcr1/CheckSRHeader@25  4 2,
  srcr1/CheckSRHeader@26  4 2,
  srcr1/PrintSR@27  4 2,
  srcr1/CheckSRHeader@28  4 2,
  srcr1/CheckSRHeader@29  4 2,
  srcr1_host/KernelTun@1  4 0,
  srcr1_host/MarkIPHeader@2  4 0,
  srcr1_host/CheckIPHeader@3  4 0,
  srcr2/arp,
  srcr2/lt,
  srcr2/gw  4 2,
  srcr2/SR2SetChecksum@4  4 2,
  srcr2/set_gw  4 0  4 0,
  srcr2/es  4 2,
  srcr2/metric,
  srcr2/forwarder  4 0,
  srcr2/querier  4 0,
  srcr2/query_forwarder  4 2  4 0,
  srcr2/query_responder  2 0,
  srcr2/gw_reply,
  srcr2/SR2SetChecksum@13  2 0,
  srcr2/SR2SetChecksum@14  2 0,
  srcr2/SR2Print@15  4 2,
  srcr2/data_ck  4 0,
  srcr2/host_cl  4 0,
  srcr2/dt  4 0,
  srcr2/Print@19  4 0,
  srcr2/ICMPError@20  4 0,
  srcr2/SetTimestamp@21  4 2,
  srcr2/SR2StripHeader@22  4 0,
  srcr2/CheckIPHeader@23  4 0,
  srcr2/from_gw_cl  4 0,
  srcr2/ncl  4 2,
  srcr2/SR2CheckHeader@26  4 2,
  srcr2/SR2CheckHeader@27  4 2,
  srcr2/PrintSR@28  4 2,
  srcr2/SR2CheckHeader@29  4 2,
  srcr2/SR2CheckHeader@30  4 2,
  srcr2_host/KernelTun@1  4 0,
  srcr2_host/MarkIPHeader@2  4 0,
  srcr2_host/CheckIPHeader@3  4 0,
  Align@click_align@105  4 2,
  Align@click_align@115  4 2);
# 392 ""
srcr2_cl2 [1] -> srcr2_host/KernelTun@1
    -> srcr2_host/MarkIPHeader@2
    -> srcr2_host/CheckIPHeader@3
    -> srcr2_cl
    -> ap_to_srcr2
    -> srcr2/host_cl
    -> srcr2/querier
    -> srcr2/data_ck
    -> data_encap
    -> data_q
    -> data_static_rate
    -> data_madwifi_rate
    -> [1] sched;
Idle@12 -> [1] data_madwifi_rate;
ncl [1] -> srcr2/ncl
    -> srcr2/SR2CheckHeader@26
    -> Align@click_align@115
    -> srcr2/forwarder
    -> srcr2/dt
    -> srcr2/data_ck;
srcr2_cl [1] -> srcr2/host_cl;
ap_to_srcr1 [1] -> srcr1_host/KernelTun@1
    -> srcr1_host/MarkIPHeader@2
    -> srcr1_host/CheckIPHeader@3
    -> srcr1_cl
    -> ap_to_srcr1
    -> srcr1/host_cl
    -> srcr1/querier
    -> srcr1/data_ck
    -> data_encap;
srcr1_cl2 [1] -> srcr1_host/KernelTun@1;
srcr1_cl [1] -> srcr1/host_cl;
sniff_dev/from_dev -> athdesc_decap
    -> phyerr_filter
    -> Classifier@27
    -> tx_filter
    -> dupe
    -> WifiDecap@30
    -> HostEtherFilter@31
    -> ncl
    -> srcr1/ncl
    -> srcr1/CheckSRHeader@25
    -> Align@click_align@105
    -> srcr1/forwarder
    -> srcr1/dt
    -> srcr1/data_ck;
srcr2/set_gw [1] -> srcr2_cl2
    -> [1] ap_to_srcr2;
srcr2/gw_reply -> route_encap
    -> route_q
    -> sched
    -> set_power
    -> athdesc_encap
    -> sniff_dev/to_dev;
srcr1/set_gw [1] -> srcr1_cl2
    -> [1] ap_to_srcr1;
tx_filter [1] -> [1] data_madwifi_rate;
srcr1/host_cl [1] -> srcr1/set_gw
    -> srcr1/querier;
srcr1/gw_reply -> route_encap;
srcr1/query_forwarder [1] -> srcr1/query_responder
    -> srcr1/SetSRChecksum@13
    -> route_encap;
srcr1/dt [1] -> srcr1/Print@19
    -> srcr1/ICMPError@20
    -> srcr1/querier;
srcr1/querier [1] -> [1] srcr1/query_forwarder;
srcr1/forwarder [1] -> srcr1/StripSRHeader@21
    -> srcr1/CheckIPHeader@22
    -> srcr1/from_gw_cl
    -> srcr1_cl2;
srcr1/from_gw_cl [1] -> [1] srcr1/set_gw;
srcr2/ncl [4] -> srcr2/SR2CheckHeader@30
    -> srcr2/gw
    -> srcr2/SR2SetChecksum@4
    -> route_encap;
srcr1/ncl [1] -> srcr1/CheckSRHeader@26
    -> srcr1/PrintSR@27
    -> srcr1/query_forwarder
    -> srcr1/SetSRChecksum@14
    -> srcr1/PrintSR@15
    -> route_encap;
srcr1/ncl [2] -> srcr1/CheckSRHeader@28
    -> srcr1/query_responder;
srcr1/ncl [3] -> srcr1/es
    -> route_encap;
srcr1/ncl [4] -> srcr1/CheckSRHeader@29
    -> srcr1/gw
    -> srcr1/SetSRChecksum@4
    -> route_encap;
srcr2/ncl [3] -> srcr2/es
    -> srcr2/SetTimestamp@21
    -> route_encap;
srcr2/host_cl [1] -> srcr2/set_gw
    -> srcr2/querier;
srcr2/ncl [2] -> srcr2/SR2CheckHeader@29
    -> srcr2/query_responder
    -> srcr2/SR2SetChecksum@13
    -> route_encap;
srcr2/ncl [1] -> srcr2/SR2CheckHeader@27
    -> srcr2/PrintSR@28
    -> srcr2/query_forwarder
    -> srcr2/SR2SetChecksum@14
    -> srcr2/SR2Print@15
    -> route_encap;
srcr2/query_forwarder [1] -> srcr2/query_responder;
srcr2/from_gw_cl [1] -> [1] srcr2/set_gw;
srcr2/dt [1] -> srcr2/Print@19
    -> srcr2/ICMPError@20
    -> srcr2/querier;
srcr2/querier [1] -> [1] srcr2/query_forwarder;
srcr2/forwarder [1] -> srcr2/SR2StripHeader@22
    -> srcr2/CheckIPHeader@23
    -> srcr2/from_gw_cl
    -> srcr2_cl2;
ap_to_srcr2 [1] -> srcr2_host/KernelTun@1;

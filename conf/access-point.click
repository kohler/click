//access_point.click

// This configuration contains a configuration for running 
// an 802.11b access point.
// It creates an interface using FromHost called "ap"
// and uses open authentication to allow stations to associate to it.
// This configuration assumes that you have a network interface named
// ath0 and that it is located on channel 3.

// Run it at user level with
// 'click access_point.click'

// Run it in the Linux kernel with
// 'click-install access-point.click'
// Messages are printed to the system log (run 'dmesg' to see them, or look
// in /var/log/messages), and to the file '/click/messages'.

AddressInfo(ap_bssid 10.0.0.1/8 00:05:4E:46:97:28);

winfo :: WirelessInfo(SSID "click-ssid", BSSID 00:05:4E:46:97:28, CHANNEL 1);

rates :: AvailableRates(DEFAULT 2 4 11 22);

q :: Queue(10)
-> ToDevice (ath0);

FromDevice(ath0)
-> prism2_decap :: Prism2Decap()
-> extra_decap :: ExtraDecap()
-> phyerr_filter :: FilterPhyErr()
-> tx_filter :: FilterTX()
-> dupe :: WifiDupeFilter(WINDOW 20) 
-> wifi_cl :: Classifier(0/08%0c 1/01%03, //data
			 0/00%0c); //mgt

wifi_cl [1] -> mgt_cl :: Classifier(0/00%f0, //assoc req
				    0/10%f0, //assoc resp
				    0/40%f0, //probe req
				    0/50%f0, //probe resp
				    0/80%f0, //beacon
				    0/a0%f0, //disassoc
				    0/b0%f0, //disassoc
				    );

wifi_cl [0] 
-> decap :: WifiDecap()
-> HostEtherFilter(ap_bssid, DROP_OTHER true, DROP_OWN true) 
-> ToHost(ap)


mgt_cl [0] -> Print ("assoc_req")
-> ar :: AssociationResponder(WIRELESS_INFO winfo,
			      RT rates)
-> q;

mgt_cl [1] -> Print ("assoc_resp") -> Discard;

mgt_cl [2]
-> beacon_source :: BeaconSource(WIRELESS_INFO winfo,
				 RT rates)
-> q;

mgt_cl [3] -> Print ("probe_resp", 200) -> Discard;
mgt_cl [4] -> bs :: BeaconScanner(RT rates) -> Discard; 
mgt_cl [5] -> Print ("disassoc") -> Discard;
mgt_cl [6] -> Print ("auth") -> auth :: OpenAuthResponder(WIRELESS_INFO winfo) -> q;


FromHost(ap, ap_bssid, ETHER ap_bssid)
-> wifi_encap :: WifiEncap(0x02, WIRELESS_INFO winfo)
-> q;





//access_point.click

// This configuration contains a configuration for running 
// an 802.11b access point.
// It creates an interface using FromHost called "ap"
// and uses open authentication to allow stations to associate to it.
// This configuration assumes that you have a network interface named
// ath1 and that it is located on channel 11.

// Run it at user level with
// 'click access_point.click'

// Run it in the Linux kernel with
// 'click-install access-point.click'
// Messages are printed to the system log (run 'dmesg' to see them, or look
// in /var/log/messages), and to the file '/click/messages'.

AddressInfo(ap_bssid 10.0.0.1/8 ath1);

winfo :: WirelessInfo(SSID "g9", BSSID ap_bssid, CHANNEL 11);

rates :: AvailableRates(DEFAULT 2 4 11 22);

control :: ControlSocket("TCP", 6777);
chatter :: ChatterSocket("TCP", 6778);

FromHost(ap, ap_bssid, ETHER ap_bssid)
  -> wifi_encap :: WifiEncap(0x02, WIRELESS_INFO winfo)
  -> set_rate :: SetTXRate(22)
  -> q :: Queue(10)
  -> ExtraEncap()
  -> to_dev :: ToDevice (ath1);

from_dev :: FromDevice(ath1)
-> prism2_decap :: Prism2Decap()
-> extra_decap :: ExtraDecap()
-> phyerr_filter :: FilterPhyErr()
-> tx_filter :: FilterTX()
-> dupe :: WifiDupeFilter() 
-> wifi_cl :: Classifier(0/08%0c 1/01%03, //data
			 0/00%0c); //mgt

wifi_cl [0] 
-> decap :: WifiDecap()
-> HostEtherFilter(ap_bssid, DROP_OTHER true, DROP_OWN true) 
-> ToHost(ap)

wifi_cl [1] -> mgt_cl :: Classifier(0/00%f0, //assoc req
				    0/10%f0, //assoc resp
				    0/40%f0, //probe req
				    0/50%f0, //probe resp
				    0/80%f0, //beacon
				    0/a0%f0, //disassoc
				    0/b0%f0, //disassoc
				    );



mgt_cl [0] -> PrintWifi()
-> ar :: AssociationResponder(WIRELESS_INFO winfo,
			      RT rates)
-> q;

mgt_cl [1] -> PrintWifi() -> Discard;

mgt_cl [2]
-> beacon_source :: BeaconSource(WIRELESS_INFO winfo,
				 RT rates)
-> q;

mgt_cl [3] -> PrintWifi() -> Discard;
mgt_cl [4] -> bs :: BeaconScanner(RT rates) -> Discard; 
mgt_cl [5] -> PrintWifi() -> Discard;
mgt_cl [6] -> PrintWifi() -> auth :: OpenAuthResponder(WIRELESS_INFO winfo) -> q;








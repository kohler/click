// pseudo-ibss.click

// This configuration contains a configuration for running 
// in 802.11 "pseudo-ibss" (or psuedo ad-hoc) mode, where
// no management packets are sent.
// It creates an interface using FromHost called "safe"
// This configuration assumes that you have a network interface named
// ath0 with the mac address located in the AddressInfo

// Run it at user level with
// 'click pseudo-ibss.click'

// Run it in the Linux kernel with
// 'click-install pseudo-ibss.click'
// Messages are printed to the system log (run 'dmesg' to see them, or look
// in /var/log/messages), and to the file '/click/messages'.

AddressInfo(safe_addr 6.0.0.1/8 ath0);
winfo :: WirelessInfo(BSSID 00:00:00:00:00:00);

FromHost(safe, safe_addr, ETHER safe_addr)
-> q :: Queue()
-> encap :: WifiEncap(0x0, WIRELESS_INFO winfo)
-> set_power :: SetTXPower(63)
-> set_rate :: SetTXRate(2)
-> radiotap_encap :: RadiotapEncap()
-> to_dev :: ToDevice(ath0raw);



from_dev :: FromDevice(ath0raw,
		       BPF_FILTER "ether[18:4] == 0x08 and ")
-> prism2_decap :: Prism2Decap()
-> extra_decap :: ExtraDecap()
-> radiotap_decap :: RadiotapDecap()
-> phyerr_filter :: FilterPhyErr()
-> tx_filter :: FilterTX()
-> dupe :: WifiDupeFilter()
-> wifi_cl :: Classifier(0/08%0c 1/00%03) //nods data
-> WifiDecap()
-> SetPacketType(HOST)
-> ToHost(safe);



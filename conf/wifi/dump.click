// dump.click
// This configuration listens on the device ath0 and 
// acts like tcpdump running on an 802.11 device.
// 
// if you run
// click dump.click -h bs.scan
// when you exit it will print out a list of the access points
// that the scanner received beacons from when click exits.


FromDevice(ath0, PROMISC true) 
-> prism2_decap :: Prism2Decap()
-> extra_decap :: ExtraDecap()
  //-> err_filter :: FilterPhyErr() 
//-> tx_filter :: FilterTX()
-> bs :: BeaconScanner()
-> Classifier(!0/80%f0) // filter out beacons
-> PrintWifi(TIMESTAMP true) 
//-> Print(TIMESTAMP true)
-> WifiDecap()
-> Strip(14)
-> CheckIPHeader()
-> IPPrint(ip)
-> Discard;


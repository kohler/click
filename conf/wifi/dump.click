FromDevice(ath0) 
-> prism2_decap :: Prism2Decap()
-> extra_decap :: ExtraDecap()
-> err_filter :: FilterPhyErr() 
  //-> tx_filter :: FilterTX()
  //-> WifiDupeFilter()
-> bs :: BeaconScanner()
-> Classifier(!0/80%f0) // filter out beacons
-> PrintWifi(TIMESTAMP true) 
//-> Print(TIMESTAMP true)
-> Discard;


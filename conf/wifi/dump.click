

FromDevice(ath0) 
-> prism2_decap :: Prism2Decap()
-> extra_decap :: ExtraDecap()
-> err_filter :: FilterPhyErr() 
-> tx_filter :: FilterTX()
-> WifiDupeFilter(WINDOW 20)
-> Classifier(!0/80%f0) // filter out beacons
-> PrintWifi() 
-> Discard;


// grid3.click

li :: LocationInfo(0, 0)

rh :: ReadHandlerCaller(1) 

// protocol els
nb :: Neighbor(NBR_TIMEOUT, LR_PERIOD, LR_JITTER, MAC_ADDR, GRID_IP)
lr :: LocalRoute(MAC_ADDR, GRID_IP, nb)

// device layer els
from_wvlan :: FromDevice(NET_DEVICE, 0)
to_wvlan :: FixSrcLoc(li) -> SetGridChecksum -> ToDevice(NET_DEVICE)


// linux ip layer els
linux :: Tun(TUN_DEVICE_PREFIX, GRID_IP, GRID_NETMASK, GRID_GW)
to_linux :: Queue -> linux

// hook it all up
from_wvlan -> Classifier(GRID_ETH_PROTO) 
  -> check_grid :: CheckGridHeader
  -> fr :: FilterByRange(1000, li) [0] 
  -> [0] nb [0]
  -> Classifier(GRID_NBR_ENCAP_PROTO)
  -> [0] lr [0] -> to_wvlan

lr [2] -> Discard // packets for geo fwding
lr [3] -> Discard // bad packets

fr [1] -> Discard // out of range

check_grid [1] -> Print(bad_grid_hdr) -> Discard

linux -> cl :: Classifier(32/GRID_HEX_IP, // ip for us
			  GRID_NET_HEX, // ip for Grid network
			  -) // the rest of the world
cl [0] -> to_linux
cl [1] -> GetIPAddress(16) -> [1] lr [1] -> check :: CheckIPHeader [0] -> to_linux
check [1] -> Discard
cl [2] -> SetIPAddress(GRID_GW) -> [1] lr // for grid gateway
nb [1] -> Print(mofo) -> to_wvlan // Routing hello packets

Hello(HELLO_PERIOD, HELLO_JITTER, MAC_ADDR, GRID_IP) -> to_wvlan


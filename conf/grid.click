// grid3.click

ControlSocket(tcp, CONTROL_PORT, CONTROL_RO);

li :: LocationInfo(POS_LAT, POS_LON)

rh :: ReadHandlerCaller(1) 

// protocol els
nb :: Neighbor(NBR_TIMEOUT, LR_PERIOD, LR_JITTER, MAC_ADDR, GRID_IP)
lr :: LocalRoute(MAC_ADDR, GRID_IP, nb)

// device layer els
from_wvlan :: FromDevice(NET_DEVICE, 0)
to_wvlan :: FixSrcLoc(li) -> SetGridChecksum -> ToDevice(NET_DEVICE)


// linux ip layer els
linux :: Tun(GRID_IP, GRID_NETMASK, GRID_GW)

// hook it all up
from_wvlan -> Classifier(12/GRID_ETH_PROTO) 
  -> check_grid :: CheckGridHeader
  -> fr :: FilterByRange(RANGE, li) [0] 
  -> [0] nb [0]
  -> Classifier(15/GRID_NBR_ENCAP_PROTO)
  -> [0] lr [0] -> to_wvlan

lr [2] -> Discard // packets for geo fwding
lr [3] -> Discard // bad packets

fr [1] -> Discard // out of range

check_grid [1] -> Print(bad_grid_hdr) -> Discard

linux -> cl :: Classifier(16/GRID_HEX_IP, // ip for us
			  16/GRID_NET_HEX, // ip for Grid network
			  -) // the rest of the world
cl [0] -> linux
cl [1] -> GetIPAddress(16) -> [1] lr [1] -> check :: CheckIPHeader [0] -> linux
check [1] -> Discard
cl [2] -> SetIPAddress(GRID_GW) -> [1] lr // for grid gateway
nb [1] -> to_wvlan // Routing hello packets

Hello(HELLO_PERIOD, HELLO_JITTER, MAC_ADDR, GRID_IP) -> to_wvlan


// grid2.cpp

// version of grid2.click designed for running through cpp

// use like: gcc -E -P grid2.cpp | click

#include "grid-node-info.h"

li :: LocationInfo(0, 0)

rh :: ReadHandlerCaller(1) 

// protocol els
nb :: Neighbor(NBR_TIMEOUT, MAC_ADDR, GRID_IP)
h :: Hello(HELLO_PERIOD, HELLO_JITTER, MAC_ADDR, GRID_IP, nb)
lr :: LocalRoute(MAC_ADDR, GRID_IP, nb)

// device layer els
from_wvlan :: FromDevice(NET_DEVICE, 0)
to_wvlan :: FixSrcLoc(li) -> SetGridChecksum -> ToDevice(NET_DEVICE)


// linux ip layer els
linux :: Tun(TUN_DEVICE, GRID_IP, GRID_NETMASK)
to_linux :: Queue -> linux

// hook it all up
from_wvlan -> Classifier(GRID_ETH_PROTO) 
  -> check_grid :: CheckGridHeader
  -> fr :: FilterByRange(1000, li) [0] 
  -> nb 
  -> Classifier(GRID_NBR_ENCAP_PROTO)
  -> [0] lr [0] -> to_wvlan
fr [1] -> Print(out_of_range) -> Discard
check_grid [1] -> Print(bad_grid_hdr) -> Discard

linux -> cl :: Classifier(GRID_IP_HEX, // ip for us
			  GRID_NET_HEX, // ip for Grid network
			  -) // the rest of the world
cl [0] -> to_linux
cl [1] -> GetIPAddress(16) -> [1] lr [1] -> check :: CheckIPHeader [0] -> to_linux
check [1] -> Discard
cl [2] -> SetIPAddress(GRID_GW) -> [1] lr // for grid gateway
h -> to_wvlan


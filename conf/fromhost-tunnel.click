// fromhost-tunnel.click

// This configuration demonstrates the use of FromHost, by tunneling all
// packets for TEST_NETWORK via the kernel Click configuration. To adapt
// for your own use, change the AddressInfo element and replace device
// "eth1" with your relevant device.

AddressInfo(
	// The address of a machine and a network segment.
	// This machine will route packets for the network via FromHost,
	// using the host address as source.
	TEST_NETWORK 18.1.1.1/8,

	// This host's real address.
	MY_ADDR 192.150.187.59, 

	// The gateway machine this host really uses to send to TEST_NETWORK.
	MY_GATEWAY 192.150.187.1)

elementclass FixChecksums {
    // fix the IP checksum, and any embedded checksums that include data
    // from the IP header (TCP and UDP in particular)
    input -> SetIPChecksum
	-> ipc :: IPClassifier(tcp, udp, -)
	-> SetTCPChecksum
	-> output;
    ipc[1] -> SetUDPChecksum -> output;
    ipc[2] -> output
}

th :: ToHost;

FromHost(fake0, TEST_NETWORK)
	// Respond to ARP requests
	-> fh_cl :: Classifier(12/0806, 12/0800)
	-> ARPResponder(0/0 1:1:1:1:1:1) 
	-> th

// Forward IP packets from the fake device to the real Internet
fh_cl[1] -> Strip(14)				// remove crap Ether header
	-> MarkIPHeader(0)
	-> StoreIPAddress(MY_ADDR, 12)		// store real address as source
	-> FixChecksums				// recalculate checksum
	-> SetIPAddress(MY_GATEWAY)		// route via gateway
	-> aq :: ARPQuerier(MY_ADDR, eth1)
	-> Queue
	-> ToDevice(eth1)

// listen for ARP responses
fd :: FromDevice(eth1)
	-> fd_cl :: Classifier(12/0806 20/0002, 12/0800, -)
	-> t::Tee -> [1]aq; t[1] -> th		// ARP responses to us and host
fd_cl[2] -> th

// Packets arriving from the Internet from the test network must have their 
// destination addresses changed, to our fake address
fd_cl[1] -> CheckIPHeader(14)
	// check for responses from the test network
	-> ipc :: IPClassifier(src net TEST_NETWORK, -)
	// replace the real destination address with the fake address
	-> StoreIPAddress(TEST_NETWORK, 30)
	-> FixChecksums
	-> th
ipc[1] -> th

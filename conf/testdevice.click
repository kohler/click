// testdevice.click

// Tests whether Click can read packets from the network.
// You may need to modify the device name in FromDevice.
// You'll probably need to be root to run this.

// Run with
//    click conf/testdevice.click
// (runs as a user-level program; uses Linux packet sockets or a similar
// mechanism), or
//    click-install conf/testdevice.click
// (runs inside a Linux kernel).

FromDevice(eth0) -> Print(ok) -> Discard;

// testbpf.click

// Tests whether Click can read packets from the BPF.
// You may need to modify the device name in FromBPF.
// You'll probably need to be root to run this.

// Run with
// userlevel/click < conf/testbpf.click

FromBPF(eth0) -> Print(ok) -> Discard;


FromDevice(eth0) 
  -> is_ip :: Classifier(12/0800, -);
is_ip [1] -> Discard;

is_ip [0] 
  -> Strip(14)
  -> CheckIPHeader 
  -> is_tcp :: IPClassifier(tcp 18.26.4.10, -);
is_tcp [1] -> Discard;

//
// below is the tcp stack
//

ack :: TCPAck;
con :: TCPConn;
dmx :: TCPDemux;
rob :: TCPBuffer(false);
b2p :: BufferConverter(1400);
fid :: CopyFlowID;
seq :: CopyTCPSeq;
out :: SetTCPChecksum
  -> SetIPChecksum
  -> IPPrint(o)
  -> EtherEncap(0x0800, 00:E0:98:09:AB:AF, 00:E0:52:EA:E2:02)
  -> ToDevice(eth0);

is_tcp [0] 
  -> IPPrint(p)
  -> CheckTCPHeader
  -> dmx
  -> [0] ack [0]     // figures out what ack number to use; schedule ACKs
  -> [0] con [0]     // process connection setup/termination packets
  -> IPPrint(i)
  -> rob             // reorder buffer
  -> IPPrint(b)
  -> [0] b2p;        // to application

b2p                  // from application
  -> Queue
  -> [1] con [1]     // sets sa, sp, da, dp, and seq number
  -> Unqueue         // XXX need retransmit thing
  -> [1] ack [1]     // tags on ack number
  -> [0] fid [0]     // remembers flowid
  -> [0] seq [0]     // remembers highest sequence number
  -> out;

con [2]              // send out SYN and SYN ACK packets
  -> [0] fid;

ack [2]              // send explicit ACKs
  -> [1] fid [1]     // set flowid
  -> [1] seq [1]     // set sequence number
  -> out;

Idle -> [1] b2p;     // for path mtu discovery

ControlSocket("TCP", 12345);


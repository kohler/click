
FromDevice(eth0) 
  -> is_ip :: Classifier(12/0800, -);
is_ip [1] -> Discard;

is_ip [0] 
  -> Strip(14)
  -> CheckIPHeader 
  -> is_tcp :: IPClassifier(tcp 18.26.4.102, -);
is_tcp [1] -> Discard;

//
// below is the tcp stack
//

ack :: TCPAck;
con :: TCPConn;
dmx :: TCPDemux;
rob :: TCPBuffer(false);

out :: MarkIPHeader
  -> SetTCPChecksum
  -> SetIPChecksum
  -> IPPrint(o)
  -> Discard;

is_tcp [0] 
  -> IPPrint(d)
  -> CheckTCPHeader
  -> dmx;

dmx [0]
  -> [0] con [0]
  -> [0] ack [0]
  -> IPPrint(i)
  -> rob
  -> IPPrint(b)
  -> Discard;

Idle -> [1] ack [1] -> Discard;

con[1] -> out;
ack[2] -> out;

ControlSocket("TCP", 12345);


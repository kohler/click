elementclass GridNode {
  $ena, $ipa, $lat, $lon | 

#  li :: LocationInfo($lat, $lon, 0);
  li :: LocFromFile($ipa);

  input
    -> HostEtherFilter($ena)
    -> CheckGridHeader
    -> fr :: FilterByRange(250, li)
    -> nn :: Neighbor(10000, 3000, 300, $ena, $ipa)
    -> Classifier(15/03)
    -> lr :: LocalRoute($ena, $ipa, nn)
    -> fl :: FixSrcLoc(li)
    -> SetGridChecksum
    -> oq :: Queue
    -> output;

#  Hello(2000, 100, $ena, $ipa) -> fl;

  TimedSource(1000000) -> [1]lr;
  lr[1] -> Print(fromLR) -> Discard;
  lr[2] -> Print(Geo) -> Discard;
  lr[3] -> Print(BadLR) -> Discard;
  fr[1] -> Discard;
  nn[1] -> fl;

  ICMPSendPings($ipa, 1.0.0.3) -> [1]lr;
};

elementclass LAN3 {
  rr :: RoundRobinSched;
  input[0] -> [0]rr;
  input[1] -> [1]rr;
  input[2] -> [2]rr;
  rr -> PrintGrid -> PullToPush -> t :: Tee;
  t[0] -> [0]output;
  t[1] -> [1]output;
  t[2] -> [2]output;
};

lan :: LAN3;

gn1 :: GridNode(0:0:0:0:0:1, 1.0.0.1, 0, 0);
gn2 :: GridNode(0:0:0:0:0:2, 1.0.0.2, 0, .002);
gn3 :: GridNode(0:0:0:0:0:3, 1.0.0.3, 0, .004);

gn1 -> [0]lan;
gn2 -> [1]lan;
gn3 -> [2]lan;

lan[0] -> gn1;
lan[1] -> gn2;
lan[2] -> gn3;


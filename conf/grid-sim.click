elementclass GridNode {
  $ena, $ipa | 

  li :: LocationInfo(44, 55);

  input
    -> CheckGridHeader
    -> fr :: FilterByRange(250, li)
    -> nn :: Neighbor(2000, $ena, $ipa)
    -> Classifier(15/02)
    -> lr :: LocalRoute($ena, $ipa, nn)
    -> fl :: FixSrcLoc(li)
    -> SetGridChecksum
    -> oq :: Queue
    -> output;

  Hello(500, 100, $ena, $ipa, nn) -> fl;

  TimedSource(1000000) -> [1]lr;
  lr[1] -> Print(fromLR) -> Discard;
  fr[1] -> Discard;
};

elementclass LAN2 {
  rr :: RoundRobinSched;
  input[0] -> [0]rr;
  input[1] -> [1]rr;
  rr -> PrintGrid -> PullToPush -> t :: Tee(2);
  t[0] -> [0]output;
  t[1] -> [1]output;
};

lan :: LAN2;

gn1 :: GridNode(0:0:0:0:0:1, 1.0.0.1);
gn2 :: GridNode(0:0:0:0:0:2, 1.0.0.2);

gn1 -> [0]lan;
gn2 -> [1]lan;

lan[0] -> gn1;
lan[1] -> gn2;


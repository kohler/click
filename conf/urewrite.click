rw :: IPRewriter(pattern 8.0.0.2 1000-2000 - - 0 1);

InfiniteSource(\<00000000111111112222222233333333444444445555>, 3)
   -> UDPIPEncap(1.0.0.2, 50, 2.0.0.2, 100, 1)
   -> IPPrint(is1)
   -> [0]rw;

InfiniteSource(\<00000000111111112222222233333333444444445555>, 3)
   -> UDPIPEncap(10.0.0.2, 50, 1.0.0.2, 100, 1)
   -> IPPrint(is2)
   -> [0]rw;
rw[0] -> IPPrint(rw0) -> IPMirror -> IPPrint(isR) -> [0]rw;
rw[1] -> IPPrint(rw1) -> Discard;



rw :: IPRewriter(pattern 8.0.0.2 1000-2000 - - 0 1);

InfiniteSource(\<00000000111111112222222233333333444444445555>, 3)
   -> UDPIPEncap(1.0.0.2, 50, 2.0.0.2, 100, 1) 
   -> Print(is1)
   -> [0]rw;

InfiniteSource(\<00000000111111112222222233333333444444445555>, 3)
   -> UDPIPEncap(10.0.0.2, 50, 1.0.0.2, 100, 1) 
   -> Print(is2)
   -> [0]rw;
rw[0] -> Print(rw0, 80) -> IPMirror -> Print(isR, 80) -> [0]rw;
rw[1] -> Print(rw1, 80) -> Discard;


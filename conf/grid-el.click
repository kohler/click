// grid-el.click
// Douglas S. J. De Couto
// 28 October 2000

// XXX this is not completed yet.  need to get more ideas about it --
// for example, there are so many inputs and outputs and paramters, I
// am not sure it is even worth it... e.g. there are the multiple
// representations of IP and network addresses: dotted decimal, hex,
// etc., and sometimes we are filtering on these addresses in strange
// offsets (i.e. embedded in a Grid hdr).  also, i think there perl
// substitution is pretty good for the Grid node setup; for the sim, i
// could unify some perl substitution stuff... but i think it is just
// getting too unwieldy now.

// compound Grid router element, that can be plugged into a Grid node,
// a Grid gateway, and a Grid Click simulation.

elementclass GridNode {
  $addr_info, $ipa_hex, $num_hops, $lat, $lon, $hello_timeout, $hello_period, $hello_jitter, $range | 

  li :: GridLocationInfo($lat, $lon, 2);
  nb :: UpdateGridRoutes($hello_timeout, $hello_period, $hello_jitter, $addr_info:eth, $addr_info:ip, $num_hops);
  lr :: LookupLocalGridRoute($addr_info:eth, $addr_info:ip, nb);
  geo :: LookupGeographicGridRoute($addr_info:eth, $addr_info:ip, nb);
  fq :: FloodingLocQuerier($addr_info:eth, $addr_info:ip);
  loc_repl :: LocQueryResponder($addr_info:eth, $addr_info:ip);

  to_wvlan :: FixSrcLoc(li) -> SetGridChecksum -> oq :: Queue -> output;

  grid_demux :: Classifier(15/03, 15/04, 15/05);

  input
    -> HostEtherFilter($addr_info:eth, 1)
    -> check_grid :: CheckGridHeader
    -> fr :: FilterByRange($range, li) [0]
    -> [0] nb [0]
    -> grid_demux [0]
    -> [0] lr [0] -> to_wvlan;

  query_demux :: Classifier(62/$ipa_hex, -);
  repl_demux :: Classifier(62/$ipa_hex, -);

  grid_demux [1] -> query_demux;
  grid_demux [2] -> repl_demux;

  repl_demux [0] -> [1] fq;
  repl_demux [1] -> [0] lr;

  loc_repl -> [0] lr;

  query_demux [0] -> loc_repl;
  query_demux [1] -> [1] fq [1] -> to_wvlan;

  lr [2] -> [0] fq [0] -> [0] geo;
  lr [3] -> PrintGrid(bad_lr) -> Discard;

  geo [0] -> to_wvlan;
  geo [1] -> PrintGrid(bad_geo) -> Discard;
  geo [2] -> Discard;

  fr [1] -> Discard;

  check_grid [1] -> Discard;


  // local IP traffic source goes here; should be IP packets
  input [1] -> cl :: Classifier(16/$ipa_hex, 16/121a07, -);

  cl [0] -> Print(ip_loopback) -> Discard;
  cl [1] -> GetIPAddress(16) -> [1] lr;
  cl [2] -> SetIPAddress(18.26.7.255) -> [1] lr;  // note weird gateway address

  // local IP traffic is sent here
  lr [1] -> [1] output;

  nb [1] -> to_wvlan;

};


elementclass MyPrintGrid {
  $tag |  // print all packets except GRID_LR_HELLO messages
  input -> cl :: Classifier(15/02, -);
  cl [0] -> output;
  cl [1] -> PrintGrid($tag) -> output;
};


% poweredbygrid.mp -- MetaPost graph examples: non-configuration graphs
% Eddie Kohler
%
% Copyright (c) 1999-2001 Massachusetts Institute of Technology
% Copyright (c) 2001-2003 International Computer Science Institute
%
% Permission is hereby granted, free of charge, to any person obtaining a
% copy of this software and associated documentation files (the "Software"),
% to deal in the Software without restriction, subject to the conditions
% listed in the Click LICENSE file. These conditions include: you must
% preserve this copyright notice, and you cannot mention the copyright
% holders in advertising related to the Software without their permission.
% The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
% notice is a summary of the Click LICENSE file; the license in that file is
% legally binding.

input elements;
verbatimtex %&latex
\documentclass[12pt]{article}
\usepackage{elemfig}
\renewcommand\rmdefault{FedraSerif}
\renewcommand\sfdefault{Basalt}
\begin{document}
etex;

%% 1
%% Network diagram for the firewall configuration of samp03_routers.mp,
%% figure 1.
beginfig(1);
  save linecap, w, version, h, s;
  linecap := butt;
  w = 34;
  version = 0;

  vardef xbox(expr x,y,amt) =
    (x*w,y*w) -- ((x+amt)*w,y*w) -- ((x+amt)*w,(y+amt)*w) -- (x*w,(y+amt)*w) -- cycle
  enddef;
  
  elementit.c(btex \element{\textbf{Click!}} etex, 1, 1, push);
  if version = 0:
    c.c = (1w,1.5w);
  else:
    c.n = (1w,2w); c.s = (1w,1w); c.e = (2w,1.5w); c.w = (0w,1.5w);
  fi;
  
  label.urt(btex \textsf{POWERED BY} etex scaled 0.7, (0w,2w));
  h := .1w; s := 2.2;

  label(btex \textsf{g} etex scaled s, (.5w,.5w));
  label(btex \textsf{R} etex scaled s, (1.5w,.5w));
  label(btex \textsf{I} etex scaled s, (2.5w,.5w));
  label(btex \textsf{d} etex scaled s, (3.5w,.5w));

  drawelement(c);
  drawconnarrow (0.05w,2.1w) {dir 190} .. {right} c.in[0];
  drawconnarrow c.out[0] {right} .. (3.8w,1.85w);
  drawconnarrow (3.8w,1.85w) {dir -100} .. {dir -130} (3.33w,1.35w);
  
  pickup pencircle scaled 0.7;
  draw xbox(1,0,.5);
  draw xbox(1,.75,.25);
  draw xbox(1.25,.5,.25);
  draw xbox(1.5,0,.5);
  draw xbox(1.5,.5,.5);

  draw xbox(3,0,.5);
  draw xbox(3.5,.5,.5);

  draw xbox(3,1.5,.5);
  draw xbox(3.5,1,.5);
  draw xbox(3,1,.25);
  draw xbox(3.25,1.25,.25);

  fill xbox(2,1,1) withcolor white;
  label(btex \textsf{\&} etex scaled .8s, (2.5w,1.5w));

  pickup pencircle scaled 2;
  draw xbox(0,0,2);
  draw xbox(0,0,1);
  if version = 0: draw xbox(2,1,1); fi;
  draw xbox(1,0,1);
  draw xbox(2,0,1);
  draw xbox(3,0,1);
  draw xbox(3,1,1);

endfig;



%% 2
%% Network diagram for the firewall configuration of samp03_routers.mp,
%% figure 1.
beginfig(2);
  save linecap;
  linecap := butt;
  w = 34;
  version = 1;

  vardef xbox(expr x,y,amt) =
    (x*w,y*w) -- ((x+amt)*w,y*w) -- ((x+amt)*w,(y+amt)*w) -- (x*w,(y+amt)*w) -- cycle
  enddef;
  
  elementit.c(btex \element{\textbf{Click!}} etex, 1, 1, push);
  c.width = 2w; c.height = w; c.sw = (0w,1w);
  %c.dx = 15; c.dy = 7; (3w,1.5w) - c.out[0] = (.6w,-1);
  c.borderscale := 2 / 0.9;
  
  label.urt(btex \textsf{POWERED BY} etex scaled 0.7, (0w,2w));
  h := .1w; s := 2.2;

  label(btex \textsf{g} etex scaled s, (.5w,.5w));
  label(btex \textsf{R} etex scaled s, (1.5w,.5w));
  label(btex \textsf{I} etex scaled s, (2.5w,.5w));
  label(btex \textsf{d} etex scaled s, (3.5w,.5w));
  label(btex \textsf{\&} etex scaled (s/2), (3.75w,1.25w));

  pickup pencircle scaled 2;
  drawelement(c);
  drawconnarrow (0.05w,2.1w) {dir 190} .. {right} c.in[0];
  drawconnarrow c.out[0] {right} .. (3.8w,1.85w);
  drawconnarrow (3.8w,1.85w) {dir -100} .. {dir -130} (3.33w,1.35w);
  
  pickup pencircle scaled 0.7;
  draw xbox(1,0,.5);
  draw xbox(1,.75,.25);
  draw xbox(1.25,.5,.25);
  draw xbox(1.5,0,.5);
  draw xbox(1.5,.5,.5);

  draw xbox(3,0,.5);
  draw xbox(3.5,.5,.5);

  draw xbox(3,1.5,.5);
  draw xbox(3.5,1,.5);
  draw xbox(3,1,.25);
  draw xbox(3.25,1.25,.25);

  pickup pencircle scaled 2;
  draw xbox(0,0,1);
  draw xbox(1,0,1);
  draw xbox(2,0,1);
  draw xbox(3,0,1);
  draw xbox(3,1,1);

endfig;


beginfig(3);
  interim linecap := butt;
  interim linejoin := mitered;
  interim miterlimit := 100;
  save scale;
  scale = 1.82;

  z1 = (0,0);
  z2 = (40,18) * scale;
  z3 = (84,8) * scale;
  z4 = (44,-12) * scale;

  z12 = .55[z1,z2];
  z23 = .55[z3,z2];
  z34 = .55[z4,z3];
  z14 = .55[z4,z1];

  z0 = whatever[z12,z34] = whatever[z23,z14];

  z140 = .55[z14,z0];
  z340 = .55[z34,z0];
  z144 = .55[z4,z14];
  z344 = .55[z4,z34];

  z40 = whatever[z340,z144] = whatever[z140,z344];

  z6 = .52[z144,z14];
  z7 = .55[z14,z140];
  z8 = .52[z40,z140];
  z9 = .55[z144,z40];

  z90 = whatever[z6,z8] = whatever[z7,z9];

  pickup pencircle scaled 1.5;
  draw z1 -- z2 -- z3 -- z4 -- cycle;
  pickup pencircle scaled 0.8;
  draw z12 -- z34 -- cycle;
  draw z23 -- z14 -- cycle;
  pickup pencircle scaled 0.6;
  draw z144 -- z340 -- cycle;
  draw z140 -- z344 -- cycle;
  pickup pencircle scaled 0.4;
  draw z6 -- z8 -- cycle;
  draw z7 -- z9 -- cycle;

  pair a,b,c;
  a = .2[.4[z12,z14],z1];
  b = .2[.35[z340,z344],z34];
  c = .5[z90,z40] + (0,1);
  z.out = a + (0,40)*scale;

  connectionpen := pencircle scaled 2;
  path p;
  t = 1.6;
  p := z.out -- a {up} .. tension 1 and t .. .5[a,b]+((0,30)*scale) .. tension t and 1 .. {down} b {up} .. tension 1 and t .. .5[b,c]+((0,15)*scale) .. tension t and 1 .. {down} c;
  miterlimit := 10;
  drawconnarrow p  withcolor white;
  connectionpen := pencircle scaled 1;
  drawconnarrow p;

  velementit.c(btex \element{\textbf{Click!}} etex scaled 0.8, 0, 1, push);
  c.out[0] = z.out;
  drawelement(c);

  elementit.g(btex \textsf{g} etex, 0, 0, push);
  elementit.r(btex \textsf{R} etex, 0, 0, push);
  elementit.i(btex \textsf{I} etex, 0, 0, push);
  elementit.d(btex \textsf{d} etex, 0, 0, push);
  g.width=r.width=i.width=d.width;
  g.e=r.w;r.e=i.w;i.e=d.w;
  g.height=r.height=i.height=d.height=c.height=g.width;

  ypart c.e = ypart g.e;
  g.w = c.e + (15,0);

  label(btex \textsf{\&} etex scaled 0.87, .5[c.e,g.w]);

  drawelement(g,r,i,d);

  pickup pencircle scaled 0.6;
  draw r.n -- r.s -- cycle;
  draw r.e -- r.w -- cycle;
  draw .5[r.w,r.sw] -- .5[r.c,r.s] -- cycle;
  draw .5[r.w,r.c] -- .5[r.sw,r.s] -- cycle;
  draw d.n -- d.s -- cycle;
  draw d.e -- d.w -- cycle;

  label.top(btex \textsf{POWEREd BY} etex scaled 0.6, .5[c.nw,d.ne]);
endfig;


beginfig(4);
  interim linecap := butt;
  interim linejoin := mitered;
  interim miterlimit := 100;
  save scale;
  scale = 1.82;

  velementit.c(btex \element{\fontseries{b}\selectfont{Click!}} etex scaled 0.8, 0, 1, push);

  elementit.g(btex \textsf{g} etex, 0, 0, push);
  elementit.r(btex \textsf{R} etex, 0, 0, push);
  elementit.i(btex \textsf{I} etex, 0, 0, push);
  elementit.d(btex \textsf{d} etex, 0, 0, push);
  g.width=r.width=i.width=d.width;
  g.e=r.w;r.e=i.w;i.e=d.w;
  g.height=r.height=i.height=d.height=c.height=g.width;

  ypart c.e = ypart g.e;
  g.w = c.e + (15,0);

  fixelement(c);
  label(btex \textsf{\&} etex scaled 0.87, .5[c.e,g.w]);
  drawelement(g,r,i,d);

  pickup pencircle scaled 0.6;
  draw r.n -- r.s -- cycle;
  draw r.e -- r.w -- cycle;
  draw .5[r.w,r.sw] -- .5[r.c,r.s] -- cycle;
  draw .5[r.w,r.c] -- .5[r.sw,r.s] -- cycle;
  z1 = .2[.6[r.sw,r.s],r.c];
  draw d.n -- d.s -- cycle;
  draw d.e -- d.w -- cycle;

  label.top(btex \textsf{POWEREd BY} etex scaled 0.6, .5[c.nw,d.ne]);

  path p;
  p := c.out[0] + (-.5,1){dir -60} .. tension 2 and 1 .. .5[c.out[0],r.s]-(0,10) .. tension 1 and 2 .. {dir 60}z1;

  connectionpen := pencircle scaled 2;
  drawconnarrow p withcolor white;
  connectionpen := pencircle scaled 1;
  drawconnarrow p;

  drawelement(c);

endfig;


end

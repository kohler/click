% samp02_processing.mp -- MetaPost graph examples: more fragments
% Eddie Kohler
%
% Copyright (c) 2000-2001 Massachusetts Institute of Technology
% Copyright (c) 2002-2003 International Computer Science Institute
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
\newcommand{\xlabel}[1]{\hbox{%
  \small\itshape\baselineskip6pt\begin{tabular}{@{}l@{}}#1\end{tabular}%
}}%
\newcommand{\fucktt}[1]{{\upshape\ttfamily#1}}%
\begin{document}
etex;
picture emptyqueue; emptyqueue := btex \phantom{\element{Queue}} etex;
picture vemptyqueue; vemptyqueue := emptyqueue rotated 90 xscaled .4;


beginfig(1);
  elementit.in(btex \element{FromDevice} etex, 0, 1, push);
  elementit.n1(btex \element{Null} etex, 1, 1, agnostic_push);
  queueit.q(emptyqueue);
  elementit.n2(btex \element{Null} etex, 1, 1, agnostic_pull);
  elementit.out(btex \element{ToDevice} etex, 1, 0, pull);
  picture dots; dots := btex $\cdots$ etex;
  arrowlen = 80;
  vsp=-8;
  brangle = angle (arrowlen,vsp);
  
  in.s = (0,2);
  q.c = 0.5[in.c,out.c];
  n1.c = 0.25[in.c,out.c];
  n2.c = 0.75[in.c,out.c];
  out.c - in.c = (4arrowlen, 0);
  drawelement(in,n1,q,n2,out);
  
  drawarrow in.e .. n1.w; drawarrow n1.e..q.w; drawarrow q.e..n2.w;
  drawarrow n2.e..out.w;
  
  vardef xxx(expr i, j, y, label) =
    save p, q; picture p; path q;
    if picture label: p := label else: p := label infont defaultfont scaled defaultscale fi;
    p := p shifted (-.5[lrcorner p,llcorner p]+(0,1));
    q := subpath (0.03,0.97) of ((i*arrowlen,y) .. (j*arrowlen,y+vsp));
    drawarrow q;
    draw (p rotated (if j>i: brangle else: -brangle fi)
      shifted .5[(i*arrowlen,y),(j*arrowlen,y+vsp)]);
  enddef;
  
  xxx(0,1,1.5vsp, btex \fucktt{push(p)} etex);
  xxx(1,2,2.5vsp, btex \fucktt{push(p)} etex);
  xxx(2,1,4.8vsp, btex \fucktt{return}\vphantom{\fucktt{p}} etex);
  xxx(1,0,5.8vsp, btex \fucktt{return}\vphantom{\fucktt{p}} etex);
  xxx(4,3,6.9vsp, btex \fucktt{pull()} etex);
  xxx(3,2,7.9vsp, btex \fucktt{pull()} etex);
  xxx(2,3,10.2vsp, btex \fucktt{return p} etex);
  xxx(3,4,11.2vsp, btex \fucktt{return p} etex);
  
  pickup pencircle scaled 2.6;
  for i = 0 upto 4:
    draw (i*arrowlen,0) .. (i*arrowlen,13vsp) withcolor 0.8white;
  endfor;
  
  label.lft(btex \xlabel{\hfill receive\\packet \fucktt{p}} etex, (0,1.5vsp));
  label.rt(btex \xlabel{enqueue \fucktt{p}} etex, (2arrowlen,.5[3.5,4.8]*vsp));
  label.rt(btex \xlabel{ready to\\transmit} etex, (4arrowlen,6.9vsp));
  label.lft(btex \xlabel{\hfill dequeue \fucktt{p}\\\hfill and return it} etex, (2arrowlen,.5[8.9,10.2]*vsp));
  label.rt(btex \xlabel{send \fucktt{p}} etex, (4arrowlen,12.2vsp));
endfig;

beginfig(2);
  vardef waypt(expr q) expr p =
    drawarrow p withpen connectionpen;
    point q*length p of p
  enddef;
  color xcolor; xcolor := .4white;
  pen xpen; xpen := pencircle scaled 1.5;
  vardef drawx(expr z, size, rotation) =
    save p; path p;
    p = (.75size,-size)--(-.75size,size);
    draw p rotated rotation shifted z withpen xpen withcolor xcolor;
    draw p reflectedabout((0,1),(0,-1)) rotated rotation shifted z withpen xpen withcolor xcolor;
  enddef;
  
  elementit.in1(btex \element{FromDevice} etex, 0, 1, push);
  elementit.out1(btex \element{ToDevice} etex, 1, 0, pull);
  elementit.c(btex \element{Counter} etex, 1, 1, agnostic_push);
  c.outpers[0] := agnostic_pull;
  c.w + (-15,20) = in1.e;
  xpart(out1.w - c.e) = 15; ypart(out1.w - 0.5[c.e,in1.e]) = 0;

  elementit.in2(btex \element{FromDevice} etex, 0, 1, push);
  elementit.out2(btex \element{ToDevice} etex, 1, 0, pull);
  queueit.q(emptyqueue);
  ypart in1.s - 32 = ypart in2.n;
  xpart 0.5[in2.w,out2.e] = xpart 0.5[in1.w,out1.e];
  out2.w - q.e = q.w - in2.e = (15,0);
  
  drawelement(in1,in2,out1,c,q,out2);
  
  x0 = xpart q.e; y0 = ypart in1.out[0] - 3;
  z1 = waypt(.32) in1.out[0]{right} ..tension 2.. z0 .. {dir -30}out1.in[0];
  drawx(z1,4,0);
  
  z2 = waypt(.5) in1.out[0]{right} .. {right}c.in[0];
  drawx(z2,4,10);
  
  z3 = waypt(.5) c.out[0]{right} .. {right}out1.in[0];
  drawx(z3,4,60);
  
  drawconnect(in2,0,0,q);
  drawconnect(q,0,0,out2);
  
  drawx(c.c, 18, 90);
endfig;


beginfig(3);
  elementit.p(btex \element{PrioSched} etex, 2, 1, pull);
  z0 = p.in[0]+(-15,3); z1 = p.in[1]+(-15,-3); z2 = p.out[0]+(15,0);
  drawelement(p);
  drawarrow z0 .. {right}p.in[0] withpen connectionpen;
  drawarrow z1 .. {right}p.in[1] withpen connectionpen;
  drawarrow p.out[0]{right} .. z2 withpen connectionpen;
  label.lft(btex \mlabel{high priority} etex, z0);
  label.lft(btex \mlabel{low priority} etex, z1);
  
  elementit.r(btex \element{RoundRobinSched} etex, 3, 1, pull);
  z'0 = r.in[0]+(-15,3); z'1 = r.in[1]+(-13.5,0); z'2 = r.in[2]+(-15,-3); z'3 = r.out[0]+(15,0);
  r.w - p.e = (80,0);
  drawelement(r);
  drawarrow z'0 .. {right}r.in[0] withpen connectionpen;
  drawarrow z'1 .. {right}r.in[1] withpen connectionpen;
  drawarrow z'2 .. {right}r.in[2] withpen connectionpen;
  drawarrow r.out[0]{right} .. z'3 withpen connectionpen;
  z'4 = z'1+(-8,0);
  path circ; circ = reverse (makepath pencircle) xscaled 8 yscaled 35 shifted z'4;
  z'5 = circ intersectiontimes (z'4 .. z'4+(90,150));
  drawarrow (subpath (x'5+.4,length circ) of circ) .. (subpath (0,x'5) of circ);
endfig;

beginfig(4);
  boxjoin(a.s - b.n = (0,8));
  elementit.p1(btex \element{PrioSched} etex, 2, 1, pull);
  z10 = p1.in[0]+(-15,3); z11 = p1.in[1]+(-15,-3); z12 = p1.out[0]+(15,0);
  elementit.p2(btex \element{PrioSched} etex, 2, 1, pull);
  z20 = p2.in[0]+(-15,3); z21 = p2.in[1]+(-15,-3); z22 = p2.out[0]+(15,0);
  elementit.p3(btex \element{PrioSched} etex, 2, 1, pull);
  z30 = p3.in[0]+(-15,3); z31 = p3.in[1]+(-15,-3); z32 = p3.out[0]+(15,0);
  boxjoin();
  elementit.r(btex \element{RoundRobinSched} etex, 3, 1, pull);
  r.w - .5[p1.ne,p3.se] = (30,0);
  drawelement(r,p1,p2,p3);
  drawconnect(p1,0,0,r);
  drawconnect(p2,0,1,r);
  drawconnect(p3,0,2,r);
  drawconnarrow z10 .. {right}p1.in[0]; drawconnarrow z11 .. {right}p1.in[1];
  drawconnarrow z20 .. {right}p2.in[0]; drawconnarrow z21 .. {right}p2.in[1];
  drawconnarrow z30 .. {right}p3.in[0]; drawconnarrow z31 .. {right}p3.in[1];
  drawconnarrow r.out[0]{right} .. r.out[0]+(15,0);
endfig;

end

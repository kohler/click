% samp01_examples.mp -- MetaPost graph examples: configuration fragments
% Eddie Kohler
%
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
\begin{document}
etex;
picture emptyqueue; emptyqueue := btex \phantom{\elementlabel{Queue}} etex;
picture vemptyqueue; vemptyqueue := emptyqueue rotated 90 xscaled .4;


%% 1
%% A sample element.
beginfig(1);
  elementit.samp(btex \element{Tee(2)} etex, 1, 2, push);
  drawelement(samp);
endfig;


%% 2
%% A sample element, with labels.
beginfig(2);
  elementit.samp(btex \element{Tee(2)} etex, 1, 2, push);
  samp.c = (0,0);
  drawelement(samp);
  
  z.in = (-50,ypart samp.in[0]);
  drawarrow z.in -- 0.9[z.in,samp.in[0]] dashed evenly scaled .5;
  label.lft(btex input port etex scaled 0.8, z.in);
  
  z.out = (50,ypart 0.5[samp.out[0],samp.out[1]]);
  drawarrow z.out -- 0.9[z.out,samp.out[0]] dashed evenly scaled .5;
  drawarrow z.out -- 0.9[z.out,samp.out[1]] dashed evenly scaled .5;
  label.rt(btex output ports etex scaled 0.8, z.out);
  
  z.type = (-4.5, 24);
  drawarrow z.type -- (x.type,5.1) dashed evenly scaled .5;
  label.top(btex element class etex scaled 0.8, z.type);
  
  z.config = (6.5, -22);
  drawarrow z.config -- (x.config,-5.8) dashed evenly scaled .5;
  label.bot(btex conf{}iguration string etex scaled 0.8, z.config);
endfig;


%% 3
%% A very simple router.
beginfig(3);
  boxjoin(a.se + (15,0) = b.sw);
  elementit.src(btex \element{FromDevice(eth0)} etex, 0, 1, push);
  elementit.strip(btex \element{Counter} etex, 1, 1, agnostic_push);
  elementit.sink(btex \element{Discard} etex, 1, 0, push);
  drawelement(src,strip,sink);
  drawconnect(src,0,0,strip);
  drawconnect(strip,0,0,sink);
endfig;


%% 4
%% A more complex router fragment.
beginfig(4);
  % Left SFQ
  velementit.hashl(btex \element{HashSwitch(...)} etex, 1, 2, push);
  vqueueit.q1l(vemptyqueue);
  vqueueit.q2l(vemptyqueue);
  velementit.rrl(btex \element{RoundRobinSched} etex, 2, 1, pull);
  rrl.in[0] - q1l.out[0] = rrl.in[1] - q2l.out[0] = (0,-10);
  .5[q1l.n,q2l.n] - hashl.s = (0,-10);

  % Right SFQ
  velementit.hashr(btex \element{HashSwitch(...)} etex, 1, 2, push);
  vqueueit.q1r(vemptyqueue);
  vqueueit.q2r(vemptyqueue);
  velementit.rrr(btex \element{RoundRobinSched} etex, 2, 1, pull);
  rrr.in[0] - q1r.out[0] = rrr.in[1] - q2r.out[0] = (0,-10);
  .5[q1r.n,q2r.n] - hashr.s = (0,-10);
  
  %z.fairlabel = .5[hash.ne,rr.se] + (8,0);
  %label(btex \element{StochasticFairQueue} etex rotated 90, z.fairlabel);
  %picture fairq; fairq := currentpicture; currentpicture := nullpicture;

  % Rest of system
  velementit.arpq(btex \element{ARPQuerier} etex, 2, 1, push);
  velementit.red(btex \element{RED} etex, 1, 1, agnostic_push);
  velementit.filt(btex \quad\element{Classif{}ier(...)}\quad\lower2.3ex\null etex, 1, 2, push);
  velementit.prio(btex \element{PrioSched} etex, 2, 1, pull);
  velementit.t1(btex \element{ToDevice} etex, 1, 0, pull);
  velementit.t2(btex \element{ToDevice} etex, 1, 0, pull);
  
  red.s - filt.n = arpq.s - red.n = (0,10);
  filt.s - .5[hashl.n,hashr.n] = .5[rrl.s,rrr.s] - prio.n = (0,18);
  rrr.w - rrl.e = (10,0);  
  prio.s - .5[t1.n,t2.n] = (0,18);
  t2.w - t1.e = (16,0);
  
  drawelement(arpq,red,filt,rrl,rrr,hashl,q1l,q2l,hashr,q1r,q2r,prio,t1,t2);

  drawconnarrow arpq.in[0]+(0,10) .. arpq.in[0];
  drawconnarrow arpq.in[1]+(0,10) .. arpq.in[1];
  drawconnect(arpq,0,0,red);
  drawconnect(red,0,0,filt);
  drawconnect(filt,0,0,hashl);
  drawconnect(filt,1,0,hashr);
  drawconnect(rrl,0,0,prio);
  drawconnect(rrr,0,1,prio);
  
  drawconnect(hashl,0,0,q1l);
  drawconnect(hashl,1,0,q2l);
  drawconnect(q1l,0,0,rrl);
  drawconnect(q2l,0,1,rrl);
  drawconnect(hashr,0,0,q1r);
  drawconnect(hashr,1,0,q2r);
  drawconnect(q1r,0,0,rrr);
  drawconnect(q2r,0,1,rrr);

  drawarrow prio.out[0]-(1,0){down} .. {down}t1.in[0] withpen connectionpen;
  drawarrow prio.out[0]+(1,0){down} .. {down}t2.in[0] withpen connectionpen;

  label.top(btex \portlabel{high priority} etex scaled 1.22, filt.out[0]+(0,2));
  label.top(btex \portlabel{low priority} etex scaled 1.22, filt.out[1]+(0,2));
endfig;


%% 5
%% A simple configuration with some RED elements.
beginfig(5);
  picture p; p := btex \element{Classif{}ier(...)\quad~~~~~~~~~~~~~~} etex;
  elementit.f(p, 1, 3, push);
  
  elementit.red0(btex \element{RED(p1, ...)} etex, 1, 1, agnostic_push);
  elementit.red1(btex \element{RED(p2, ...)} etex, 1, 1, agnostic_push);
  elementit.red2(btex \element{RED(p3, ...)} etex, 1, 1, agnostic_push);
  
  queueit.q(emptyqueue);
  
  f.e - red1.w = (-15,0);
  red1.n = 0.5[red0.n,red2.n];
  red1.n - red0.s = red2.n - red1.s = (0,-4);
  red1.e - q.w = (-15,0);
  
  drawelement(red0,red1,red2,f,q);
  
  drawconnect(f,0,0,red0);
  drawconnect(f,1,0,red1);
  drawconnect(f,2,0,red2);
  drawconnectna(red0,0,0,q);
  drawconnectna(red2,0,0,q);
  drawarrow f.in[0]-(12,0) .. f.in[0] withpen connectionpen;
  drawarrow q.out[0] .. q.out[0]+(12,0) withpen connectionpen;
  
  interim ahangle := 70;
  drawconnect(red1,0,0,q);
  
  label.lft(btex \portlabel{high priority~} etex scaled 1.2, f.out[0]-(3,0));
  label.lft(btex \portlabel{medium priority~} etex scaled 1.2, f.out[1]-(3,0));
  label.lft(btex \portlabel{low priority~} etex scaled 1.2, f.out[2]-(3,0));
endfig;


%% 6
%% Another RED router fragment, showing painted elements.
beginfig(6);
  elementit.red(btex \element{RED} etex, 1, 1, agnostic_push);
  elementit.filt(btex \element{Classif{}ier} etex, 1, 3, push);
  queueit.q1(emptyqueue);
  elementit.trash(btex \element{Discard} etex, 1, 0, push);
  elementit.null(btex \element{Strip} etex, 1, 1, agnostic_push);
  queueit.q2(emptyqueue);
  elementit.shape(btex $\cdots$ etex, 1, 1, pull_to_push);
  queueit.q3(emptyqueue);
  
  q1.sw - trash.nw = trash.sw - null.nw = (0,10);
  shape.w - q1.e = q3.w - shape.e = q2.w - null.e = (10,0);
  .5[q1.w,null.w] - filt.e = (10,0);
  filt.w - red.e = (10,0);
  y.last = ypart q2.out[0];
  
  fixelement(red,filt,trash,shape,null,q1,q2,q3);
  fill bpath.filt withcolor 0.8white;
  fill bpath.q1 withcolor 0.8white;
  fill bpath.q2 withcolor 0.8white;
  fill bpath.null withcolor 0.8white;
  fill bpath.trash withcolor 0.8white;
  drawelement(red,filt,trash,shape,null,q1,q2,q3);
  
  drawconnect(red,0,0,filt);
  drawconnect(filt,0,0,q1);
  drawconnect(filt,1,0,trash);
  drawconnect(filt,2,0,null);
  drawconnect(null,0,0,q2);
  drawconnect(q1,0,0,shape);
  drawconnect(shape,0,0,q3);
  drawconnarrow red.in[0]-(15,0) .. red.in[0];
  drawconnarrow q2.out[0]{right} .. {right}q2.out[0]+(15,0);
  drawconnarrow q3.out[0]{right} .. {right}q3.out[0]+(15,0);
endfig;


%% 7
%% A diffserv configuration fragment.
beginfig(7);
  elementit.cl(btex \element{~Classif{}ier(...)~}\lower1.8ex\null etex rotated 90, 1, 4, push);
  elementit.m0(btex \element{RatedSplitter(7500)} etex, 1, 2, push);
  elementit.disc(btex \element{Discard} etex, 1, 0, push);
  elementit.sh(btex \element{Shaper(10000)} etex, 1, 1, pull);
  elementit.m2(btex \element{RatedSplitter(12500)} etex, 1, 2, push);
  elementit.set(btex \element{SetIPDSCP(D)} etex, 1, 1, push);
  queueit.qa(emptyqueue);
  queueit.qb(emptyqueue);
  queueit.qc(emptyqueue);
  queueit.qd(emptyqueue xscaled 1.6);
  elementit.rr(btex \element{RoundRobinSched} etex rotated 90, 3, 1, pull);
  elementit.prio(btex \element{PrioSched} etex rotated 90, 2, 1, pull);

  ypart .5[disc.w,qa.w] = ypart m0.e; ypart (qa.sw - disc.nw) = 6;
  sh.w = qb.e + (12,0);
  ypart .5[set.w,qc.w] = ypart m2.e; ypart (qc.sw - set.nw) = 6;
  xpart (disc.w - m0.e) = xpart (set.w - m2.e) = 12;
  ypart (disc.s - qb.n) = 8;
  ypart (qb.s - qc.n) = 6;
  ypart (set.s - qd.n) = 8;
  xpart qa.w = xpart qc.w = xpart qd.w = xpart set.e + 20;
  xpart m0.w = xpart qb.w = xpart m2.w = xpart cl.e + 24;
  ypart cl.out[1] = ypart qb.in[0];
  rr.w = (xpart qc.e,ypart .5[qa.n,qc.s]) + (15,0);
  prio.w = (xpart rr.e,ypart .5[qd.s,qa.n]) + (18,0);

  fixelement(cl,m2,m0,disc,sh,qa,qb,qc,qd,rr,prio,set);
    
  drawconnarrow m0.out[0]{right} .. {right}(xpart disc.in[0],ypart qa.in[0]) .. qa.in[0];
  drawconnarrow m2.out[0]{right} .. {right}(xpart set.in[0],ypart qc.in[0]) .. qc.in[0];
  drawconnarrow cl.out[3]{right} .. tension 1.5 .. {right}(xpart m2.in[0],ypart qd.in[0]) .. qd.in[0];
  drawconnect(set,0,0,qd);
  
  drawelement(cl,m0,disc,sh,m2,qa,qb,qc,qd,rr,prio,set);

  drawconnarrow cl.in[0]-(18,0) .. cl.in[0];
  drawconnarrow prio.out[0] .. prio.out[0]+(18,0);
  
  drawconnect(cl,0,0,m0);
  drawconnect(m0,1,0,disc);
  
  drawconnect(cl,1,0,qb);
  drawconnect(qb,0,0,sh);
  drawconnect(sh,0,1,rr);

  drawconnect(cl,2,0,m2);
  drawconnect(qc,0,2,rr);
  drawconnect(m2,1,0,set);
  
  drawconnect(qa,0,0,rr);
  drawconnect(qc,0,2,rr);
  drawconnect(rr,0,0,prio);
  drawconnect(qd,0,1,prio);

  label.lft(btex \portlabel{{A}} etex, cl.out[0]-(4,0));
  label.lft(btex \portlabel{{B}} etex, cl.out[1]-(4,0));
  label.lft(btex \portlabel{{C}} etex, cl.out[2]-(4,0));
  label.lft(btex \portlabel{{D}} etex, cl.out[3]-(4,0));
endfig;


%% 8
%% A simple compound element.
vardef drawemptyqueued(suffix $) =
  save i; interim linecap := squared; i := .052*$.width;
  forever:
    draw ($.ne -- $.se) shifted (-i,0) withpen currentpen scaled 0.25 withcolor 0.8white;
    i := i + .052*$.width; exitunless i < .94*$.width;
  endfor;
  draw ($.nw -- $.ne -- $.se -- $.sw);
enddef;
vardef emptyqueueit@#(expr s) =
  _elementit.@#(s, 1, 1, push_to_pull, false, false);
  elemdraw_@# := "drawemptyqueued";
enddef;

beginfig(8);
  % define components of compound
  elementit.hash(btex \elementlabel{HashSwitch(...)} etex, 1, 2, push);
  queueit.q1(emptyqueue);
  queueit.q2(emptyqueue);
  elementit.rr(btex \elementlabel{RoundRobinSched} etex, 2, 1, pull);

  % arrange components relative to one another
  0.5[q1.w,q2.w] - hash.e = (15,0);
  rr.w - .5[q1.e,q2.e] = (15,0);
  q1.se - q2.ne = q1.sw - q2.nw = (0,8);

  % define compound itself, using the bounding box of the component elements
  emptyqueueit.c(elementbbox(hash,q1,q2,rr));
  % ports & border are bigger, add a bit of space around components
  c.portscale := c.borderscale := 1.8; c.dx = 20; c.dy = 10;
  % link to position of component elements
  compoundelementlink.c(hash,q1,q2,rr);

  % draw it all
  drawelement(c);
  fillelement(hash,q1,q2,rr)(white);
  drawelement(hash,q1,q2,rr);
  drawconnect(hash,0,0,q1);
  drawconnect(q1,0,0,rr);
  drawconnect(hash,1,0,q2);
  drawconnect(q2,0,1,rr);

  drawconnarrow portinteriorin(c,0) .. hash.in[0];
  drawconnarrow rr.out[0] .. portinteriorout(c,0);

  drawconnarrow c.in[0]-(20,0) .. c.in[0] withpen connectionpen scaled 1.5;
  drawconnarrow c.out[0] .. c.out[0]+(20,0) withpen connectionpen scaled 1.5;
endfig;


end

% samp03_routers.mp -- MetaPost graph examples: simple routers
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
\begin{document}
etex;
picture emptyqueue; emptyqueue := btex \phantom{\elementlabel{Queue}} etex;
picture vemptyqueue; vemptyqueue := emptyqueue rotated 90 xscaled .4;


%% 1
%% A simple firewall configuration.
%% (See samp05_other.mp, figure 1 for the corresponding network diagram.)
beginfig(1);
  velementit.fd0(btex \element{FromDevice(eth0)} etex, 0, 1, push);
  velementit.fd1(btex \element{FromDevice(eth1)} etex, 0, 1, push);
  velementit.c0(btex \element{\quad Classif{}ier(...)\quad}\lower2.4ex\null etex, 1, 2, push);
  velementit.c1(btex \element{\quad Classif{}ier(...)\quad}\lower2.4ex\null etex, 1, 2, push);
  velementit.filt0(btex \element{IPFilter(...)} etex, 1, 1, push);
  velementit.filt1(btex \element{IPFilter(...)} etex, 1, 1, push);
  velementit.ch0(btex \melementlabel{CheckIPHeader\\(14, ...)} etex, 1, 1, agnostic_push);
  velementit.ch1(btex \melementlabel{CheckIPHeader\\(14, ...)} etex, 1, 1, agnostic_push);
  vqueueit.q0(vemptyqueue);
  vqueueit.q1(vemptyqueue);
  velementit.td0(btex \element{ToDevice(eth0)} etex, 1, 0, pull);
  velementit.td1(btex \element{ToDevice(eth1)} etex, 1, 0, pull);

  fd0.s - c0.n = fd1.s - c1.n = (0,12);
  c0.out[0] - ch0.n = c1.out[0] - ch1.n = (5,12);
  ch0.s - filt0.n = ch1.s - filt1.n = (0,12);
  xpart q0.n = xpart fd0.n; xpart q1.n = xpart fd1.n;
  ypart q0.n = ypart q1.n = ypart filt0.s - 18;
  filt0.width = filt1.width = ch0.width = ch1.width;
  
  q0.s - td1.n = q1.s - td0.n = (0,12);

  ypart fd1.w = ypart fd0.w; xpart ch1.w - xpart fd0.e = 5;

  drawelement(fd0,fd1,td0,td1,c0,c1,ch0,ch1,filt0,filt1,q0,q1);
  drawconnect(fd0,0,0,c0);
  drawconnect(c0,0,0,ch0);
  drawconnect(ch0,0,0,filt0);
  drawconnect(filt0,0,0,q0);
  drawconnect(q0,0,0,td1);
  drawconnarrow c0.out[1]{down} .. tension 1.2 .. filt0.se+(0,-5) .. tension 1.5 .. {down}q0.in[0];
  drawconnect(fd1,0,0,c1);
  drawconnect(c1,0,0,ch1);
  drawconnect(ch1,0,0,filt1);
  drawconnect(filt1,0,0,q1);
  drawconnect(q1,0,0,td0);
  drawconnarrow c1.out[1]{down} .. tension 1.2 .. filt1.se+(0,-5) .. tension 1.5 .. {down}q1.in[0];

  label.top(btex \portlabel{IP} etex scaled 1.2, c0.out[0]+(0,2));
  label.top(btex \portlabel{other} etex scaled 1.2, c0.out[1]+(0,2));
  label.top(btex \portlabel{IP} etex scaled 1.2, c1.out[0]+(0,2));
  label.top(btex \portlabel{other} etex scaled 1.2, c1.out[1]+(0,2));
endfig;


%% 2
%% A UDP generator.
beginfig(2);
  velementit.fd(btex \element{FromDevice} etex, 0, 1, push);
  velementit.c(btex \element{\quad Classif{}ier(...)\quad}\lower2.4ex\null etex, 1, 2, push);
  velementit.arpr(btex \element{ARPResponder(...)} etex, 1, 1, agnostic_push);
  velementit.rs(btex \melementlabel{RatedSource\\(data, rate, count)} etex, 0, 1, agnostic_push);
  velementit.ipe(btex \melementlabel{UDPIPEncap\\(saddr, sport, daddr, dport)} etex, 1, 1, agnostic_push);
  velementit.ethe(btex \melementlabel{EtherEncap(0x0800, ...)} etex, 1, 1, agnostic_push);
  vqueueit.q(vemptyqueue yscaled 0.8);
  velementit.ctr(btex \element{Counter} etex, 1, 1, agnostic_pull);
  velementit.td(btex \element{ToDevice} etex, 1, 0, pull);

  fd.s - c.n = (0,10); c.out[0] - arpr.n = (0,18);

  rs.width = ipe.width = ethe.width;
  rs.s - ipe.n = ipe.s - ethe.n = (0,10);
  q.n = .5[ethe.s,arpr.s] - (0,20);
  q.s - ctr.n = ctr.s - td.n = (0,10);

  xpart (arpr.w - ethe.e) = 15; ypart ethe.s = ypart arpr.s;
  
  drawelement(fd,c,arpr,ipe,rs,ethe,q,ctr,td);
  
  drawconnect(fd,0,0,c);
  drawconnect(c,0,0,arpr);
  drawconnarrow arpr.out[0]{down} .. tension 1.4 .. {down}q.in[0];
  drawconnect(rs,0,0,ipe);
  drawconnect(ipe,0,0,ethe);
  drawconnarrow ethe.out[0]{down} .. tension 1.4 .. {down}q.in[0];
  drawconnect(q,0,0,ctr);
  drawconnect(ctr,0,0,td);
  
  label.top(btex \portlabel{ARP queries} etex scaled 1.2, c.out[0]+(0,2));
  label.top(btex \portlabel{other} etex scaled 1.2, c.out[1]+(0,2));
  drawconnarrow c.out[1]{down} .. c.out[1]+(10,-10);
  label.rt(btex to Linux etex scaled 0.8, c.out[1]+(10,-9));
endfig;


%% 3
%% An Ethernet switch.
beginfig(3);
  velementit.r0(btex \element{FromDevice(eth0)} etex, 0, 1, push);
  velementit.r1(btex \element{FromDevice(eth1)} etex, 0, 1, push);

  %picture p; p := btex \element{Classif{}ier(14/4242, -)}\lower4ex\null etex;
  picture p; p := btex \element{Classif{}ier(...)}\lower4ex\null etex;
  velementit.f0(p, 1, 2, push);
  velementit.f1(p, 1, 2, push);

  vqueueit.q0(vemptyqueue);
  vqueueit.q1(vemptyqueue);
  velementit.w0(btex \element{ToDevice(eth0)} etex, 1, 0, pull);
  velementit.w1(btex \element{ToDevice(eth1)} etex, 1, 0, pull);
  
  velementit.s(btex \element{EtherSwitch} etex, 2, 2, push);
  velementit.in(btex \element{s1 :: Suppressor} etex, 2, 2, agnostic_push);
  velementit.out(btex \element{s2 :: Suppressor} etex, 2, 2, agnostic_push);
  velementit.st(btex \element{EtherSpanTree(s1, s2, ...)} etex, 2, 2, agnostic_push);

  f1.width = r1.width = f0.width = r0.width = kw0.width = kw1.width;
  in.width = out.width = s.width;
  in.s - s.n = s.s - out.n = (0,10);

  r1.w - r0.e = (10,0); w1.w - w0.e = (whatever,0);
  xpart r0.c = xpart w0.c; xpart r1.c = xpart w1.c;
  xpart w1.c = xpart q1.c;
  r1.s - f1.n = r0.s - f0.n = q0.s - w0.n = (0,8) = q1.s - w1.n;
  s.w - st.e = (20,0);
  ypart (f0.s - in.n) = 30;
  ypart (out.s - q0.n) = 30;
  xpart 0.5[s.e,st.w] = xpart 0.5[f1.e,f0.w] = xpart 0.5[w1.e,w0.w];

  fixelement(r0,st,f0,f1,r1,q0,q1,out,in,s,w0,w1);
  drawelement(r0,r1,f0,f1,q0,q1,s,st,in,out,w0,w1);
  
  drawconnect(r0, 0, 0, f0);
  drawconnect(r1, 0, 0, f1);

  drawarrow f0.out[1]{down} .. tension 2 and 1.5 .. {down}in.in[0] withpen connectionpen;
  drawconnect(in, 0, 0, s);
  drawconnect(s, 0, 0, out);
  drawarrow out.out[0]{down} .. tension 2 and 1.5 .. {down}q0.in[0] withpen connectionpen;
  
  drawconnect(f1, 1, 1, in);
  drawconnect(in, 1, 1, s);
  drawconnect(s, 1, 1, out);
  drawconnect(out, 1, 0, q1);

  drawconnect(f0, 0, 0, st);
  drawconnect(st, 0, 0, q0);
  drawarrow f1.out[0]{down} .. tension 2 and 1.5 .. {down}st.in[1] withpen connectionpen;
  drawarrow st.out[1]{down} .. tension 2 and 1 .. out.sw-(5,5) .. tension 1 and 1.5 .. {down}q1.in[0] withpen connectionpen;

  drawconnect(q0, 0, 0, w0);
  drawconnect(q1, 0, 0, w1);
  
  % Draw labels
  defaultfont := "Helvetica";
  defaultscale := .7;
  label.top(btex \portlabel{802.1d\\protocol} etex scaled 1.25, f0.out[0]+(0,2));
  label.top(btex \portlabel{other} etex scaled 1.25, f0.out[1]+(0,2));
  label.top(btex \portlabel{802.1d\\protocol} etex scaled 1.25, f1.out[0]+(0,2));
  label.top(btex \portlabel{other} etex scaled 1.25, f1.out[1]+(0,2));
endfig;


%% 4
%% A proxy configuration.
beginfig(4);
  velementit.c(btex \element{\quad IPClassif{}ier(...)\quad}\lower2.4ex\null etex, 1, 2, push);
  velementit.ipr(btex \melementlabel{IPRewriter\\(pattern~~FAKE 1024-65535 LOCAL 8000~~0~1,\hfill\null\\\quad drop)\hfill\null} etex, 2, 2, push);
  velementit.tol(btex \element{ToLinux} etex, 1, 0, push);
  velementit.fl(btex \melementlabel{FromLinux\\(fdev0, FAKE)} etex, 0, 1, push);
  velementit.flc(btex \melementlabel{\quad Classif{}ier(...)\quad}\lower 2.4ex\null etex, 1, 2, push);
  velementit.arpr(btex \element{ARPResponder} etex, 1, 1, agnostic_push);
  velementit.st(btex \element{Strip(14)} etex, 1, 1, agnostic_push);
  velementit.ch(btex \element{CheckIPHeader} etex, 1, 1, agnostic_push);
  velementit.eth(btex \element{EtherEncap(0x0800, ...)} etex, 1, 1, agnostic_push);

  ipr.out[0] - eth.n = (0,12);
  eth.s - tol.n = (0,8);
  fl.s - flc.n = (0,8);
  flc.out[0] - arpr.n = (18,10);
  (xpart flc.out[1], ypart arpr.s) - (0,10) = st.in[0];
  st.s - ch.n = (0,8);
  ch.s - ipr.in[1] = (0,12);
  st.width = ch.width;
  c.out[0] - (xpart ipr.in[0], ypart fl.n) = (0,28);
  
  drawelement(c,ipr,tol,fl,flc,arpr,ch,st,eth);
  
  drawconnect(c,0,0,ipr);
  drawconnect(fl,0,0,flc);
  drawconnect(flc,0,0,arpr);
  drawconnect(flc,1,0,st);
  drawconnect(st,0,0,ch);
  drawconnect(ch,0,1,ipr);
  drawconnect(ipr,0,0,eth);
  drawconnect(eth,0,0,tol);

  label.top(btex \portlabel{Web} etex scaled 1.2, c.out[0]+(0,2));
  label.top(btex \portlabel{other} etex scaled 1.2, c.out[1]+(0,2));
  label.top(btex \portlabel{ARP queries} etex scaled 1.2, flc.out[0]+(0,2));
  label.top(btex \portlabel{IP} etex scaled 1.2, flc.out[1]+(0,2));
  drawconnarrow c.out[1]{down} .. c.out[1]+(12,-12);
  label.rt(btex to \element{ARPQuerier} etex scaled 0.8, c.out[1]+(12,-11));
  drawconnarrow c.in[0]+(0,12) .. c.in[0];
  label.top(btex from \element{IPFragmenter} etex scaled 0.8, c.in[0]+(0,11));
  drawconnarrow arpr.out[0]{down} .. arpr.out[0]+(-12,-12);
  label.lft(btex to Linux etex scaled 0.8, arpr.out[0]+(-12,-11));
  %drawconnarrow ipr.out[1]{down} .. ipr.out[1]+(12,-12);
  %label.rt(btex to \element{StaticIPLookup} etex scaled 0.8, ipr.out[1]+(12,-11));
  
  z.crud = (xpart ipr.e + 10, ypart c.in[0] + 11);
  drawconnarrow ipr.out[1]{down} .. (.5[x.crud, xpart ipr.out[1]], ypart ipr.out[1] - 30){right} .. tension 1.5 .. {up}z.crud;
  %drawconnarrow ipr.out[1]{down} .. ipr.se + (6,-6) .. {up}z.crud;
  label.top(btex to \element{StaticIPLookup} etex scaled 0.8, z.crud);
endfig;


end

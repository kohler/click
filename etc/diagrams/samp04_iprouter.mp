% samp04_iprouter.mp -- MetaPost graph examples: the glorious IP router
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
\begin{document}
etex;
picture emptyqueue; emptyqueue := btex \phantom{\elementlabel{Queue}} etex;
picture vemptyqueue; vemptyqueue := emptyqueue rotated 90 xscaled .4;
color defaultecolor; defaultecolor := white;

%% 1
%% Our most impressive figure: The uniprocessor Click IP router.
%% Uncomment the following line for a color figure.
%defaultecolor := (191/255,255/255,255/255);

vardef icmperrorit@#(expr p) =
  save defaultscale; defaultscale := 0.6;
  elementit.@#(p scaled 0.6, 1, 1, agnostic_push);
  @#.dy = 4; @#.portscale := 0.8;
enddef;

beginfig(1);
  
  % work on IP path first

  vardef ipoutputpath(suffix $)(expr cppic, gwpic, fixsrcpic) =
    boxjoin(a.s - b.n = (0,8); a.width = b.width);
    
    velementit.dropbroadcasts$(btex \element{DropBroadcasts} etex, 1, 1, agnostic_push);
    velementit.cp$(cppic, 1, 3, push);
    killoutput(cp$,0);
    velementit.gio$(gwpic, 1, 3, agnostic_push);
    killoutput(gio$,0); gio$.outpers[1] := push;
    velementit.fixipsrc$(fixsrcpic, 1, 1, agnostic_push);
    velementit.dt$(btex \element{DecIPTTL} etex, 1, 3, agnostic_push);
    killoutput(dt$,0); dt$.outpers[1] := push;
    velementit.fr$(btex \element{IPFragmenter(1500)} etex, 1, 3, push);
    killoutput(fr$,0); fr$.outpers[1] := push;

    boxjoin(a.width=b.width);
    icmperrorit.icmp$a(btex \melementlabel{ICMPError\\redirect} etex);
    icmperrorit.icmp$b(btex \melementlabel{ICMPError\\bad param} etex);
    icmperrorit.icmp$c(btex \melementlabel{ICMPError\\TTL expired} etex);
    icmperrorit.icmp$d(btex \melementlabel{ICMPError\\must frag} etex);
    icmp$a.w = cp$.se + (10,-4);
    icmp$b.w = gio$.se + (10,-4);
    icmp$c.w = dt$.se + (10,-4);
    icmp$d.w = fr$.se + (10,-4);
    icmp$c.dx=4;
  enddef;

  ipoutputpath(A, btex \element{PaintTee(1)} etex,
    btex \element{IPGWOptions(1.0.0.1)} etex,
    btex \element{FixIPSrc(1.0.0.1)} etex);
  ipoutputpath(B, btex \element{PaintTee(2)} etex,
    btex \element{IPGWOptions(2.0.0.1)} etex,
    btex \element{FixIPSrc(2.0.0.1)} etex);
  xpart(fr.B.w) - xpart(icmp.A.a.e) = 25; ypart(cp.A.n) = ypart(cp.B.n);

  boxjoin(a.s - b.n = (0,8));
  velementit.arpq.A(btex \element{ARPQuerier(1.0.0.1, ...)} etex, 2, 1, push);
  vqueueit.outq.A(vemptyqueue);
  velementit.todev.A(btex \element{ToDevice(eth0)} etex, 1, 0, pull);
  
  boxjoin(a.s - b.n = (0,8));
  velementit.arpq.B(btex \element{ARPQuerier(2.0.0.1, ...)} etex, 2, 1, push);
  vqueueit.outq.B(vemptyqueue);
  velementit.todev.B(btex \element{ToDevice(eth1)} etex, 1, 0, pull);

  fr.A.s - arpq.A.n = fr.B.s - arpq.B.n = (0,48);
  
  %

  boxjoin();
  velementit.route(btex \element{StaticIPLookup(...)} etex, 1, 3, push);
  ypart(route.s - dropbroadcasts.A.n) = 36;

  % ip input

  velementit.strip(btex \element{Strip(14)} etex, 1, 1, agnostic_push);
  velementit.checkheader(btex \element{CheckIPHeader(...)} etex, 1, 1, agnostic_push);
  strip.width = checkheader.width;
  strip.s - checkheader.n = (0,8);

  checkheader.s - route.n = (0,20);
  xpart(strip.c) = xpart(0.5[gio.A.c, gio.B.c]);
  
  % input interfaces

  boxjoin();
  velementit.from.A(btex \element{FromDevice(eth0)} etex, 0, 1, push);
  velementit.c.A(btex \element{~~~~~~~~~Classif{}ier(...)~~~~~~~~~}\lower3.8ex\null etex,
    1, 3, push);
  velementit.paint.A(btex \element{Paint(1)} etex, 1, 1, agnostic_push);
  
  velementit.from.B(btex \element{FromDevice(eth1)} etex, 0, 1, push);
  velementit.c.B(btex \element{~~~~~~~~~Classif{}ier(...)~~~~~~~~~}\lower3.8ex\null etex,
    1, 3, push);
  velementit.paint.B(btex \element{Paint(2)} etex, 1, 1, agnostic_push);

  ypart(paint.A.s - strip.n) = ypart(paint.B.s - strip.n) = 20;
  from.A.s - c.A.n = from.B.s - c.B.n = (0,10);
  c.A.out[2] - paint.A.in[0] = c.B.out[2] - paint.B.in[0] = (0,60);

  % ARP responders, queriers
  
  velementit.arpr.A(btex \melementlabel{ARPResponder\\(1.0.0.1 ...)} etex scaled 0.7, 1, 1, agnostic_push);
  velementit.arpr.B(btex \melementlabel{ARPResponder\\(2.0.0.1 ...)} etex scaled 0.7, 1, 1, agnostic_push);
  c.A.out[0] - 0.7[arpr.A.nw,arpr.A.ne] = (0,8);
  c.B.out[0] - 0.7[arpr.B.nw,arpr.B.ne] = (0,8);

  xpart c.A.c = xpart gio.A.c + 16;
  xpart c.B.c = xpart gio.B.c;
  
  %
  
  fixelement(gio.A,fr.A,dt.A,fixipsrc.A,cp.A,dropbroadcasts.A,
    icmp.A.a,icmp.A.b,icmp.A.c,icmp.A.d,
    gio.B,fr.B,dt.B,fixipsrc.B,cp.B,dropbroadcasts.B,
    icmp.B.a,icmp.B.b,icmp.B.c,icmp.B.d,
    from.A,from.B,paint.A,paint.B,c.A,c.B,
    arpq.A,outq.A,todev.A,arpq.B,outq.B,todev.B,
    arpr.A,arpr.B,
    route,checkheader,strip);

  fillelement(from.A,from.B,c.A,c.B,
    arpq.A,outq.A,todev.A,arpq.B,outq.B,todev.B,
    arpr.A,arpr.B)(defaultecolor);
  drawelement(from.A,from.B,c.A,c.B,
    arpq.A,outq.A,todev.A,arpq.B,outq.B,todev.B,
    arpr.A,arpr.B);

  drawconnect(from.A,0,0,c.A);
  drawconnect(c.A,2,0,paint.A);
  drawconnarrow paint.A.out[0]{down} .. tension 1.2 .. {down}strip.in[0];
  drawconnarrow paint.B.out[0]{down} .. tension 2.5 .. {down}strip.in[0];
  drawconnect(from.B,0,0,c.B);
  drawconnect(c.B,2,0,paint.B);
  
  drawconnect(strip,0,0,checkheader);
  drawconnect(checkheader,0,0,route);

  drawarrow route.out[1]{down} .. tension 2 .. {down}dropbroadcasts.A.in[0] withpen connectionpen;
  drawarrow route.out[0]{down} .. route.out[0]-(10,10) withpen connectionpen;
  drawconnect(dropbroadcasts.A,0,0,cp.A);
  drawconnect(cp.A,0,0,gio.A);
  drawconnect(gio.A,0,0,fixipsrc.A);
  drawconnect(fixipsrc.A,0,0,dt.A);
  drawconnect(dt.A,0,0,fr.A);

  drawconnect(route,2,0,dropbroadcasts.B);
  drawconnect(dropbroadcasts.B,0,0,cp.B);
  drawconnect(cp.B,0,0,gio.B);
  drawconnect(gio.B,0,0,fixipsrc.B);
  drawconnect(fixipsrc.B,0,0,dt.B);
  drawconnect(dt.B,0,0,fr.B);

  vardef connecticmp(suffix $,#,##,$$) =
    drawarrow $.out[#]{down} .. tension 2 and 1 .. {right}$$.in[##];
    draw $$.out[0]{right} .. tension 1 and 1 .. {up}($$.out[0] + (8,8));
  enddef;
  
  pickup connectionpen;
  connecticmp(cp.A,1,0,icmp.A.a);
  connecticmp(gio.A,1,0,icmp.A.b);
  connecticmp(dt.A,1,0,icmp.A.c);
  connecticmp(fr.A,1,0,icmp.A.d);
  numeric x, y; (x,y) = icmp.A.d.out[0] + (8,8);
  numeric xx, yy; (xx,yy) = (0.5[xpart(route.in[0]), x], ypart(route.n)+12);
  drawarrow z{up}
  .. {up}(x,ypart route.n) .. {left}(xx,yy)
  .. {down}route.in[0];

  connecticmp(cp.B,1,0,icmp.B.a);
  connecticmp(gio.B,1,0,icmp.B.b);
  connecticmp(dt.B,1,0,icmp.B.c);
  connecticmp(fr.B,1,0,icmp.B.d);
  numeric x; x = xpart(icmp.B.d.out[0]) + 8;
  draw z{up} .. (x,ypart(icmp.B.a.out[0])+8){up}
  .. tension 1 and 1
  .. {left}(x - (yy - ypart(icmp.B.a.out[0]) - 8), yy)
  .. {left}(xx,yy);
  
  fillelement(gio.A,fr.A,dt.A,fixipsrc.A,cp.A,dropbroadcasts.A,
    icmp.A.a,icmp.A.b,icmp.A.c,icmp.A.d,
    gio.B,fr.B,dt.B,fixipsrc.B,cp.B,dropbroadcasts.B,
    icmp.B.a,icmp.B.b,icmp.B.c,icmp.B.d,
    paint.A,paint.B,strip,
    checkheader,route)(defaultecolor);
  drawelement(gio.A,fr.A,dt.A,fixipsrc.A,cp.A,dropbroadcasts.A,
    icmp.A.a,icmp.A.b,icmp.A.c,icmp.A.d,
    gio.B,fr.B,dt.B,fixipsrc.B,cp.B,dropbroadcasts.B,
    icmp.B.a,icmp.B.b,icmp.B.c,icmp.B.d,
    paint.A,paint.B,strip,
    checkheader,route);
  
  drawconnect(fr.A,0,0,arpq.A);
  drawconnect(arpq.A,0,0,outq.A);
  drawconnect(outq.A,0,0,todev.A);

  drawconnect(fr.B,0,0,arpq.B);
  drawconnect(arpq.B,0,0,outq.B);
  drawconnect(outq.B,0,0,todev.B);

  drawconnect(c.A,0,0,arpr.A);
  drawconnect(c.B,0,0,arpr.B);
  drawarrow arpr.A.out[0] .. arpr.A.out[0]-(0,7);
  drawarrow arpr.B.out[0] .. arpr.B.out[0]-(0,7);

  % labels
  defaultfont := "Helvetica";
  defaultscale := 1;

  label.top(btex \portlabel{ARP\\queries} etex scaled 1.2, c.A.out[0]+(0,2));
  label.top(btex \portlabel{ARP\\responses} etex scaled 1.2, c.A.out[1]+(0,2));
  label.top(btex \portlabel{IP} etex scaled 1.2, c.A.out[2]+(0,2));
  label.top(btex \portlabel{ARP\\queries} etex scaled 1.2, c.B.out[0]+(0,2));
  label.top(btex \portlabel{ARP\\responses} etex scaled 1.2, c.B.out[1]+(0,2));
  label.top(btex \portlabel{IP} etex scaled 1.2, c.B.out[2]+(0,2));

  defaultscale := 0.7;
  label.bot(btex to \element{Queue}~~ etex scaled 0.8, arpr.A.out[0]-(0,5));
  label.bot(btex to \element{Queue}~~ etex scaled 0.8, arpr.B.out[0]-(0,5));

  dy = ypart arpr.A.out[0] - 5 - ypart c.A.out[1];
  drawarrow c.A.out[1]{down} .. c.A.out[1] + (0,dy) withpen connectionpen;
  label.bot(btex ~to \element{ARPQuerier} etex scaled 0.8, c.A.out[1] + (0,dy));
  drawarrow c.B.out[1]{down} .. c.B.out[1] + (0,dy) withpen connectionpen;
  label.bot(btex ~to \element{ARPQuerier} etex scaled 0.8, c.B.out[1] + (0,dy));
  drawarrow arpq.A.in[1] + (0,10) .. arpq.A.in[1] withpen connectionpen;
  label.top(btex from \element{Classif{}ier} etex scaled 0.8, arpq.A.in[1] + (0,10));
  drawarrow arpq.B.in[1] + (0,10) .. arpq.B.in[1] withpen connectionpen;
  label.top(btex from \element{Classif{}ier} etex scaled 0.8, arpq.B.in[1] + (0,10));
  
  label.lft(btex to Linux etex scaled 0.8, route.out[0]-(10,9));
endfig;


end;

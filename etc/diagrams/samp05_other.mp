% samp05_other.mp -- MetaPost graph examples: non-configuration graphs
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

%% 1
%% Network diagram for the firewall configuration of samp03_routers.mp,
%% figure 1.
beginfig(1);
  save linecap;
  linecap := butt;

  elementit.idns(btex \texttt{INTERNAL\char`\_DNS} etex scaled 0.8, 0, 0, push);
  elementit.ismtp(btex \texttt{INTERNAL\char`\_SMTP} etex scaled 0.8, 0, 0, push);
  elementit.inntp(btex \texttt{INTERNAL\char`\_NNTP} etex scaled 0.8, 0, 0, push);
  idns.dx = ismtp.dx = inntp.dx = 5;
  idns.dy = ismtp.dy = inntp.dy = 5;
  idns.se - ismtp.nw = (-10,-5);
  inntp.n = .5[idns.c,ismtp.c] - (0,25);
  
  pair icenter; icenter = idns.c/3 + inntp.c/3 + ismtp.c/3;
  z.idimen = 1.5(xpart(icenter - idns.w), 1.2ypart(icenter - inntp.s));
  fixelement(idns,ismtp,inntp);
  path inet; inet = (icenter - (x.idimen,0){up} .. tension 1.1 .. icenter + (0,y.idimen){right} .. tension 1.1 .. icenter + (x.idimen,0){down} .. tension 1.1 .. icenter - (0,y.idimen){left} .. tension 1.1 .. cycle) shifted (0,-5);

  fill inet withcolor .8white;
  draw inet;
  fillelement(idns,ismtp,inntp)(white);
  drawelement(idns,ismtp,inntp);
  label.top(btex \texttt{INTERNAL} etex, icenter - (0,y.idimen - 5));

  elementit.fw(btex f{}irewall etex scaled 1.25, 0, 0, push);
  fw.w = .5[lrcorner inet, urcorner inet] - (15,0);
  fixelement(fw);
  path onet; onet = inet xscaled 0.7 yscaled 0.85;
  onet := onet shifted (fw.e - (15,0) - .5[llcorner onet, ulcorner onet]);

  elementit.ebast(btex \texttt{BASTION\vphantom{\char`\_}} etex scaled 0.8, 0, 0, push);
  elementit.enntp(btex \texttt{NNTP\char`\_FEED} etex scaled 0.8, 0, 0, push);
  enntp.nw - ebast.se = (-10,-8);
  .5[enntp.c, ebast.c] = .5[lrcorner onet, ulcorner onet] + (0,5);

  fill onet withcolor .9white;
  draw onet;
  
  elementit.r(btex router etex, 0, 0, push);
  r.w = .5[lrcorner onet, urcorner onet] - (15,0);
  fixelement(r);
  
  z.internet = r.e + (25,0);
  drawarrow r.e .. z.internet withpen pencircle scaled 3;
  label.rt(btex Internet etex, z.internet + (4,1));

  fillelement(r,fw,ebast,enntp)(white);
  drawelement(r,fw,ebast,enntp);
  label.top(btex `{}`DMZ'{}' etex, .5[llcorner onet, lrcorner onet] + (0,5));
endfig;


%% 2-5
%% Some flowcharts representing Tulip packet processing.
vardef lovearrow(expr pth) =
  save p; pair p[];
  p1 = point 0 of pth; p2 = point (length pth) of pth;
  p3 = (abs(p2-p1) / 3, 0);
  ((0,2.5) -- p3+(-5,2.5) -- p3+(-5,5) -- p3 -- p3+(-5,-5) -- p3+(-5,-2.5) -- (0,-2.5) -- cycle) scaled 3 rotated(angle(p2 - p1)) shifted p1
enddef;

vardef diamondit@#(text tt) =
  beginbox_("thediamond_","sizediamond_",@#,tt);
  generic_declare(pair) _n.n, _n.s, _n.e, _n.w;
  generic_declare(numeric) _n.dya, _n.dyb, _n.dxa, _n.dxb;
  @#c := @#off + .5[llcorner pic_@#, urcorner pic_@#] + (0, @#dya - @#dyb);
  @#e-@#c = @#c-@#w = .5(@#dxa+@#dxb,0) + .5*(lrcorner pic_@#-llcorner pic_@#);
  @#n-@#c = @#c-@#s = .5(0,@#dya+@#dyb) + .5*(ulcorner pic_@#-llcorner pic_@#);
  endbox_(cleardiamond_,@#);
enddef;

def thediamond_(suffix $) =
  $.e -- $.n -- $.w -- $.s -- cycle
enddef;

vardef cleardiamond_(suffix $) =
  _n_ := str $;
  generic_redeclare(numeric) _n.n, _n.s, _n.e, _n.w, _n.c, _n.off, _n.dxb, _n.dyb, _n.dxa, _n.dya;
enddef;

def sizediamond_(suffix $) =
  if unknown $.dxa and unknown $.dxb: $.dxa=defaultdx; fi
  if unknown $.dxa: $.dxa=$.dxb; fi
  if unknown $.dxb: $.dxb=$.dxa; fi
  if unknown $.dya and unknown $.dyb: $.dya=defaultdy; fi
  if unknown $.dya: $.dya=$.dyb; fi
  if unknown $.dyb: $.dyb=$.dya; fi
enddef;

beginfig(2);
  boxjoin(b.n - a.s = (0,-15));
  boxit.in(btex \mlabel{Packet arrives} etex);
  diamondit.fifop(btex \hbox{\mlabel{Internal\\FIFO full?}} etex);
  (fifop.dxa,fifop.dxb) = (21,21);
  (fifop.dya,fifop.dyb) = (7.5,7.5);
  boxit.enq(btex \mlabel{Enqueue packet\\on FIFO} etex);
  boxjoin(b.n - a.s = (0,-15));
  boxit.reqdesc(btex \mlabel{Request receive\\descriptor} etex);
  diamondit.descp(btex \mlabel{Got\\descriptor?} etex);
  (descp.dxa,descp.dxb) = (20,20);
  (descp.dya,descp.dyb) = (5.5,7.5);
  diamondit.busyp(btex \mlabel{Descriptor\\busy?} etex);
  (busyp.dxa,busyp.dxb) = (20,20);
  (busyp.dya,busyp.dyb) = (8,6);
  boxit.writ(btex \mlabel{Dequeue packet and\\write it to memory} etex);
  reqdesc.n - enq.s = (0,-20);

  boxjoin();
  boxit.dropfifo(btex \mlabel{Drop packet\\(FIFO overrun)} etex);
  boxit.dropframe(btex \mlabel{Drop packet\\(missed frame)} etex);
  fifop.e - dropfifo.w = descp.e - dropframe.w = (-15,0);

  fill bpath.reqdesc withcolor 0.9white;
  fill bpath.writ withcolor 0.9white;
  drawboxed(in,fifop,enq,reqdesc,descp,busyp,writ,dropfifo,dropframe);
  draw bpath.dropfifo withpen pencircle scaled 2;
  draw bpath.dropframe withpen pencircle scaled 2;
  drawconnarrow in.s -- fifop.n;
  drawconnarrow fifop.e -- dropfifo.w;
  drawconnarrow fifop.s -- enq.n;
  drawconnarrow enq.s -- reqdesc.n;
  drawconnarrow reqdesc.s -- descp.n;
  drawconnarrow descp.e -- dropframe.w;
  drawconnarrow descp.s -- busyp.n;
  drawconnarrow busyp.s -- writ.n;
  z.busyin = .5[enq.s,reqdesc.n];
  drawconnarrow busyp.w{left} .. {up}busyp.w+(-10,10) .. {up}(xpart busyp.w - 10,y.busyin-10) .. {right}(xpart busyp.w,y.busyin) .. {right}z.busyin;

  label.lrt(btex \portlabel{N} etex scaled 1.1, fifop.s);
  label.top(btex \portlabel{Y} etex scaled 1.1, fifop.e - (0,1));
  label.lrt(btex \portlabel{Y} etex scaled 1.1, descp.s);
  label.top(btex \portlabel{N} etex scaled 1.1, descp.e - (0,1));
  label.lrt(btex \portlabel{N} etex scaled 1.1, busyp.s);
  label.top(btex \portlabel{Y} etex scaled 1.1, busyp.w - (0,1));
endfig;

beginfig(3);
  boxjoin(b.n - a.s = (0,-15));
  boxit.poll(btex \mlabel{Poll packet from\\receive DMA ring} etex);
  boxit.push(btex \mlabel{Push packet\\to \element{Queue}} etex);
  diamondit.fullp(btex \mlabel{\element{Queue}\\full?} etex);
  (fullp.dxa,fullp.dxb) = (20,20);
  (fullp.dya,fullp.dyb) = (7,6);
  boxit.enq(btex \mlabel{Enqueue packet\\on \element{Queue}} etex);
  boxjoin(b.n - a.s = (0,-15));
  boxit.pull(btex \mlabel{Pull packet\\from \element{Queue}} etex);
  boxit.todev(btex \mlabel{Enqueue packet on\\transmit DMA ring} etex);
  pull.n - enq.s = (0,-30);

  boxjoin();
  boxit.dropq(btex \mlabel{Drop packet\\(\element{Queue} drop)} etex);
  dropq.w - fullp.e = (15,0);
  pull.w-pull.e = push.w-push.e;

  drawboxed(poll,fullp,pull,push,enq,todev,dropq);
  draw bpath.dropq withpen pencircle scaled 2;
  drawconnarrow poll.s -- push.n;
  drawconnarrow push.s -- fullp.n;
  drawconnarrow fullp.e -- dropq.w;
  drawconnarrow fullp.s -- enq.n;
  drawconnarrow enq.s -- pull.n;
  drawconnarrow pull.s -- todev.n;

  label.lrt(btex \portlabel{N} etex scaled 1.1, fullp.s);
  label.top(btex \portlabel{Y} etex scaled 1.1, fullp.e - (0,1));

  path slash; slash = (.5[enq.s,pull.n] - (7,1)) -- (.5[enq.s,pull.n] + (7,3));
  draw slash withpen pencircle scaled 4 withcolor white;
  draw slash shifted (0,2);
  draw slash shifted (0,-2);
endfig;

beginfig(4);
  boxjoin(b.n - a.s = (0,-15));
  boxit.pollx(btex \mlabel{Poll transmit DMA ring} etex);
  boxit.readx(btex \mlabel{Read packet from memory} etex);
  boxit.sendx(btex \mlabel{Packet sent} etex);

  pollx.w-pollx.e=readx.w-readx.e;

  fill bpath.readx withcolor 0.9white;
  fill bpath.pollx withcolor 0.9white;
  drawboxed(readx,pollx,sendx);
  draw bpath.sendx withpen pencircle scaled 2;
  drawconnarrow pollx.s -- readx.n;
  drawconnarrow readx.s -- sendx.n;
endfig;

beginfig(5);
  boxjoin(b.n - a.s = (0,-15));
  boxit.in(btex \mlabel{Packet arrives} etex);
  diamondit.fifop(btex \hbox{\mlabel{Internal\\FIFO full?}} etex);
  (fifop.dxa,fifop.dxb) = (21,21);
  (fifop.dya,fifop.dyb) = (7.5,7.5);
  boxit.enq(btex \mlabel{Enqueue packet\\on FIFO} etex);
  boxit.reqdesc(btex \mlabel{Poll for receive\\descriptor} etex);
  diamondit.descp(btex \mlabel{Got\\descriptor?} etex);
  (descp.dxa,descp.dxb) = (20,20);
  (descp.dya,descp.dyb) = (5.5,7.5);
  
  boxjoin(b.n - a.s = (0,-15));
  boxit.wait(btex \mlabel{Suspend until next\\packet arrives} etex);
  boxit.reqdescA(btex \mlabel{Poll for receive\\descriptor} etex);
  diamondit.descpA(btex \mlabel{Got\\descriptor?} etex);
  (descpA.dxa,descpA.dxb) = (20,20);
  (descpA.dya,descpA.dyb) = (5.5,7.5);
  boxit.dropframe(btex \mlabel{Drop packet\\(missed frame)} etex);
  
  boxjoin();
  boxit.writ(btex \mlabel{Dequeue packet and\\write it to memory} etex);
  boxit.dropfifo(btex \mlabel{Drop packet\\(FIFO overf{}low)} etex);
  fifop.e - dropfifo.w = (-15,0);
  dropframe.w - writ.e = (15,0);
  xpart writ.s = xpart descp.s;
  ypart wait.e = ypart descp.e;

  fixsize(in,fifop,enq,reqdesc,descp,writ,wait,reqdescA,descpA,dropframe,dropfifo);
  fixpos(in,fifop,enq,reqdesc,descp,writ,wait,reqdescA,descpA,dropframe,dropfifo);
  fill bpath.reqdesc withcolor 0.9white;
  fill bpath.reqdescA withcolor 0.9white;
  fill bpath.writ withcolor 0.9white;
  drawboxed(in,fifop,enq,reqdesc,descp,wait,reqdescA,descpA,dropfifo,dropframe,writ);
  draw bpath.dropfifo withpen pencircle scaled 2;
  draw bpath.dropframe withpen pencircle scaled 2;
  drawconnarrow in.s -- fifop.n;
  drawconnarrow fifop.e -- dropfifo.w;
  drawconnarrow fifop.s -- enq.n;
  drawconnarrow enq.s -- reqdesc.n;
  drawconnarrow reqdesc.s -- descp.n;
  drawconnarrow descp.e -- wait.w;
  drawconnarrow descp.s -- writ.n;
  drawconnarrow wait.s -- reqdescA.n;
  drawconnarrow reqdescA.s -- descpA.n;
  drawconnarrow descpA.w{left} .. {down}writ.n;
  drawconnarrow descpA.s -- dropframe.n;

  label.lrt(btex \portlabel{N} etex scaled 1.1, fifop.s);
  label.top(btex \portlabel{Y} etex scaled 1.1, fifop.e - (0,1));
  label.lrt(btex \portlabel{Y} etex scaled 1.1, descp.s);
  label.top(btex \portlabel{N} etex scaled 1.1, descp.e - (0,1));
  label.top(btex \portlabel{Y} etex scaled 1.1, descpA.w - (0,1));
  label.lrt(btex \portlabel{N} etex scaled 1.1, descpA.s);
endfig;


end

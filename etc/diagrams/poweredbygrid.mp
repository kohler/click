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

end

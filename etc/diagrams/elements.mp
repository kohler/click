input boxes;
prologues := 1;
string defaultelementfont;
input fonts;
defaultscale := 1;
linejoin := mitered;

pair element_offset;
element_offset = (7.5, 4.5);
min_element_height = 19;
element_height_increment = 4;

defaultelementborderscale = 1;
defaultelementportscale = 1;

port_length = 6;
port_sep = 3;
port_offset = 4;
input_length = 7;
input_width = 4.5;
output_length = 6;
output_width = 3.8;
agnostic_sep = 1;

push = 0;
pull = 1;
agnostic = 2;
agnostic_push = 3;
agnostic_pull = 4;
push_to_pull = 5;
pull_to_push = 6;

pen elementpen.border, elementpen.port, connectionpen;
elementpen.border = pencircle scaled 0.9;
elementpen.port = pencircle scaled 0.35;
connectionpen = pencircle scaled 0.45;

color personalitycolor[], agnosticcolor[];
personalitycolor[push] = black;
personalitycolor[agnostic_push] = personalitycolor[agnostic] = white;
personalitycolor[pull] = personalitycolor[agnostic_pull] = white;
agnosticcolor[agnostic_push] = black;
agnosticcolor[agnostic_pull] = white;
agnosticcolor[agnostic] = 0.6white;

path _agnostic_output, _agnostic_input, _normal_output, _normal_input;
_agnostic_output := ((-.5,0.5output_length-agnostic_sep)
  -- (-output_width+agnostic_sep,0.5output_length-agnostic_sep)
  -- (-output_width+agnostic_sep,-0.5output_length+agnostic_sep)
  -- (-.5,-0.5output_length+agnostic_sep) -- cycle);
_agnostic_input := ((.5,0.5input_length-1.414agnostic_sep)
  -- (input_width-1.414agnostic_sep,0)
  -- (.5,-0.5input_length+1.414agnostic_sep) -- cycle);
_normal_input := ((.5,0.5input_length) -- (input_width,0)
    -- (.5,-0.5input_length) -- cycle);
_normal_output := ((-.5,0.5output_length) -- (-output_width,0.5output_length)
    -- (-output_width,-0.5output_length) -- (-.5,-0.5output_length) -- cycle);


%% make

vardef _make_element_ports(suffix $, port, side)(expr n, length, isout) =
  save i, sc; pair sc.adj;
  sc.sep = (length - 2*port_offset - n*port_length + port_sep) / n;
  sc.delta = port_length + sc.sep;
  sc = length/2 - port_offset - (sc.sep - port_sep)/2 - 0.5port_length;
  sc.adj = if isout: 1/2$.flowvector else: -1/2$.flowvector fi;
  for i = 0 upto n-1:
    $.port[i] = $.side + $.sidevector * (sc - sc.delta*i) + sc.adj;
  endfor;
enddef;

vardef make_element_inputs(suffix $)(expr xlen, ylen) =
  if $.down:
    _make_element_ports($, in, if $.rev: s else: n fi, $.nin, xlen-6, false);
  else:
    _make_element_ports($, in, if $.rev: e else: w fi, $.nin, ylen, false);
  fi;
enddef;

vardef make_element_outputs(suffix $)(expr xlen, ylen) =
  if $.down:
    _make_element_ports($, out, if $.rev: n else: s fi, $.nout, xlen-6, true);
  else:
    _make_element_ports($, out, if $.rev: w else: e fi, $.nout, ylen, true);
  fi;
enddef;

vardef clearelement_(suffix $) =
  _n_ := str $;
  generic_redeclare(numeric) _n.down, _n.rev, _n.sidevector, _n.flowvector, _n.width, _n.height, _n.nin, _n.nout, _n.borderscale, _n.portscale, _n.drawports;
  _n_ := str $ & ".in[0]";
  generic_redeclare(numeric) _n;
  _n_ := str $ & ".out[0]";
  generic_redeclare(numeric) _n;
  _n_ := str $ & ".inpers[0]";
  generic_redeclare(numeric) _n;
  _n_ := str $ & ".outpers[0]";
  generic_redeclare(numeric) _n;
  _n_ := "elemdraw_." & str $;
  generic_redeclare(numeric) _n;
enddef;

vardef _elementit@#(expr label_str, ninputs, noutputs, personality, down_var, rev_var) =
  picture _label_; numeric _x_, _y_;
  
  if picture label_str: _label_ = label_str
  else: _label_ = label_str infont defaultelementfont scaled defaultscale
  fi;
  
  boxit.@#(_label_);
  _n_ := str @#;
  generic_declare(boolean) _n.down, _n.rev, _n.drawports;
  generic_declare(pair) _n.sidevector, _n.flowvector;
  generic_declare(numeric) _n.width, _n.height, _n.nin, _n.nout, _n.borderscale, _n.portscale;
  _n_ := str @# & ".in[0]";
  generic_declare(pair) _n;
  _n_ := str @# & ".out[0]";
  generic_declare(pair) _n;
  _n_ := "elemdraw_." & str @#;
  generic_declare(string) _n;
  
  @#.down = down_var;
  if down_var: @#.sidevector = (-1, 0); else: @#.sidevector = (0, 1); fi;
  if down_var: @#.flowvector = (0, -1); else: @#.flowvector = (1, 0); fi;
  @#.rev = rev_var;
  if rev_var: @#.flowvector := -@#.flowvector; @#.sidevector := -@#.sidevector; fi;
  @#.drawports = true;
  
  @#.width = xpart(@#.e - @#.w);
  @#.height = ypart(@#.n - @#.s);
  
  @#.nin = ninputs;
  @#.nout = noutputs;
  if ninputs > 0: make_element_inputs(@#, @#.width, @#.height); fi;
  if noutputs > 0: make_element_outputs(@#, @#.width, @#.height); fi;
  
  _x_ := personality;
  if _x_ = push_to_pull: _x_ := push;
  elseif _x_ = pull_to_push: _x_ := pull; fi;
  for _y_ = 0 upto ninputs-1: @#.inpers[_y_] = _x_; endfor;
  
  _x_ := personality;
  if _x_ = push_to_pull: _x_ := pull;
  elseif _x_ = pull_to_push: _x_ := push; fi;
  for _y_ = 0 upto noutputs-1: @#.outpers[_y_] = _x_; endfor;
  
  @#.borderscale = defaultelementborderscale;
  @#.portscale = defaultelementportscale;
  
  elemdraw_@# = "drawboxes";
  
  expandafter def expandafter clearboxes expandafter =
    clearboxes clearelement_(@#);
  enddef
enddef;

vardef elementit@#(expr s, ninputs, noutputs, personality_var) =
  _elementit.@#(s, ninputs, noutputs, personality_var, false, false);
enddef;
vardef relementit@#(expr s, ninputs, noutputs, personality_var) =
  _elementit.@#(s, ninputs, noutputs, personality_var, false, true);
enddef;
vardef velementit@#(expr s, ninputs, noutputs, personality_var) =
  _elementit.@#(s, ninputs, noutputs, personality_var, true, false);
enddef;
vardef rvelementit@#(expr s, ninputs, noutputs, personality_var) =
  _elementit.@#(s, ninputs, noutputs, personality_var, true, true);
enddef;


%% change

vardef killinput(suffix $)(expr p) =
  if (p >= 0) and (p < $.nin): save i;
    for i = p upto $.nin-2:
      $.in[i] := $.in[i+1];
      $.inpers[i] := $.inpers[i+1];
    endfor;
    $.nin := $.nin - 1
  fi
enddef;

vardef killoutput(suffix $)(expr p) =
  if (p >= 0) and (p < $.nout): save i;
    for i = p upto $.nout-2:
      $.out[i] := $.out[i+1];
      $.outpers[i] := $.outpers[i+1];
    endfor;
    $.nout := $.nout - 1
  fi
enddef;


%% fix

vardef set_element_dx(suffix $) =
  if $.down: save x;
    x.maxport = max($.nin, $.nout);
    x.len = x.maxport*port_length + (x.maxport-1)*port_sep + 2port_offset;
    x.w = xpart(urcorner pic_$ - llcorner pic_$);
    x.ww = x.w + 2xpart(element_offset);
    if x.len > x.ww: $.dx = (x.len - x.w) / 2;
    else: $.dx = xpart element_offset; fi;
  else:
    $.dx = xpart element_offset;
  fi;
enddef;

vardef set_element_dy(suffix $) =
  save y;
  y.h = ypart(urcorner pic_$ - llcorner pic_$);
  y.hh = y.h + 2ypart(element_offset);
  if $.down: y := y.hh;
  else:
    y.maxport = max($.nin, $.nout);
    y.len = y.maxport*port_length + (y.maxport-1)*port_sep + 2port_offset;
    y := max(y.hh, y.len);
  fi;
  
  y'' := min_element_height;
  forever:
    exitif y'' >= y;
    y'' := y'' + element_height_increment;
  endfor;
  
  $.dy = (y'' - y.h)/2;
enddef;

vardef fixelementsize(text t) =
  forsuffixes $=t:
    if not known $.dx: set_element_dx($); fi;
    if not known $.dy: set_element_dy($); fi;
    fixsize($);
  endfor;
enddef;

vardef fixelement(text elements) =
  fixelementsize(elements);
  fixpos(elements);
enddef;


%% draw

vardef draw_element_inputs(suffix $) =
  path _p_, _ag_;
  _p_ := _normal_input scaled $.portscale;
  _ag_ := _agnostic_input scaled $.portscale;
  if $.down: _p_ := _p_ rotated -90; _ag_ := _ag_ rotated -90; fi;
  if $.rev: _p_ := _p_ rotated 180; _ag_ := _ag_ rotated 180; fi;
  for i = 0 upto $.nin - 1:
    fill _p_ shifted $.in[i] withcolor personalitycolor[$.inpers[i]];
    draw _p_ shifted $.in[i];
    if $.inpers[i] >= agnostic:
      fill _ag_ shifted $.in[i] withcolor agnosticcolor[$.inpers[i]];
      draw _ag_ shifted $.in[i]; fi;
  endfor;
enddef;

vardef draw_element_outputs(suffix $) =
  path _p_, _ag_;
  _p_ := _normal_output scaled $.portscale;
  _ag_ := _agnostic_output scaled $.portscale;
  if $.down: _p_ := _p_ rotated -90; _ag_ := _ag_ rotated -90; fi;
  if $.rev: _p_ := _p_ rotated 180; _ag_ := _ag_ rotated 180; fi;
  for i = 0 upto $.nout - 1:
    fill _p_ shifted $.out[i] withcolor personalitycolor[$.outpers[i]];
    draw _p_ shifted $.out[i];
    if $.outpers[i] >= agnostic:
      fill _ag_ shifted $.out[i] withcolor agnosticcolor[$.outpers[i]];
      draw _ag_ shifted $.out[i]; fi;
  endfor;
enddef;

vardef drawelement(text elements) =
  drawelementbox(elements);
  drawunboxed(elements);
enddef;

vardef drawelementbox(text elements) =
  save $, oldpen; oldpen := savepen;
  interim linejoin := mitered;
  fixelementsize(elements);
  fixpos(elements);
  forsuffixes $ = elements:
    pickup elementpen.port scaled $.portscale;
    if $.drawports:
      if $.nin > 0: draw_element_inputs($); fi;
      if $.nout > 0: draw_element_outputs($); fi;
    fi;
    pickup elementpen.border scaled $.borderscale;
    scantokens elemdraw_$($);
  endfor;
  pickup oldpen;
enddef;

vardef fillelement(text elements)(text color) =
  fixelementsize(elements);
  fixpos(elements);
  forsuffixes $=elements:
    fill bpath.$ withcolor color;
  endfor;
enddef;


%% queues

vardef _drawqueued(expr p,delta,rot,lim,pp) =
  save i; interim linecap := squared; i := delta;
  forever:
    draw (p) shifted ((i,0) rotated rot) withpen currentpen scaled 0.25;
    i := i + delta; exitunless i < lim;
  endfor;
  draw (pp);
enddef;
vardef drawqueued(suffix $) =
  _drawqueued($.ne -- $.se, 6, 180, .9*$.width, $.nw -- $.ne -- $.se -- $.sw);
enddef;
vardef drawrqueued(suffix $) =
  _drawqueued($.nw -- $.sw, 6, 0, .9*$.width, $.ne -- $.nw -- $.sw -- $.se);
enddef;
vardef drawvqueued(suffix $) =
  _drawqueued($.se -- $.sw, 5, 90, .9*$.height, $.nw -- $.sw -- $.se -- $.ne);
enddef;
vardef drawrvqueued(suffix $) =
  _drawqueued($.ne -- $.nw, 5, 270, .9*$.height, $.sw -- $.nw -- $.ne -- $.se);
enddef;

vardef queueit@#(expr s) =
  _elementit.@#(s, 1, 1, push_to_pull, false, false);
  elemdraw_@# := "drawqueued";
enddef;
vardef rqueueit@#(expr s) =
  _elementit.@#(s, 1, 1, push_to_pull, false, true);
  elemdraw_@# := "drawrqueued";
enddef;
vardef vqueueit@#(expr s) =
  _elementit.@#(s, 1, 1, push_to_pull, true, false);
  elemdraw_@# := "drawvqueued";
enddef;
vardef rvqueueit@#(expr s) =
  _elementit.@#(s, 1, 1, push_to_pull, true, true);
  elemdraw_@# := "drawrvqueued";
enddef;


%% connections

vardef arrowhead expr p =
  save q,h,e,f; path q,h; pair e,f;
  e = point length p of p;
  q = gobble(p shifted -e cutafter makepath(pencircle scaled 2ahlength))
    cuttings;
  h = gobble(p shifted -e cutafter makepath(pencircle scaled 1.5ahlength))
    cuttings;
  f = point 0 of h;
  (q rotated .5ahangle & reverse q rotated -.5ahangle -- f -- cycle)  shifted e
enddef;
def _finarr text t =
  draw (subpath (0, xpart(_apth intersectiontimes makepath(pencircle scaled 1.2ahlength) shifted (point length _apth of _apth))) of _apth) t;
  fill arrowhead _apth  t
enddef;

def connectpath(suffix $,#,##,$$) =
  $.out[#]{$.flowvector} .. {$$.flowvector}$$.in[##]
enddef;
vardef drawconnectj(suffix $,#,##,$$)(text t) text rest =
  interim linejoin := mitered;
  drawarrow $.out[#]{$.flowvector} t {$$.flowvector}$$.in[##] withpen connectionpen rest
enddef;
def drawconnect(suffix $,#,##,$$) =
  drawconnectj($,#,##,$$)(..)
enddef;
vardef drawconnectna(suffix $,#,##,$$) text rest =
  interim linejoin := mitered;
  draw $.out[#]{$.flowvector} .. {$$.flowvector}$$.in[##] withpen connectionpen rest
enddef;

def drawconnarrow expr p =
  _apth:=p; _finarr withpen connectionpen
enddef;
def drawconnarrowna expr p =
  draw p withpen connectionpen
enddef;
def drawdblconnarrow expr p =
  _apth:=p; _findarr withpen connectionpen
enddef;

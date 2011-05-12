// script-trianglewave.click

// A Script element is used to control a RatedSource element's rate
// according to a triangle wave.  Watch the result by running
//    click -p9999 script-trianglewave.click && clicky -p9999

// See also the other script-*wave.click scripts.

s :: RatedSource(RATE 1000)
	-> c :: Counter
	-> d :: Discard;

Script(TYPE ACTIVE,
	init x 1000,
	init delta -5,
	wait 0.005s,
	set x $(add $x $delta),
	write s.rate $x,
	goto l1 $(in $x 0 1000),
	loop,
	label l1,
	set delta $(if $(eq $x 0) 5 -5),
	loop);

ClickyInfo(STYLE #c { decorations: activity } activity { decay: 0 });

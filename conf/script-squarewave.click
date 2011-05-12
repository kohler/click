// script-squarewave.click

// A Script element is used to control a RatedSource element's rate
// according to a square wave: 1/2 second at 1000 packets per second
// alternates with 1/2 second off.  Watch the result by running
//    click -p9999 script-squarewave.click && clicky -p9999

// See also the other script-*wave.click scripts.

s :: RatedSource(RATE 1000)
	-> c :: Counter
	-> d :: Discard;

Script(TYPE ACTIVE,
	init x 0,
	wait 0.5s,
	set x $(sub 1000 $x),
	write s.rate $x,
	loop);

ClickyInfo(STYLE #c { decorations: activity } activity { decay: 0 });

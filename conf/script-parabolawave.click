// script-parabolawave.click

// A Script element is used to control a RatedSource element's rate
// according to a parabolic wave.  Watch the result by running
//    click -p9999 script-parabolawave.click && clicky -p9999

// See also the other script-*wave.click scripts.

s :: RatedSource(RATE 1125)
	-> c :: Counter
	-> d :: Discard;

Script(TYPE ACTIVE,
	init x 1125,
	init velocity -5,
	init acceleration -5,
	wait 0.01s,
	set x $(add $x $velocity),
	write s.rate $x,
	goto skip $(lt $(abs $velocity) 75),
	set acceleration $(neg $acceleration),
	label skip,
	set velocity $(add $velocity $acceleration),
	loop);

ClickyInfo(STYLE #c { decorations: activity }
	activity { decay: 0; max-value: 1125 });

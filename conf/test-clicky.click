// test-clicky.click

// This simple test file can be used to demonstrate the Clicky GUI.

rr :: RoundRobinSched;

TimedSource(0.2) -> c1 :: Counter
	-> Queue(20)
	-> Print(q1, ACTIVE false)
	-> [0]rr;
TimedSource(0.5) -> c2 :: Counter
	-> Queue(20)
	-> Print(q2, ACTIVE false)
	-> [1]rr;

rr -> TimedSink(0.1);

ClickyInfo(STYLE @import test-clicky.ccss);

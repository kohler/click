i1 :: InfiniteSource(\<11111111>, -1, 5)
i2 :: InfiniteSource(\<22222222>, -1, 5)
i3 :: InfiniteSource(\<33333333>, -1, 5)
i4 :: InfiniteSource(\<44444444>, -1, 5)
i5 :: InfiniteSource(\<55555555>, -1, 5)
i6 :: InfiniteSource(\<66666666>, -1, 5)
i7 :: InfiniteSource(\<77777777>, -1, 5)
i8 :: InfiniteSource(\<88888888>, -1, 7)

ss :: StrideSched(10,20,30,40,50,60,70,100000)

i1 -> q1::Queue
i2 -> q2::Queue
i3 -> q3::Queue
i4 -> q4::Queue
i5 -> q5::Queue
i6 -> q6::Queue
i7 -> q7::Queue
i8 -> q8::Queue
//i9::InfiniteSource(\<99999999>, -1, 9) -> Queue ->  [8]ss

q1 -> [0]ss;
q2 -> [1]ss;
q3 -> [2]ss;
q4 -> [3]ss;
q5 -> [4]ss;
q6 -> [5]ss;
q7 -> [6]ss;
q8 -> [7]ss;

dd :: Discard

ss[0] -> Print(ss: , 4) -> dd

// ScheduleInfo(i1 1, i2 1, i3 1, i4 1, i5 1, i6 1, i7 1, i8 1, dd 1)

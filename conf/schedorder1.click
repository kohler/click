s1 :: SchedOrderTest(1, SIZE 100, STOP true);
s2 :: SchedOrderTest(2, SIZE 100, LIMIT 10);
s3 :: SchedOrderTest(3, SIZE 100);

ScheduleInfo(s1 1, s2 2, s3 3);
DriverManager(wait_stop, print s1.order, stop);

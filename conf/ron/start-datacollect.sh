#!/bin/tcsh
cd /usr/home/ron/yipal/datacollection-export/multi/
touch multi2-$1.log
./multicollect.pl -f multi2.ip -n $1 -s 10 >>& multi2-$1.log &


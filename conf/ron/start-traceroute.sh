#!/bin/tcsh
cd /usr/home/ron/yipal/datacollection-export/multi/
touch multi2-$1.trace
./collectroute.pl -f multi2.ip -n $1 -s 30 >>& multi2-$1.trace &


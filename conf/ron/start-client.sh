#!/bin/tcsh

setenv CLICKPATH /usr/home/ron/yipal/click-export/lib
sudo rm -f /tmp/clicksocket
touch /usr/home/ron/yipal/datacollection-export/multi/multi2-$1.log
cd /usr/home/ron/yipal/click-export/bin
sudo ./click ../conf/$1-client.conf >>& /usr/home/ron/yipal/datacollection-export/multi/multi2-$1.log &
sleep 1
sudo chmod a+w /tmp/clicksocket



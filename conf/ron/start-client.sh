#!/bin/tcsh

setenv CLICKPATH /usr/home/ron/yipal/click-export/lib
sudo rm -f /tmp/clicksocket
cd /usr/home/ron/yipal/click-export/bin
sudo ./click ../conf/$1-client.conf &
sudo chmod a+w /tmp/clicksocket



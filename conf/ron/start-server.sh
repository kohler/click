#!/bin/tcsh

setenv CLICKPATH /usr/home/ron/yipal/click-export/lib
cd /usr/home/ron/yipal/click-export/bin
sudo ./click ../conf/$1-server.conf >>& /dev/null &



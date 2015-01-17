#!/bin/tcsh
sudo kill `ps -ax | grep multicollect | perl -ne '/^\s*(\d+)\s+/; print "$1 ";'`
sudo rm -f /tmp/clicksocket

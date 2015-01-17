#!/bin/tcsh
sudo kill `ps -ax | grep collectroute | perl -ne '/^\s*(\d+)\s+/; print "$1 ";'`
sudo rm -f /tmp/clicksocket

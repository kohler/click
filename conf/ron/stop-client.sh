#!/bin/tcsh
sudo kill `ps -ax | grep click | grep client.conf | perl -ne '/^\s*(\d+)\s+/; print "$1 ";'`
sudo rm -f /tmp/clicksocket

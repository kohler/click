#!/bin/tcsh
sudo kill `ps -ax | grep click | grep server.conf | perl -ne '/^\s*(\d+)\s+/; print "$1 ";'`



#!/usr/bin/env python
#
# Kostas Choumas
# 
#

import sys, os, math, string, traceback, getopt, time

nodes = []

bp = "no"
ns = "no"

prepare = "no"
kill = "no"
stop = "no"

exp_id = ""
oml_server = ""

channel = ""
rate = ""
power = ""
dev_eth = "eth0"

ssh_str = "ssh -x -o LogLevel=QUIET -q -2"
perl_file = "gen_config_roofnet_bp.pl"


def before_click(node):
  node_name = "node" + node.rjust(3, "0")
  print "Updating click and modprobe at node %s" % (node)
  os.system("%s root@%s \"cd click-packages; git pull\"" % (ssh_str, node_name))
  os.system("%s root@%s \"cd click; git pull; make\"" % (ssh_str, node_name))
  os.system("%s root@%s \"modprobe ath5k\"" % (ssh_str, node_name))
  print "Done"


def start_click(node):
  node_name = "node" + node.rjust(3, "0")
  print "Creating configuration file & starting click at node %s" % (node)
  os.system("%s root@%s \"killall -9 click > /dev/null 2>&1\"" % (ssh_str, node_name))
  args = "--eth=" + dev_eth + " --channel=" + channel
  if (bp == "yes"):
    args += " --bp"
  if (rate != ""):
    args += " --rate=" + rate
  if (power != ""):
    args += " --power=" + power
  os.system("%s root@%s \"click/conf/wifi/bp/%s %s > /tmp/roofnet_bp_%s.click\"" % (ssh_str, node_name, perl_file, args, node))
  oml_args = ""
  if (oml_server != ""):
    oml_args = "--oml-exp-id " + exp_id + " --oml-id " + node + " --oml-server " + oml_server
  os.system("%s root@%s \"click/userlevel/click %s /tmp/roofnet_bp_%s.click > /dev/null 2>&1 &\"" % (ssh_str, node_name, oml_args, node))
  print "Done"


def kill_module(node):
  node_name = "node" + node.rjust(3, "0")
  print "Stop module at node %s" % (node)
  os.system("%s root@%s \"modprobe -r ath5k\"" % (ssh_str, node_name))
  print "Done"


def stop_click(node):
  node_name = "node" + node.rjust(3, "0")
  print "Stop click at node %s" % (node)
  os.system("%s root@%s \"killall -9 click\"" % (ssh_str, node_name))
  print "Done"


def start_click_ns(node):
  print "Creating configuration file for node %s" % (node)
  args = "--ns --id=" + node + " --channel=" + channel
  if (bp == "yes"):
    args += " --bp"
  if (rate != ""):
    args += " --rate=" + rate
  if (power != ""):
    args += " --power=" + power
  #neighs = nodes[(nodes.index(node)-1)%len(nodes)] + "," + nodes[(nodes.index(node)+1)%len(nodes)]
  #args += " --neigh=" + neighs
  if (node != "1"):
    args += " 2>/dev/null"
  os.system("perl %s %s > /tmp/roofnet_bp_ns_%s.click" % (perl_file, args, node))
  if (node == "1"):
    print "Done"


if (__name__ == '__main__'):

  try:
    optlist, args = getopt.getopt(sys.argv[1:], '', ['bp', 'ns', 'kill', 'stop', 'prepare', 'channel=', 'rate=', 'power=', 'ethernet=', 'exp-id=', 'oml-server='])
  except:
    traceback.print_exc()
    sys.exit(1)

  for opt in optlist:
    if (opt[0] == '--bp'):
      bp = "yes"
    elif (opt[0] == '--ns'):
      ns = "yes"
    elif (opt[0] == '--kill'):
      kill = "yes"
    elif (opt[0] == '--stop'):
      stop = "yes"
    elif (opt[0] == '--prepare'):
      prepare = "yes"
    elif (opt[0] == '--channel'): 
      channel = opt[1]
    elif (opt[0] == '--rate'):
      rate = opt[1]
    elif (opt[0] == '--power'):
      power = opt[1]
    elif (opt[0] == '--ethernet'):
      dev_eth = opt[1]
    elif (opt[0] == '--exp-id'):
      exp_id = opt[1]
    elif (opt[0] == '--oml-server'):
      oml_server = opt[1]


  if (ns == "yes"):
    for node in range(1,int(args[0])+1):
      nodes.append(str(node))
    path = sys.argv[0].split("/")
    path[-1] = perl_file
    perl_file = string.join(path, "/")
  else:
    nodes = args

  if (stop == "no" and channel == ""):
    print "There are parameters undefined and they are required."
    sys.exit(1)

  if (exp_id == ""):
    exp_id = "bp"

  if (bp == "yes"):
    print "Protocol is Backpressure"
  else:
    print "Protocol is Roofnet"
  print "Channel is %s" % (channel)
  print "Rate is (%s/2)Mbps" % (rate)
  print "Power is %smW" % (power)
  
  for node in nodes:
    if (ns == "yes"):
      start_click_ns(node)
    elif (kill == "yes"):
      kill_module(node)
      stop_click(node)
    elif (stop == "yes"):
      stop_click(node)
    elif (prepare == "yes"):
      before_click(node)
      start_click(node)
    else:
      start_click(node)


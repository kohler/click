#!/bin/sh
#
# generate a roofnet config file for click
# John Bicket
# 
#

DEV="ath2"
GATEWAY="false"
if [ -f /tmp/is_gateway ]; then
    GATEWAY="true"
fi


mac=$(/sbin/ifconfig wifi0 | sed -n 's/^.*HWaddr \([0-9A-Za-z:]*\).*/\1/p')
# extract the bottom three octects to use as IP
                            
hi_hex=$(echo $mac | sed -n 's/.*:.*:.*:\([0-9A-Za-z:]*\):.*:.*.*/\1/p')
mid_hex=$(echo $mac | sed -n 's/.*:.*:.*:.*:\([0-9A-Za-z:]*\):.*.*/\1/p')
lo_hex=$(echo $mac | sed -n 's/.*:.*:.*:.*:.*:\([0-9A-Za-z:]*\).*/\1/p')
                                                        
hi="0x$hi_hex";             
mid="0x$mid_hex";           
lo="0x$lo_hex";             

SUFFIX=$(printf "%d.%d.%d" $hi $mid $lo)
WIRELESS_MAC=$mac
SRCR_IP="5.$SUFFIX"
SRCR_NM="255.0.0.0"
SRCR_NET="5.0.0.0"
SRCR_BCAST="5.255.255.255"
SRCR2_IP="6.$SUFFIX"
SRCR2_NM="255.0.0.0"
SRCR2_NET="6.0.0.0"
SRCR2_BCAST="5.255.255.255"
WLANCONFIG=/usr/sbin/wlanconfig
if [ -f /home/roofnet/bin/wlanconfig ]; then
	WLANCONFIG=/home/roofnet/bin/wlanconfig
fi



$WLANCONFIG $DEV destroy > /dev/null 2>&1
$WLANCONFIG $DEV create wlandev wifi0 wlanmode monitor > /dev/null
/sbin/ifconfig $DEV mtu 1900
/sbin/ifconfig $DEV txqueuelen 5
/sbin/ifconfig $DEV up
echo '804' >  /proc/sys/net/$DEV/dev_type
/sbin/modprobe tun > /dev/null 2>&1

MODE="g"
PROBES="2 60 2 1500 4 1500 11 1500 22 1500"
#    $probes = "2 60 12 60 2 1500 4 1500 11 1500 22 1500 12 1500 18 1500 24 1500 36 1500 48 1500 72 1500 96 1500";

echo "rates :: AvailableRates(DEFAULT 11 22,
$WIRELESS_MAC 11 22);
";

SRCR_FILE="srcr.click"
if [ ! -f $SRCR_FILE ]; then
    SRCR_FILE="/home/roofnet/click/conf/wifi/srcr.click"
    if [ ! -f $SRCR_FILE ]; then
	SRCR_FILE="/tmp/srcr.click"
	if [ ! -f $SRCR_FILE ]; then
	    SRCR_FILE="/tmp/srcr.click"
	fi
    fi
fi

if [ ! -f $SRCR_FILE ]; then
    echo "couldn't find srcr.click";
    exit 1;
fi

cat $SRCR_FILE


echo "
control :: ControlSocket(\"TCP\", 7777);
chatter :: ChatterSocket(\"TCP\", 7778);


// has one input and one output
// takes and spits out ip packets

elementclass LinuxHost {
    \$dev, \$ip, \$nm, \$mac |
    input -> ToHost(\$dev);
    FromHost(\$dev, \$ip/\$nm, ETHER \$mac) -> output;
}

// has one input and one output
// takes and spits out ip packets
elementclass LinuxIPHost {
    \$dev, \$ip, \$nm |

  input -> KernelTun(\$ip/\$nm, MTU 1500, DEV_NAME \$dev) 
  -> MarkIPHeader(0)
  -> CheckIPHeader(CHECKSUM false)
  -> output;

}

elementclass SniffDevice {
    \$device, \$promisc|
	// we only want txf for NODS packets
	// ether[2:2] == 0x1200 means it has an ath_rx_radiotap header (it is 18 bytes long)
	// ether[2:2] == 0x1000 means it has an ath_tx_radiotap header (it is 16 bytes long)
	// ether[18] == 0x08 means NODS
  from_dev :: FromDevice(\$device, 
			 PROMISC \$promisc) 
  -> output;
  input -> to_dev :: ToDevice(\$device);
}

sniff_dev :: SniffDevice($DEV, false);

sched :: PrioSched()
-> set_power :: SetTXPower(POWER 60)
-> athdesc_encap :: AthdescEncap()
//-> radiotap_encap :: RadiotapEncap()
-> sniff_dev;

route_q :: FullNoteQueue(10) 
-> [0] sched;

data_q :: FullNoteQueue(10)
-> data_static_rate :: SetTXRate(RATE 22)
//-> data_madwifi_rate :: MadwifiRate(OFFSET 4,
//			       ALT_RATE true,
//			       RT rates,
//			       ACTIVE true)

-> [1] sched;

//Idle -> [1] data_madwifi_rate;



route_encap :: WifiEncap(0x0, 00:00:00:00:00:00)
->  route_q;
data_encap :: WifiEncap(0x0, 00:00:00:00:00:00)
-> data_q;




srcr1 :: srcr_ett($SRCR_IP, $SRCR_NM, $WIRELESS_MAC, $GATEWAY, 
		 \"$PROBES\");

srcr1_host :: LinuxIPHost(srcr1, $SRCR_IP, $SRCR_NM)
->  srcr1_cl :: IPClassifier(dst net 10.0.0.0/8, -);

ap_to_srcr1 :: SRDestCache();

srcr1_cl [0] -> [0] ap_to_srcr1 [0] -> [1] srcr1;
srcr1_cl [1] -> [1] srcr1;


srcr1 [0] -> route_encap;   // queries, replies
srcr1 [1] -> route_encap;   // bcast_stats
srcr1 [2] -> data_encap;    // data
srcr1 [3] -> srcr1_cl2 :: IPClassifier(src net 10.0.0.0/8, -); //data to me

srcr1_cl2 [0] -> [1] ap_to_srcr1 [1] -> srcr1_host; 
srcr1_cl2 [1] -> srcr1_host; // data to me





srcr2 :: sr2($SRCR2_IP, $SRCR2_NM, $WIRELESS_MAC, $GATEWAY, 
		 \"$PROBES\");

srcr2_host :: LinuxIPHost(srcr2, $SRCR2_IP, $SRCR2_NM)
->  srcr2_cl :: IPClassifier(dst net 10.0.0.0/8, -);

ap_to_srcr2 :: SRDestCache();

srcr2_cl [0] -> [0] ap_to_srcr2 [0] -> [1] srcr2;
srcr2_cl [1] -> [1] srcr2;


srcr2 [0] -> route_encap;   // queries, replies
srcr2 [1] -> route_encap;   // bcast_stats
srcr2 [2] -> data_encap;    // data
srcr2 [3] -> srcr2_cl2 :: IPClassifier(src net 10.0.0.0/8, -); //data to me

srcr2_cl2 [0] -> [1] ap_to_srcr2 [1] -> srcr2_host; 
srcr2_cl2 [1] -> srcr2_host; // data to me






sniff_dev 
-> athdesc_decap :: AthdescDecap()
-> phyerr_filter :: FilterPhyErr()
-> Classifier(0/08%0c) //data
-> tx_filter :: FilterTX()
-> dupe :: WifiDupeFilter() 
-> WifiDecap()
-> HostEtherFilter($WIRELESS_MAC, DROP_OTHER true, DROP_OWN true) 
-> ncl :: Classifier(12/09??, 12/06??);


ncl[0] -> srcr1;
ncl[1] -> srcr2;

tx_filter[1] -> Discard;
//tx_filter [1]  -> [1] data_madwifi_rate;

";


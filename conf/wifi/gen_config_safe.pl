#!/usr/bin/perl -w

use strict;

use Getopt::Long;



sub usage() {
    print STDERR "usage:
    --dev {e. g. ath0}
    --ssid
    --gateway
    --ap ( act as an access point. off by default)
    --txf (enable/disable tx-feedback. on by default)
";
    exit 1;
}

sub mac_addr_from_dev($) {
    my $device = $_[0];
    my $output = `/sbin/ifconfig $device 2>&1`;
    if ($output =~ /^$device: error/) {
        return "";
    }
    my @tmp = split(/\s+/, $output);
    my $mac = $tmp[4];
    $mac =~ s/-/:/g;
    my @hex = split(/:/, $mac);
    
    return uc (join ":", @hex[0 .. 5]);
}




sub mac_addr_to_ip($) {
    my $mac = $_[0];
    my @hex = split(/:/, $mac);
    if (scalar(@hex) != 6) {
        return "0";
    }
    # convert the hex digits to decimals                                            
    my $x = hex($hex[3]);
    my $y = hex($hex[4]);
    my $z = hex($hex[5]);

    return "$x.$y.$z";

}

my $dev;
my $rawdev;
my $ssid;
my $gateway = 0;
my $ap = 0;
my $txf = 1;
my $interval = 10000;
my $rate_control = "static-2";
my $kernel = 0;
GetOptions('device=s' => \$dev,
	   'ssid=s' => \$ssid,
	   'gateway' => \$gateway,
	   'ap!' => \$ap,
	   'txf!' => \$txf,
	   'rate-control=s' => \$rate_control,
	   'kernel!' => \$kernel,
	   ) or usage();


if (! defined $dev) {
    if (`/sbin/ifconfig ath0 2>&1` =~ /Device not found/) {
	if (`/sbin/ifconfig wlan0 2>&1` =~ /Device not found/) {
	} else {
	    $dev = "wlan0";
	}
    } else {
	$dev = "ath0";
    }
}


if (! defined $dev) {
    usage();
}

$rawdev = $dev;
if ($dev =~ /ath/) {
    $rawdev = "${dev}raw";
}

my $wireless_mac = mac_addr_from_dev($dev);
my $suffix = mac_addr_to_ip($wireless_mac);
my $safe_ip = "6." . $suffix;

print <<EOF;
AddressInfo(safe_addr $safe_ip/8 $wireless_mac);
winfo :: WirelessInfo(BSSID 00:00:00:00:00:00);

FromHost(safe, safe_addr, ETHER safe_addr)
-> q :: Queue()
-> encap :: WifiEncap(0x0, WIRELESS_INFO winfo)
-> set_power :: SetTXPower(63)
-> set_rate :: SetTXRate(2)
-> radiotap_encap :: RadiotapEncap()
-> to_dev :: ToDevice($rawdev);



// ether[2:2] == 0x1200 means it has an ath_rx_radiotap header (it is 18 bytes long)
// ether[18] == 0x08 means NODS
// ether[34:4] == 0 and ether[38:2] == 0 means a bssid of 00:00:00:00:00:00
// ether[48] is the ethertype
from_dev :: FromDevice($rawdev,
	   	   BPF_FILTER "ether[2:2] == 0x1200 and ether[18] == 0x08 and ether[34:4] == 0 and ether[38:2] == 0 and ether[48] == 0x08"
)
-> prism2_decap :: Prism2Decap()
-> extra_decap :: ExtraDecap()
-> radiotap_decap :: RadiotapDecap()
-> phyerr_filter :: FilterPhyErr()
-> tx_filter :: FilterTX()
-> dupe :: WifiDupeFilter()
-> wifi_cl :: Classifier(0/08%0c 1/00%03) //nods data
-> WifiDecap()
-> SetPacketType(HOST)
-> ToHost(safe);

EOF

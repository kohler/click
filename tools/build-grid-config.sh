#!/bin/sh

# Build a Grid Click configuration file with appropriate parameters
# for each network device.

# Prints the finished click config on stdout

# some default params
click=click
click_config_file=grid.click
grid_param_file=grid-params.m4
m4=m4


usage () {
    echo "
Usage: build-grid-config.sh <IP> <NETMASK> <DEV> [options]

Options:
  -V, --verbose

  --gateway-device DEV    Enable gateway functionality, use DEV as the gateway device; 
			  get the gateway IP by looking at DEV's configuration.
  --gateway-ip IP         Specify an explicit gateway device IP address.
    
  --no-location           Be location-unaware; this is the default.  
  --location LAT LON      Specify the node's location as decimal latitude/longitude.
			  This option is not compatible with --no-location.
  --location-error ERR    Location error radius, in metres.  Defaults to 0.
  --loc-tag TAG           Location tag string.  

  --disable-gf            Disable geographic forwarding

  --click CLICK           Where to find the Click executable.
                          Default: $click
  --m4 M4                 Where to find the m4 executable.
                          Default: $m4
  --config CONFIG         Which Click configuration file to use.
			  Default: $click_config_file
  --params PARAM	  Which Grid parameter file to use.
                          Default: $grid_param_file	    

  -h, --help              Print this message and exit.
" 1>&2
    exit 0
}

short_usage () {
    echo "Usage: build-grid-config.sh <IP> <NETMASK> <DEV> [options]" 1>&2
    echo "Try build-grid-config.sh --help for more information." 1>&2
    exit 1
}


verbose=0

grid_ip=
grid_netmask=
grid_dev=

gw_ip=
gw_dev=

lat=
lon=
no_loc=
loc_err=0
loc_tag=no_tag

disable_gf=0

# process command line
if test $# -gt 0; then
    if [ x"$1" = "x--help" ]; then
	usage
    fi
fi

if test $# -lt 3; then  
    short_usage
fi

grid_ip=$1; shift 1;
grid_netmask=$1; shift 1;
grid_dev=$1; shift 1;

while [ x"$1" != x ]; do
case $1 in
    -h|--h|--he|--hel|--help)
	usage;;
    -V|--verb|--verbo|--verbos|--verbose)
	verbose=1; shift 1;;
    --no-loc|--no-loca|--no-locat|--no-locati|--no-locatio|--no-location)
	no_loc=1; shift 1;;
    --loc|--loca|--locat|--locati|--locatio|--location)
	if test $# -lt 3; then 
	    echo "Missing latitude or longitude."
	    usage
	fi
	shift 1; lat=$1
	shift 1; lon=$1;
	shift 1;;
    --location-err|--location-erro|--location-error)
	if test $# -lt 2; then 
	    echo "Missing location error."
	    usage
	fi
	shift 1; loc_err=$1;
	shift 1;;
    --loc-tag)
	if test $# -lt 2; then
	    echo "Missing location tag."
	    usage
	fi
	shift 1; loc_tag=$1;
	shift 1;;
    --gateway-dev|--gateway-devi|--gateway-devic|--gateway-device)
	if test $# -lt 2; then 
	    echo "Missing gateway device."
	    usage
	fi
	shift 1; gw_dev=$1;
	shift 1;;
    --cli|--clic|--click)
	if test $# -lt 2; then 
	    echo "Missing executable name."
	    usage
	fi
	shift 1; click=$1;
	shift 1;;
    --cpp)
	if test $# -lt 2; then 
	    echo "Missing executable name."
	    usage
	fi
	shift 1; CPP=$1;
	shift 1;;
    --conf|--confi|--config)
	if test $# -lt 2; then 
	    echo "Missing configuration file."
	    usage
	fi
	shift 1; click_config_file=$1;
	shift 1;;
    --par|--para|--param|--params)
	if test $# -lt 2; then 
	    echo "Missing parameter file."
	    usage
	fi
	shift 1; grid_param_file=$1;
	shift 1;;
    --gateway-ip)
	if test $# -lt 2; then 
	    echo "Missing gateway IP address."
	    usage
	fi
	shift 1; gw_ip=$1;
	shift 1;;
    --disable-gf)
        disable_gf=1;;
    *)
	usage;;
    esac
done

if [ -n "$lat" -o -n "$lon" ]; then
    if [ "x$no_loc" = "x1" ]; then
	echo "You specifed --no-location together with --location."
	usage
    fi
    no_loc=0
else
    no_loc=1
fi
	
# check for gateway device
if [ -n "$gw_dev" ]; then
    foo=`ifconfig -a | sed -n -e "s/^\($gw_dev\).*/\1/p"`
    if [ -z "$foo" ]; then 
	echo "Error: gateway device $gw_dev does not exist." 1>&2
	exit 1
    fi
fi

# possibly setup gateway
if [ -n "$gw_dev" ]; then
    if [ -z "$gw_ip" ]; then
	# get the gateway device IP
	# thanks to Thomer for this phat regexp...
	gw_ip=`ifconfig $gw_dev | sed -n -e 's/[[:space:]]*inet[^0-9]*\([0-9]*\.[0-9]*\.[0-9]*\.[0-9]*\).*/\1/p'`
    fi

    # get the gateway device MAC address
    gw_mac=`ifconfig $gw_dev | sed -n -e 's/.*\(\([0-9a-fA-F]\{2\}:\)\{5\}[0-9a-fA-f]\{2\}\).*/\1/p'`
    if [ -z "$grid_mac" ]; then
	# fallback case for OpenBSD, which doesn't put Ethernet MAC in ifconfig output
	gw_mac=`netstat -n -I $gw_dev | sed -n -e 's/.*\(\([0-9a-fA-F]\{2\}:\)\{5\}[0-9a-fA-f]\{2\}\).*/\1/p'`
    fi
fi


# check for Grid device
foo=`ifconfig -a | sed -n -e "s/^\($grid_dev\).*/\1/p"`
if [ "$foo" != "$grid_dev" ]; then 
    echo "Error: Grid device $grid_dev does not exist." 1>&2
    exit 1
fi

# get Grid device MAC address
grid_mac=`ifconfig $grid_dev | sed -n -e 's/.*\(\([0-9a-fA-F]\{2\}:\)\{5\}[0-9a-fA-f]\{2\}\).*/\1/p'`
if [ -z "$grid_mac" ]; then
    # fallback case for OpenBSD, which doesn't put Ethernet MAC in ifconfig output
    grid_mac=`netstat -n -I $grid_dev | sed -n -e 's/.*\(\([0-9a-fA-F]\{2\}:\)\{5\}[0-9a-fA-f]\{2\}\).*/\1/p'`
fi

if [ $verbose -eq 1 ]; then
    if [ -n "$gw_dev" ]; then
	echo "Using gateway device $gw_dev, IP address $gw_ip, MAC address $gw_mac" 1>&2
    fi

    echo "Using Grid device $grid_dev, IP address $grid_ip, MAC address $grid_mac" 1>&2
    if [ "$no_loc" -eq 0 ]; then
	echo "Using location $lat,$lon" 1>&2
    else
	echo "Not using location" 1>&2
    fi
fi


# build preprocessor definition argument string
defines=

# add location arguments
if [ $no_loc -eq 1 ]; then
    # XXX needs to be handled correctly
    defines="$defines -DPOS_LAT=0 -DPOS_LON=0 -DARG_LOC_GOOD=false -DARG_LOC_ERR=0 -DARG_LOC_TAG=$loc_tag"
else
    defines="$defines -DPOS_LAT=$lat -DPOS_LON=$lon -DARG_LOC_GOOD=true -DARG_LOC_ERR=$loc_err -DARG_LOC_TAG=$loc_tag"
fi

# add Grid device arguments
no_dot=`echo $grid_ip | sed -e 's/\./ /g'`
grid_hex_ip=`printf %.2x%.2x%.2x%.2x  $no_dot`

defines="$defines -DGRID_IP=$grid_ip -DGRID_HEX_IP=$grid_hex_ip -DGRID_NETMASK=$grid_netmask"
defines="$defines -DGRID_MAC_ADDR=$grid_mac -DGRID_NET_DEVICE=$grid_dev"

# optional gateway configuration
if [ -n "$gw_dev" ]; then
    defines="$defines -DIS_GATEWAY -DGW_IP=$gw_ip -DGW_MAC_ADDR=$gw_mac -DGW_NET_DEVICE=$gw_dev"
fi

# turn off geographic forwarding?
if [ $disable_gf -eq 1 ]; then
    defines="$define -DDISABLE_GF"
fi

# get protocol numbers from Click binary 
defines="$defines -DGRID_ETH_PROTO=`$click -q -e 'g::GridHeaderInfo' -h g.grid_ether_proto`"
defines="$defines -DGRID_PROTO_LR_HELLO=`$click -q -e 'g::GridHeaderInfo' -h g.grid_proto_lr_hello`"
defines="$defines -DGRID_PROTO_NBR_ENCAP=`$click -q -e 'g::GridHeaderInfo' -h g.grid_proto_nbr_encap`"
defines="$defines -DGRID_PROTO_LOC_QUERY=`$click -q -e 'g::GridHeaderInfo' -h g.grid_proto_loc_query`"
defines="$defines -DGRID_PROTO_LOC_REPLY=`$click -q -e 'g::GridHeaderInfo' -h g.grid_proto_loc_reply`"
defines="$defines -DGRID_PROTO_ROUTE_PROBE=`$click -q -e 'g::GridHeaderInfo' -h g.grid_proto_route_probe`"
defines="$defines -DGRID_PROTO_ROUTE_REPLY=`$click -q -e 'g::GridHeaderInfo' -h g.grid_proto_route_reply`"

# get offset and header size info from Click binary
sizeof_ether=14
offsetof_grid_hdr_type=`$click -q -e 'g::GridHeaderInfo' -h g.offsetof_grid_hdr_type`
defines="$defines -DOFFSET_GRID_PROTO=$(($sizeof_ether + $offsetof_grid_hdr_type))"

sizeof_grid_hdr=`$click -q -e 'g::GridHeaderInfo' -h g.sizeof_grid_hdr`
sizeof_grid_nbr_encap=`$click -q -e 'g::GridHeaderInfo' -h g.sizeof_grid_nbr_encap`
defines="$defines -DOFFSET_ENCAP_IP=$(($sizeof_ether + $sizeof_grid_hdr + $sizeof_grid_nbr_encap))"

offsetof_grid_loc_query_dst_ip=`$click -q -e 'g::GridHeaderInfo' -h g.offsetof_grid_loc_query_dst_ip`
defines="$defines -DOFFSET_LOC_QUERY_DST=$(($sizeof_ether + $sizeof_grid_hdr + $offsetof_grid_loc_query_dst_ip))"

offsetof_grid_nbr_encap_dst_ip=`$click -q -e 'g::GridHeaderInfo' -h g.offsetof_grid_nbr_encap_dst_ip`
defines="$defines -DOFFSET_LOC_REPLY_DST=$(($sizeof_ether + $sizeof_grid_hdr + $offsetof_grid_nbr_encap_dst_ip))"

defines="$defines -DOFFSET_ROUTE_PROBE_DST=$(($sizeof_ether + $sizeof_grid_hdr + $offsetof_grid_nbr_encap_dst_ip))"

defines="$defines -DTUN_INPUT_HEADROOM=$(($sizeof_grid_hdr + $sizeof_grid_nbr_encap))"



# build the config!
cat $grid_param_file $click_config_file | $m4 $defines

# end with a run through sed to fix up the spaces in the classifier patterns
# sed -e "s/\ \//\//" -e "s/\/\ /\//"


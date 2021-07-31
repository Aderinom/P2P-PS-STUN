#!/bin/bash

#out_if=$(ip route get 1.1.1.1 | grep eth  | awk '{print $5}')
#function get_out_interface()

priv_if=eth0
publ_if=eth1

echo "1" >/proc/sys/net/ipv4/ip_forward



function switch_nat_mode() {
	case $1 in 
		1)
		    echo "eins"
	    ;;
		1)
		    echo "zwei"
	    ;;
	esac
}

switch_nat_mode 1



function full_cone() {
    # 1:1 full mapping
    priv_ip=$1
    publ_ip=$2

    iptables --flush
    iptables -t nat --flush

    iptables -t nat -A POSTROUTING -o $publ_if -j SNAT --to-source $publ_ip
    iptables -t nat -A PREROUTING  -i $publ_if -j DNAT --to-destination $priv_ip
}

function restricted_cone_nat() {
    # 1:1 full mapping but internal has to initiate
    publ_ip=$2
    iptables --flush
    iptables -t nat --flush
    iptables -t nat -A POSTROUTING -o $publ_if -j SNAT --to-source $publ_ip
}

function port_restricted_cone_nat() {
    # 1:1 mapping restricted to certain ports and internal has to initiate
    publ_ip=$2
    iptables --flush
    iptables -t nat --flush

    iptables -A FORWARD -i $priv_if -o $publ_if -p tcp --dport 4444 -j ACCEPT
    iptables -A FORWARD -i $publ_if -o $priv_if -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -j DROP

    iptables -t nat -A POSTROUTING -o $publ_if -j SNAT --to-source $publ_ip
}

function symmetrical_nat() {
    # 1:x mapping forcing same DESTIP+Port mix
    publ_ip=$2
    iptables --flush
    iptables -t nat --flush

    iptables -A FORWARD -i $priv_if -o $publ_if -j ACCEPT
    iptables -A FORWARD -i $publ_if -o $priv_if -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -j DROP

    iptables -t nat -A POSTROUTING -o $publ_if -j MASQUERADE 
}

function symmetrical_nat_random() {
    # 1:x mapping forcing same DESTIP+Port mix
    publ_ip=$2
    iptables --flush
    iptables -t nat --flush

    iptables -A FORWARD -i $priv_if -o $publ_if -j ACCEPT
    iptables -A FORWARD -i $publ_if -o $priv_if -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -j DROP

    iptables -t nat -A POSTROUTING -o $publ_if -j MASQUERADE 
}
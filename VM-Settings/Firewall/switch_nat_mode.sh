#!/bin/bash


# Get public IP and IF by checking where default route is
publ_if=$(ip route get 1.1.1.1 | grep via | awk  '{printf $5}')
publ_ip=$(ip route get 1.1.1.1 | grep via | awk  '{printf $7}')
# Private_if is not public if and not lo 
priv_if=$(ip -br address | awk -v publ_if=$publ_if '{if($1!=publ_if && $1!="lo"){printf "%s\n",$1}}')
priv_ip=$(ip route | grep eth0 | awk '{printf $9}')

sed -i 's/#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/' /etc/sysctl.conf
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
    target=$2

    iptables --flush
    iptables -t nat --flush

    iptables -t nat -A POSTROUTING -o $publ_if -j SNAT --to-source $publ_ip
    iptables -t nat -A PREROUTING  -i $publ_if -j DNAT --to-destination $target
}

function restricted_cone_nat() {
    # 1:1 full mapping but internal has to initiate

    iptables --flush
    iptables -t nat --flush
    iptables -t nat -A POSTROUTING -o $publ_if -j SNAT --to-source $publ_ip
}

function port_restricted_cone_nat() {
    # 1:1 mapping restricted to certain ports and internal has to initiate
    iptables --flush
    iptables -t nat --flush

    iptables -A FORWARD -i $priv_if -o $publ_if -p upd --dport 3333 -j ACCEPT
    iptables -A FORWARD -i $publ_if -o $priv_if -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -j DROP

    iptables -t nat -A POSTROUTING -o $publ_if -j SNAT --to-source $publ_ip
}

function symmetrical_nat() {
    # 1:x mapping forcing same DESTIP+Port mix
    iptables --flush
    iptables -t nat --flush

    iptables -A FORWARD -i $priv_if -o $publ_if -j ACCEPT
    iptables -A FORWARD -i $publ_if -o $priv_if -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -j DROP

    iptables -t nat -A POSTROUTING -p udp -o $publ_if -j SNAT --to-source $publ_ip 
    iptables -t nat -A POSTROUTING -p tcp -o $publ_if -j SNAT --to-source $publ_ip 
}

function symmetrical_nat_random() {
    # 1:x mapping forcing same DESTIP+Port mix
    iptables --flush
    iptables -t nat --flush

    iptables -A FORWARD -i $priv_if -o $publ_if -j ACCEPT
    iptables -A FORWARD -i $publ_if -o $priv_if -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -j DROP

    iptables -t nat -A POSTROUTING -o $publ_if -j MASQUERADE 
}
#!/bin/bash

# Get public IP and IF by checking where default route is
publ_if=$(ip route get 1.1.1.1 | grep via | awk  '{printf $5}')
publ_ip=$(ip route get 1.1.1.1 | grep via | awk  '{printf $7}')
# Private_if is not public if and not lo 
priv_if=$(ip -br address | awk -v publ_if=$publ_if '{if($1!=publ_if && $1!="lo"){printf "%s\n",$1}}')
priv_ip=$(ip route | grep eth0 | awk '{printf $9}')

# Override the ipv4 forwarding rule to
sed -i 's/#net.ipv4.ip_forward=1/net.ipv4.ip_forward=1/' /etc/sysctl.conf
echo "1" >/proc/sys/net/ipv4/ip_forward


function switch_nat_mode() {
	case $1 in 
		"full")
		    echo "switching to Full_cone!"
            full_cone
	    ;;
	    "restr")
		    echo "switching to Restricted_cone_nat!"
            restricted_cone_nat
	    ;;

        "sym")
		    echo "switching to Symmetrical_nat!"
            symmetrical_nat
	    ;;
        "sym_r")
		    echo "switching to random Symmetrical_nat!"
            rand_symmetrical_nat
	    ;;


        "sym_m")
		    echo "switching to Symmetrical_nat using masquerade!"
            symmetrical_nat_masq
	    ;;
        "sym_m_r")
		    echo "switching to random Symmetrical_nat using masquerade!"
            rand_symmetrical_natmasq
	    ;;
        *)
            echo "Usasge"
            echo "switch_nat_mode.sh full <targetIP>    Full_cone nat forwarding everything to targetIP"
            echo "switch_nat_mode.sh restr              Restricted_cone nat forwarding already initialized connections to targetIP"
            echo "switch_nat_mode.sh sym                Symmetric nat using the hosts port as mapped port"
            echo "switch_nat_mode.sh sym_r              Symmetric nat using a random starting port as mapped port"
            echo "switch_nat_mode.sh sym_m              Symmetric nat using the hosts port as mapped port by masquerade"
            echo "switch_nat_mode.sh sym_m_r            Symmetric nat using a random starting port as mapped port by masquerade"
        ;;
	esac
}


function full_cone() {
    # 1:1 full mapping
    iptables --flush
    iptables -t nat --flush

    iptables -t nat -A POSTROUTING -o $publ_if -j SNAT --to-source $publ_ip
    iptables -t nat -A PREROUTING  -i $publ_if -j DNAT --to-destination $target_ip
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

    iptables -t nat -A POSTROUTING -o $publ_if -j SNAT --to-source $publ_ip

}

function rand_symmetrical_nat() {
    # 1:x mapping forcing same DESTIP+Port mix
    iptables --flush
    iptables -t nat --flush

    iptables -A FORWARD -i $priv_if -o $publ_if -j ACCEPT
    iptables -A FORWARD -i $publ_if -o $priv_if -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -j DROP

    iptables -t nat -A POSTROUTING -o $publ_if -j SNAT --to-source $publ_ip --random

}


function symmetrical_nat_masq() {
    # 1:x mapping forcing same DESTIP+Port mix
    iptables --flush
    iptables -t nat --flush

    iptables -A FORWARD -i $priv_if -o $publ_if -j ACCEPT
    iptables -A FORWARD -i $publ_if -o $priv_if -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -j DROP

    iptables -t nat -A POSTROUTING -o $publ_if -j MASQUERADE

}

function rand_symmetrical_natmasq() {
    # 1:x mapping forcing same DESTIP+Port mix
    iptables --flush
    iptables -t nat --flush

    iptables -A FORWARD -i $priv_if -o $publ_if -j ACCEPT
    iptables -A FORWARD -i $publ_if -o $priv_if -m state --state RELATED,ESTABLISHED -j ACCEPT
    iptables -A FORWARD -j DROP

    iptables -t nat -A POSTROUTING -o $publ_if -j MASQUERADE --random

}

target_ip=$2

switch_nat_mode $1
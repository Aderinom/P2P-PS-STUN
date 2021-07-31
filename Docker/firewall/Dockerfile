FROM ubuntu:xenial

ENV fwprivate_address=""
ENV fwpublic_address=""

RUN apt-get update && \
    apt-get install -y iptables iproute2 curl iputils-ping iputils-tracepath rsyslog

CMD export privateif=$(ip route | grep $fwprivate_address | grep -o eth.) && \
    export publicif=$(ip route  | grep $fwpublic_address | grep -o eth.) && \
    while :; do :; done & kill -STOP $! && wait $!


    #    iptables -t nat -A POSTROUTING -o $publicif -j MASQUERADE &&\
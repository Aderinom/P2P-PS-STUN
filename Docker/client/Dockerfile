FROM ubuntu:xenial

ENV gateway_address=""

RUN apt-get update && \
    apt-get install -y iproute2 curl iputils-ping iputils-tracepath rsyslog

CMD ip route del default && \
    while :; do :; done & kill -STOP $! && wait $!
    #ip route add default via $gateway_address &&\
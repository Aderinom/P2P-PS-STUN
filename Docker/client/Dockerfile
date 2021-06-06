FROM ubuntu:xenial

ENV gateway_address=""

RUN apt-get update && \
    apt-get install -y iproute2 curl iputils-ping iputils-tracepath

CMD ip route del default && \
    ip route add default via $gateway_address &&\
    while :; do :; done & kill -STOP $! && wait $!
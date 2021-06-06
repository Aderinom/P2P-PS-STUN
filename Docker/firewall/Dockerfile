FROM alpine:3.13.5

ENV fwprivate_address=""
ENV fwpublic_address=""

RUN apk update && apk add iptables curl iputils 

CMD export privateif=$(ip route | grep $fwprivate_address | grep -o eth.) && \
    export publicif=$(ip route  | grep $fwpublic_address | grep -o eth.) && \
    iptables -t nat -A POSTROUTING -o $publicif -j MASQUERADE &&\
    while :; do :; done & kill -STOP $! && wait $!
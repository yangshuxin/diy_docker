#!/bin/bash

if [ $(id -u) -ne 0 ]; then
    echo "need root privilege"
    exit 1
fi

# enable ip forwarding
sysctl -w net.ipv4.ip_forward=1

x=$(iptables -t nat -L | grep MASQUERADE | grep "172.19.0.0/16" | wc -l)
if [ $x -lt 1 ]; then
    iptables -t nat -A POSTROUTING -s 172.19.0.0/16 -j MASQUERADE
fi

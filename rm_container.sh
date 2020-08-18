#!/bin/bash

if [ $# -ne 1 ]; then
    echo "$0 <container_name>"
    exit 1
fi

if [ -z "$1" ]; then
    echo "invalid container name"
    exit 1
fi

if [ $(id -u) -ne 0 ]; then
    echo "need root privilege"
    exit 1
fi

if [ ! -d /var/lib/docker_diy/containers/$1 ]; then
    echo "container $1 does not exist"
    exit 1
fi

mount | awk '{print $3}' | grep "/var/lib/docker_diy/containers/$1" |
while read x; do
    echo "umount $x"
    umount $x
    if [ $? -ne 0 ]; then
        echo "failed to umount $x"
        exit 1
    fi
done

rm -rf "/var/lib/docker_diy/containers/$1"
if [ $? -eq 0 ]; then
    echo "$1 was deleted"
    exit 0
fi

echo "Oops failed"
exit 1

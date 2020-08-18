#!/bin/bash

###########################################################
#
#  This tool is to export docker image, say ubuntu:latest,
# into tarball, and then untar it under, in this case,
# /var/lib/docker_diy/images/ubuntu/latest.
#
###########################################################
#

IMAGE_ROOT=/var/lib/docker_diy/images

function usage() {
cat << _EOF_
$0 docker_image
# eg1. docker_image bosybox
# eg2 docker_image ubuntu:latest
_EOF_
}

if [ $# -ne 1 ]; then
    usage
    exit 1
fi

if [ $(id -u) -ne 0 ]; then
    echo "need root privilege"
    exit 1
fi

IMAGE_NAME=$1
IMAGE_REPO=`echo $1 | awk -F: '{print $1}'`
IMAGE_TAG=`echo $1 | awk -F: '{print $2}'`
if [ -z "$IMAGE_TAG" ]; then
    IMAGE_TAG="latest"
fi

EXPORT_PATH=$IMAGE_ROOT/$IMAGE_REPO/$IMAGE_TAG
if [ -d $EXPORT_PATH ]; then
    echo "$EXPORT_PATH already exist"
    exit 1
fi

TARBALL_NAME=${IMAGE_REPO}_${IMAGE_TAG}
TMP_DOCKER_NAME=${IMAGE_REPO}_${IMAGE_TAG}_EXPORT

if [[ "$(docker images -q $IMAGE_NAME 2> /dev/null)" == "" ]]; then
    docker pull $IMAGE_NAME || exit 1
fi
docker run -d --name $TMP_DOCKER_NAME $IMAGE_NAME &&
docker export -o /tmp/$TARBALL_NAME.tar $TMP_DOCKER_NAME &&
docker stop $TMP_DOCKER_NAME &&
docker rm $TMP_DOCKER_NAME &&
mkdir -p $EXPORT_PATH &&
tar xf /tmp/$TARBALL_NAME.tar -C $EXPORT_PATH

res=$?
if [ $res -eq 0 ]; then
    echo "successfully export image to $EXPORT_PATH"
fi

rm  /tmp/$TARBALL_NAME.tar
exit $res

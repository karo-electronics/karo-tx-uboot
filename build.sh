#!/bin/sh
set -e
UBOOT_VERSION=1.1.6

export BUILD_DIR=../.build-uboot-${UBOOT_VERSION}
export MAKEALL_LOGDIR=../.uboot-${UBOOT_VERSION}.log

export CROSS_COMPILE=arm-xscale-linux-gnu-
# make sure ${CROSS_COMPILE}gcc can be found via $PATH e.g.:
#PATH=/usr/local/arm/cross-gcc-4.2.0-soft/i686-pc-linux-gnu/bin:$PATH

[ -d "${BUILD_DIR}" ] || mkdir -p "${BUILD_DIR}"
[ -d "${MAKEALL_LOGDIR}" ] || mkdir -p "${MAKEALL_LOGDIR}"

# canonicalize path names
BUILD_DIR=$(cd "${BUILD_DIR}" && pwd)
MAKEALL_LOGDIR=$(cd "${MAKEALL_LOGDIR}" && pwd)

if [ $# = 0 -a ! -f ${BUILD_DIR}/include/config.h ];then
    sh MAKEALL triton320
else
    make "$@"
fi
if [ -s ${BUILD_DIR}/u-boot ];then
    ${CROSS_COMPILE}size ${BUILD_DIR}/u-boot
fi

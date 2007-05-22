#!/bin/sh
set -e

export BUILD_DIR=../.build-uboot-1.1.6
export MAKEALL_LOGDIR=../.uboot-1.1.6.log
export CROSS_COMPILE=arm-softfloat-linux-gnu-

[ -d "${BUILD_DIR}" ] || mkdir -p "${BUILD_DIR}"
[ -d "${MAKEALL_LOGDIR}" ] || mkdir -p "${MAKEALL_LOGDIR}"

BUILD_DIR=$(cd "${BUILD_DIR}" && pwd)
MAKEALL_LOGDIR=$(cd "${MAKEALL_LOGDIR}" && pwd)

sh MAKEALL triton320

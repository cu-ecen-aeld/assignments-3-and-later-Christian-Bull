#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git

# older kernel version requires too many downgrades, not worth using
# KERNEL_VERSION=v5.15.163
# CONFIG_PREFIX=/tmp/aeld/rootfs ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu-


KERNEL_VERSION=v6.1.148
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
CROSS_COMPILE_DIR=/opt/arm-gnu-toolchain/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu
WRITER_APP=/home/cbull/projects/cu-boulder/ecea-5305/assignment-1-Christian-Bull/finder-app

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here

    # generate default config
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    # builds kernal
    make -j 8 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all

fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs && cd ${OUTDIR}/rootfs 
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git --depth 1 --single-branch --branch ${BUSYBOX_VERSION}
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
else
    cd busybox
fi

# TODO: Make and install busybox
make distclean
make defconfig
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=/${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
cd ${OUTDIR}/rootfs
lib1=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter" | awk -F': ' '{print $2}' | tr -d '[]')
lib1=$(basename "$lib1")

libs=($(${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library" | awk -F'[][]' '{print $2}'))

# find on filesystem
cp $(find /usr/ /opt/ -name "$lib1") ${OUTDIR}/rootfs/lib

for lib in "${libs[@]}"; 
do
    echo "finding $lib"
    file=$(find /usr/ /opt/ -name "$lib" 2>/dev/null | head -n 1)
    cp "$file" ${OUTDIR}/rootfs/lib64
done

# TODO: Make device nodes
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${WRITER_APP}
make clean
make writer CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
cp writer start-qemu-app.sh start-qemu-terminal.sh autorun-qemu.sh ${OUTDIR}/rootfs/home/
cp finder.sh finder-test.sh ${OUTDIR}/rootfs/home/

mkdir -p ${OUTDIR}/rootfs/home/conf
cp conf/username.txt conf/assignment.txt ${OUTDIR}/rootfs/home/conf

# TODO: Chown the root directory

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio

gzip -f ${OUTDIR}/initramfs.cpio
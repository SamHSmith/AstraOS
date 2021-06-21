#!/bin/sh

# Absolute path to this script, e.g. /home/user/bin/foo.sh
SCRIPT=$(readlink -f "$0")
# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH=$(dirname "$SCRIPT")
#echo $SCRIPTPATH
cd $SCRIPTPATH

QEMU_FLAGS="-machine virt -cpu rv64 -smp 4 -m 512M -serial stdio -bios none -kernel bin/kernel.bin -display sdl"
QEMU="./qemu/build/qemu-system-riscv64"

CFLAGS="$CFLAGS -g -Isrc/cyclone_crypto -Isrc/cyclone_crypto/common"
CFLAGS="$CFLAGS $(cat always_to_be_used_compiler_flags)"
#echo $CFLAGS

KERNEL_SOURCES="src/*.s src/*.c src/cyclone_crypto/hash/sha512.c src/cyclone_crypto/common/cpu_endian.c userland/aos_syscalls.s"

SQUARE_SOURCES="square_src/elf.c userland/aos_syscalls.s src/printf.c"


if [ "$1" = "clean" ]
then
rm -drf bin
fsmake/build.sh clean
fsread/build.sh clean

elif [ "$1" = "run" ]
then
 
$QEMU $QEMU_FLAGS

elif [ "$1" = "debug" ]
then
 
$QEMU $QEMU_FLAGS -s -S
 
else

#do build

if [ -z ${CC+x} ]; then CC=riscv64-unknown-elf-gcc; fi

mkdir -p bin

#build fsmake and fsread
fsmake/build.sh
fsread/build.sh
cp fsmake/bin/* bin
cp fsread/bin/* bin

$CC $CFLAGS -T virt.lds -o bin/kernel.bin $KERNEL_SOURCES
$CC $CFLAGS -o bin/square $SQUARE_SOURCES

mkdir -p disk_dir/partitions
cp bin/square disk_dir/partitions/super_cool_square
rm -f drive1.dsk
bin/fsmake drive1.dsk disk_dir/

fi
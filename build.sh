#!/bin/sh

# Absolute path to this script, e.g. /home/user/bin/foo.sh
SCRIPT=$(readlink -f "$0")
# Absolute path this script is in, thus /home/user/bin
SCRIPTPATH=$(dirname "$SCRIPT")
#echo $SCRIPTPATH
cd $SCRIPTPATH

QEMU_FLAGS="-machine virt -cpu rv64 -smp 4 -m 512M -serial stdio -bios none -kernel bin/kernel.bin -display sdl -s"
QEMU="./qemu/build/qemu-system-riscv64"

CFLAGS="$CFLAGS -g -Isrc/cyclone_crypto -Isrc/cyclone_crypto/common"
CFLAGS="$CFLAGS $(cat always_to_be_used_compiler_flags)"
#echo $CFLAGS

KERNEL_SOURCES="src/*.s src/kernel.c src/printf.c src/cyclone_crypto/hash/sha512.c src/cyclone_crypto/common/cpu_endian.c userland/aos_syscalls.s common/spinlock.s common/atomics.s common/stacktrace.s"

SQUARE_SOURCES="square_src/elf.c userland/aos_syscalls.s"

TREE_SOURCES="tree/tree.c userland/aos_syscalls.s"

CAT_SOURCES="cat/cat.c userland/aos_syscalls.s"

ECHO_SLOWLY_SOURCES="echo_slowly/echo_slowly.c userland/aos_syscalls.s"

RAY2D_SOURCES="ray2d/ray2d.c userland/aos_syscalls.s"

DAVE_TERMINAL_SOURCES="dave_terminal/dave.c userland/aos_syscalls.s common/spinlock.s"

VRMS_SOURCES="vrms/vrms.c userland/aos_syscalls.s"

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
 
$QEMU $QEMU_FLAGS -S
 
else

#do build

if [ -z ${RCC+x} ]; then RCC=riscv64-unknown-elf-gcc; fi

mkdir -p bin

#build fsmake and fsread
fsmake/build.sh
fsread/build.sh
cp fsmake/bin/* bin
cp fsread/bin/* bin

$RCC $CFLAGS -T virt.lds -o bin/kernel.bin $KERNEL_SOURCES
$RCC $CFLAGS -o bin/square $SQUARE_SOURCES
$RCC $CFLAGS -o bin/tree $TREE_SOURCES
$RCC $CFLAGS -o bin/echo_slowly $ECHO_SLOWLY_SOURCES
$RCC $CFLAGS -o bin/cat $CAT_SOURCES
$RCC $CFLAGS -o bin/ray2d $RAY2D_SOURCES
$RCC $CFLAGS -o bin/dave_terminal $DAVE_TERMINAL_SOURCES
$RCC $CFLAGS -o bin/vrms $VRMS_SOURCES

mkdir -p disk_dir/partitions
cp bin/square disk_dir/partitions/super_cool_square
cp bin/ray2d disk_dir/partitions/ray2d
cp bin/dave_terminal disk_dir/partitions/dave_terminal
cp bin/vrms disk_dir/partitions/vrms
cp bin/tree disk_dir/partitions/tree
cp bin/echo_slowly disk_dir/partitions/echo_slowly
cp bin/cat disk_dir/partitions/cat
rm -f drive1.dsk
bin/fsmake drive1.dsk disk_dir/

fi

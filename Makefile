CFLAGS+= -g
CFLAGS+= -mcmodel=medany #-Ofast
CFLAGS+=-static -ffreestanding -nostdlib -static-libgcc
CFLAGS+=-march=rv64g -mabi=lp64 -Isrc/cyclone_crypto -Isrc/cyclone_crypto/common
LDFLAGS=

QEMU=./qemu/build/qemu-system-riscv64

QEMU_FLAGS=-machine virt -cpu rv64 -smp 4 -m 512M -serial pipe:./pipe -bios none -kernel kernel.bin -display sdl

SOURCES=$(wildcard src/*.s) $(wildcard src/*.c) src/cyclone_crypto/hash/sha512.c src/cyclone_crypto/common/cpu_endian.c

kernel.bin: virt.lds
	riscv64-unknown-elf-gcc $(CFLAGS) $(LDFLAGS) -T $< -o $@ $(SOURCES)

ELFSOURCES= elfsrc/elf.c userland/aos_syscalls.s

elf: elfsrc/elf.c src/user.s
	riscv64-unknown-elf-gcc $(CFLAGS) -o $@ $(ELFSOURCES)

run: clean kernel.bin
	mkfifo pipe.in pipe.out
	$(QEMU) $(QEMU_FLAGS) &
	cat pipe.out
	rm -drf pipe.in pipe.out

debug: clean kernel.bin
	mkfifo pipe.in pipe.out
	$(QEMU) $(QEMU_FLAGS) -s -S &
	cat pipe.out
	rm -drf pipe.in pipe.out

clean:
	make -C fsmake clean
	make -C fsread clean
	rm -drf pipe.in pipe.out
	rm -drf kernel.bin
	rm -f elf


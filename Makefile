CFLAGS= -g -mcmodel=medany -Wall -Ofast
CFLAGS+=-static -ffreestanding -nostdlib
CFLAGS+=-march=rv64gc -mabi=lp64
LDFLAGS=

QEMU=./qemu/build/qemu-system-riscv64

QEMU_FLAGS=-machine virt -cpu rv64 -smp 4 -m 512M -serial pipe:./pipe -bios none -kernel kernel.bin -display sdl

kernel.bin: virt.lds
		riscv64-unknown-elf-gcc $(CFLAGS) $(LDFLAGS) -T $< -o $@ $(wildcard src/*.s) $(wildcard src/*.c)

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
	rm -drf pipe.in pipe.out
	rm -drf kernel.bin hdd.dsk


CFLAGS= -g -mcmodel=medany -Wall
CFLAGS+=-static -ffreestanding -nostdlib
CFLAGS+=-march=rv64gc -mabi=lp64
LDFLAGS=
DRIVE=hdd.dsk

kernel.bin: virt.lds
		riscv64-unknown-elf-gcc $(CFLAGS) $(LDFLAGS) -T $< -o $@ $(wildcard src/*.s) $(wildcard src/*.c)

hdd:
	./make_hdd.sh

run: clean kernel.bin hdd
	qemu-system-riscv64 -machine virt -cpu sifive-u54 -smp 4 -m 128M -nographic -serial mon:stdio -bios none -kernel kernel.bin -drive if=none,format=raw,file=$(DRIVE),id=foo -device virtio-blk-device,scsi=off,drive=foo

debug: clean kernel.bin hdd
	qemu-system-riscv64 -machine virt -cpu sifive-u54 -smp 4 -m 128M -nographic -serial mon:stdio -bios none -kernel kernel.bin -drive if=none,format=raw,file=$(DRIVE),id=foo -device virtio-blk-device,scsi=off,drive=foo -s -S
clean:
	rm -drf kernel.bin hdd.dsk

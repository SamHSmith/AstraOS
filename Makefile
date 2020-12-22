CFLAGS= -g -mcmodel=medany
CFLAGS+=-static -ffreestanding -nostdlib 
CFLAGS+=-march=rv64gc -mabi=lp64
LDFLAGS=
DRIVE=hdd.dsk

objects = trap.o boot.o kernel.o

%.o: src/%.c
		riscv64-unknown-linux-gnu-gcc $(CFLAGS) $< -c

%.o: src/%.s
		riscv64-unknown-linux-gnu-gcc $(CFLAGS) $< -c

kernel.bin: virt.lds $(objects)
		riscv64-unknown-linux-gnu-ld $(LDFLAGS) -T $< -o $@ $(objects)

isos.iso: kernel.bin
	rm -drf iso
	mkdir iso
	mkdir iso/boot
	mkdir iso/boot/grub
	cp $< iso/boot
	echo 'set timeout=0\nset default=0\n\nmenuentry "InterstellarOS" {\n  multiboot /boot/kernel.bin\n  boot\n}' > iso/boot/grub/grub.cfg
	grub-mkrescue --output=$@ iso
	rm -drf iso

run: clean kernel.bin
	qemu-system-riscv64 -machine virt -cpu rv64 -smp 1 -m 512M -nographic -serial mon:stdio -bios none -kernel kernel.bin -drive if=none,format=raw,file=$(DRIVE),id=foo -device virtio-blk-device,scsi=off,drive=foo

clean:
	rm -drf $(objects) kernel.bin hdd.dsk

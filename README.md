### AstraOS's development is hosted on [source hut](https://sr.ht/~samhsmith/AstraOS/). Talk to us and report bugs there.
### Warning: this project doesn't actually have a readme yet. I don't believe in creating such things before there is more substance to the project. 

a-p-petrosyan insisted however on renaming the rivos_manjaro_guide as a temporary readme.

## Arch-based guide for bulding and running Astra

- First clone the repository
```
git clone https://git.sr.ht/~samhsmith/AstraOS
cd AstraOS
git submodule update --init
cd ..
```

 - Clone the `risc-v` gnu toolchain
```
git clone https://github.com/riscv/riscv-gnu-toolchain.git
```

 - Install build dependencies
```
sudo pacman -S autoconf automake curl python3 mpc mpfr gmp gawk base-devel bison flex texinfo gperf libtool patchutils bc zlib expat
```

- Build the toolchain. Install to /opt/riscv/ this will take a while
```
cd riscv-gnu-toolchain
sudo mkdir /opt/riscv
PATH=/opt/riscv/bin:$PATH
./configure --prefix=/opt/riscv && sudo make -j$(nproc)
cd ..
```

- Now install the run-time dependencies.
```
sudo pacman -Sy ninja ceph glusterfs libiscsi python python-sphinx spice-protocol xfsprogs
```

- Now build the virtual machine that is used to run rivos
```
cd AstraOS
mkdir qemu/build
cd qemu/build
../configure --target-list=riscv64-softmmu
make -j$(nproc)
cd ../..
```

- Run the virtual machine
```
make run
```

# Notes

By default you'll get my super gnarly (ultra custom) keyboard layout that you probably
don't want to use.

Changing the keyboard layout can be done as follows:

In `src/tempuser.h`

- Sam's custom layout:
```
#include "samorak.h"
//#include "qwerty.h"
```

- US Qwerty:
```
//#include "samorak.h"
#include "qwerty.h"
```

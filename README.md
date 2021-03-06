# LibreTech U-boot Tree

## Board support
-------------
 - LibreTech CC/ : branch `libretech-cc` for Amlogic U-Boot, branch `u-boot/v2017.11/libretech-cc` for v2017.11 based U-Boot

## Mainline U-boot for LibreTech CC
-----------
### How to build and install

Big FAT Warning :
Support for Mainline U-boot will require Linux 4.16, or the Linux 4.14 with patches you can find at : https://github.com/libre-computer-project/libretech-linux
If you run another kernel, stick to the Amlogic U-Boot in the next section.

Checkout vendor u-boot source and prepare environment:

```
# git clone https://github.com/BayLibre/u-boot.git -b libretech-cc libretech-cc-amlogic-u-boot
# wget https://releases.linaro.org/archive/13.11/components/toolchain/binaries/gcc-linaro-aarch64-none-elf-4.8-2013.11_linux.tar.xz
# wget https://releases.linaro.org/archive/13.11/components/toolchain/binaries/gcc-linaro-arm-none-eabi-4.8-2013.11_linux.tar.xz
# tar xvfJ gcc-linaro-aarch64-none-elf-4.8-2013.11_linux.tar.xz
# tar xvfJ gcc-linaro-arm-none-eabi-4.8-2013.11_linux.tar.xz
# export PATH=$PWD/gcc-linaro-aarch64-none-elf-4.8-2013.11_linux/bin:$PWD/gcc-linaro-arm-none-eabi-4.8-2013.11_linux/bin:$PATH
```

Build vendor u-boot:

```
# cd libretech-cc-amlogic-u-boot
libretech-cc-amlogic-u-boot # make libretech_cc_defconfig
libretech-cc-amlogic-u-boot # make
libretech-cc-amlogic-u-boot # export FIPDIR=$PWD/fip
libretech-cc-amlogic-u-boot # cd -
```

Checkout mainline U-boot :
```
# git clone https://github.com/BayLibre/u-boot.git -b u-boot/v2017.11/libretech-cc libretech-cc-u-boot
# wget https://releases.linaro.org/components/toolchain/binaries/7.2-2017.11/aarch64-elf/gcc-linaro-7.2.1-2017.11-x86_64_aarch64-elf.tar.xz
# tar xvfJ gcc-linaro-7.2.1-2017.11-x86_64_aarch64-elf.tar.xz
# export PATH=$PWD/gcc-linaro-7.2.1-2017.11-x86_64_aarch64-elf/bin:$PATH
# cd libretech-cc-u-boot
libretech-cc-u-boot # make libretech-cc_defconfig
libretech-cc-u-boot # make CROSS_COMPILE=aarch64-elf-
```

Generate the final image :
```
libretech-cc-u-boot # mkdir fip
libretech-cc-u-boot # cp $FIPDIR/gxl/bl2.bin fip/
libretech-cc-u-boot # cp $FIPDIR/gxl/acs.bin fip/
libretech-cc-u-boot # cp $FIPDIR/gxl/bl21.bin fip/
libretech-cc-u-boot # cp $FIPDIR/gxl/bl30.bin fip/
libretech-cc-u-boot # cp $FIPDIR/gxl/bl301.bin fip/
libretech-cc-u-boot # cp $FIPDIR/gxl/bl31.img fip/
libretech-cc-u-boot # cp u-boot.bin fip/bl33.bin
libretech-cc-u-boot # $FIPDIR/blx_fix.sh fip/bl30.bin fip/zero_tmp fip/bl30_zero.bin fip/bl301.bin fip/bl301_zero.bin fip/bl30_new.bin bl30
libretech-cc-u-boot # $FIPDIR/acs_tool.pyc fip/bl2.bin fip/bl2_acs.bin fip/acs.bin 0
libretech-cc-u-boot # $FIPDIR/blx_fix.sh fip/bl2_acs.bin fip/zero_tmp fip/bl2_zero.bin fip/bl21.bin fip/bl21_zero.bin fip/bl2_new.bin bl2
libretech-cc-u-boot # $FIPDIR/gxl/aml_encrypt_gxl --bl3enc --input fip/bl30_new.bin
libretech-cc-u-boot # $FIPDIR/gxl/aml_encrypt_gxl --bl3enc --input fip/bl31.img
libretech-cc-u-boot # $FIPDIR/gxl/aml_encrypt_gxl --bl3enc --input fip/bl33.bin
libretech-cc-u-boot # $FIPDIR/gxl/aml_encrypt_gxl --bl2sig --input fip/bl2_new.bin --output fip/bl2.n.bin.sig
libretech-cc-u-boot # $FIPDIR/gxl/aml_encrypt_gxl --bootmk --output fip/u-boot.bin --bl2 fip/bl2.n.bin.sig --bl30 fip/bl30_new.bin.enc --bl31 fip/bl31.img.enc --bl33 fip/bl33.bin.enc
```
Binaries should be available in `fip` directory :
```
fip/
├── ...
├── u-boot.bin (for eMMC)
├── u-boot.bin.sd.bin (for SDCard)
└── ...
```

### Install on SD Card

To install on blank SDCard (assuming SDCard in on /dev/mmcblk0):
```
# sudo mkfs.vfat /dev/mmcblk0p1
# sudo dd if=fip/u-boot.bin.sd.bin of=/dev/mmcblk0 conv=fsync,notrunc bs=1 count=444
# sudo dd if=fip/u-boot.bin.sd.bin of=/dev/mmcblk0 conv=fsync,notrunc bs=512 skip=1 seek=1
# sync
```

### Install on eMMC

To install on an eMMC you will first need to boot the board with an eMMC attached to it. From the running Linux on the board follow these steps:

Clean the first block of the eMMC where u-boot will be copied:
```
$ dd if=/dev/zero of=/dev/mmcblk0 bs=512 count=1
```

Then copy u-boot to the eMMC (assuming the eMMC device is mmcblk0) by running :
```
$ dd if=u-boot.bin of=/dev/mmcblk0 bs=512 seek=1
```

#### [Optionnal extra step] configure u-boot to load and save environment on eMMC
i.e. to save the u-boot environment on the eMMC and completely get rid of the SD card.

- Apply this patch to mailine u-boot (previously cloned repo) `libretech-cc-u-boot`:
```
diff --git a/configs/libretech-cc_defconfig b/configs/libretech-cc_defconfig
index 7eb4e38f5001..3414191e873c 100644
--- a/configs/libretech-cc_defconfig
+++ b/configs/libretech-cc_defconfig
@@ -8,7 +8,7 @@ CONFIG_DEBUG_UART=y
 # CONFIG_ENV_IS_NOWHERE is not set
 CONFIG_ENV_IS_IN_FAT=y
 CONFIG_ENV_FAT_INTERFACE="mmc"
-CONFIG_ENV_FAT_DEVICE_AND_PART="0:auto"
+CONFIG_ENV_FAT_DEVICE_AND_PART="1:auto"
 CONFIG_ENV_FAT_FILE="uboot.env"
 CONFIG_FAT_WRITE=y
 # CONFIG_DISPLAY_CPUINFO is not set
```

This will tell u-boot to save and load the environment from the eMMC (device 1) and not the SD card (device 0).

- Follow the previous steps to `Generate the final image`.
- Re-flash u-boot on the eMMC following `Install on eMMC`.

If not already done, create a FAT partition on the eMMC:
```
# fdisk /dev/mmcblk0
n
p
1
100
400
w
# mkfs.vfat /dev/mmcblk0p1
```

You can reboot and saveenv/loadenv from eMMC.

## Amlogic Vendor U-boot for LibreTech CC
-----------
### How to build and install

Checkout source and prepare environment:

```
# git clone https://github.com/BayLibre/u-boot.git -b libretech-cc
# wget https://releases.linaro.org/archive/13.11/components/toolchain/binaries/gcc-linaro-aarch64-none-elf-4.8-2013.11_linux.tar.xz
# wget https://releases.linaro.org/archive/13.11/components/toolchain/binaries/gcc-linaro-arm-none-eabi-4.8-2013.11_linux.tar.xz
# tar xvfJ gcc-linaro-aarch64-none-elf-4.8-2013.11_linux.tar.xz
# tar xvfJ gcc-linaro-arm-none-eabi-4.8-2013.11_linux.tar.xz
# export PATH=$PWD/gcc-linaro-aarch64-none-elf-4.8-2013.11_linux/bin:$PWD/gcc-linaro-arm-none-eabi-4.8-2013.11_linux/bin:$PATH
```

Build:

```
# cd libretech-u-boot
libretech-u-boot # make libretech_cc_defconfig
libretech-u-boot # make
```

Binaries should be available in `fip` directory :
```
fip/
├── ...
├── u-boot.bin (for eMMC)
├── u-boot.bin.sd.bin (for SDCard)
└── ...
```

Install on blank SDCard (assuming SDCard in on /dev/mmcblk0):
```
# sudo mkfs.vfat /dev/mmcblk0p1
# sudo dd if=fip/u-boot.bin.sd.bin of=/dev/mmcblk0 conv=fsync,notrunc bs=1 count=444
# sudo dd if=fip/u-boot.bin.sd.bin of=/dev/mmcblk0 conv=fsync,notrunc bs=512 skip=1 seek=1
# sync
```

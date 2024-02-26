#! /bin/bash
qemu-system-i386 -D ./log.txt \
                 -d cpu_reset \
                 -no-reboot \
                 -vga std \
                 -drive id=disk,format=raw,file=bin/vainos.img
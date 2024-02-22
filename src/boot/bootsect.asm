[org 0x7c00]
[bits 16]

jmp bootsect_code
nop

db "vainos", 0, 0   ; 8 bytes, name of the operating system
dw 512            ; 2 bytes, bytes per sector, each one is 512 bytes long
db 4              ; every cluster on disk is 4 sectors long
                  ; (default value generated by `mkfs.vfat -v -F16` from makefile)
dw 4              ; 2 bytes, reserved sectors, used to calculate the starting
                  ; sector of the first FAT; the boot sector and three extra
                  ; useless sectors in the 10 mb disk
db 2              ; 1 byte, numer of file allocation tables, the prefered
                  ; amout is 2 for backup
dw 512            ; 2 bytes, number of root directory entries (file or
                  ; directories) inside the root directory, the max amount
                  ; is 512 items, this is the recommended value
dw 65535          ; small number of sectors in the volume
                  ; NOTE: for my tests purposes, this amount is simply set
                  ; with an arbitrary value
db 0xf8           ; media descriptor, the value must be 0xf8 for unknown capacity
dw 20             ; amount of sectors per FAT, we have 2 FATs of 20 sectors
dw 63             ; sectors per track, used for LBA/CHS conversion, the
                  ; hd disk contains 63 sectors per track
dw 16             ; number of heads (16 heads on a standard hd disk)
dd 0              ; hidden sectors, the amount of sectors between the first
                  ; sector of the disk and the beginning of the volume
dd 0              ; large number of sector, unused in our case as we use
                  ; the small amount of sectors
db 0x29           ; extended boot signature, must be equal to 0x29 to indicate
                  ; that the next items of the EBPB are set
dd 0xffff         ; serial number, we set the 32 bits to 1, related to the
                  ; hardware, it does not matter for us...


db 0              ; drive number, 0 for hd disk
db 0              ; reserved and unused byte
db "VAINOS", 0, 0, 0, 0, 0    ; 11 bytes long volume label string
                  ; TODO: #7 must be equal to NO NAME if the root directory
                  ; entry does not exist. This entry does not exist yet,
                  ; but I will have to change it once I get the root
                  ; directory label entry properties...
db "FAT16", 0, 0, 0 ; 8 bytes long file system name

; fill all the bytes between the end of the OEM and the expected starting byte
; of the boot sector code
times 0x3e - ($-$$) db 0

bootsect_code:

KERNEL_OFFSET equ 0x1000 ; The same one we used when linking the kernel


mov [BOOT_DRIVE], dl ; Remember that the BIOS sets us the boot drive in 'dl' on boot
mov bp, 0x9000
mov sp, bp
mov bx, MSG_REAL_MODE 
call print
call print_nl
call load_kernel ; read the kernel from disk
call switch_to_pm ; disable interrupts, load GDT,  etc. Finally jumps to 'BEGIN_PM'
jmp $ ; Never executed

%include "boot/print16.asm"
%include "boot/print16_hex.asm"
%include "boot/disk16.asm"
%include "boot/gdt32.asm"
%include "boot/print32.asm"
%include "boot/switch32.asm"

[bits 16]
load_kernel:
    mov bx, MSG_LOAD_KERNEL
    call print
    call print_nl

    mov bx, KERNEL_OFFSET ; Read from disk and store in 0x1000
    mov dh, 16
    mov dl, [BOOT_DRIVE]
    call disk_load
    ret

[bits 32]
BEGIN_PM:
    mov ebx, MSG_PROT_MODE
    call print_string_pm
    cli
    call KERNEL_OFFSET ; Give control to the kernel
    jmp $ ; Stay here when the kernel returns control to us (if ever)


BOOT_DRIVE db 0 ; It is a good idea to store it in memory because 'dl' may get overwritten
MSG_REAL_MODE db "Load 16bit", 0
MSG_PROT_MODE db "Load 32bit    ", 0
MSG_LOAD_KERNEL db "Loading kernel", 0

; padding
times 510 - ($-$$) db 0
dw 0xaa55
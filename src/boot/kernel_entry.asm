[bits 32]
; mode
;  0: text
;  1: pallette
;  2: 15bpp
;  3: 16bpp
;  4: 24bpp
;  5: 32bpp

[extern main] ; Define calling point. Must have same name as kernel.c 'main' function
call main ; Calls the C function. The linker will know where it is placed in memory
jmp $
%include "cpu/interrupt.asm"
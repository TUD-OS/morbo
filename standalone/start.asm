        ;; Set stack and jump to _main

        CPU P3
        
        EXTERN main, __exit
        GLOBAL _mbheader, _start, jmp_multiboot
        
        SECTION .text._start EXEC NOWRITE ALIGN=4
_mbheader:
        align 4
        dd 1BADB002h            ; magic
        dd 3h                   ; features (page aligned modules, mem info, video mode table)
        dd -(3h + 1BADB002h)    ; checksum
        dd 0h                   ; align to 8 byte
_mbi2_s:                        ; mbi 2 header
        dd 0xe85250d6                          ; magic
        dd 0x0                                 ; features
        dd (_mbi2_e - _mbi2_s)                 ; size
        dd -(0xe85250d6 + (_mbi2_e - _mbi2_s)) ; checksum
                                ; end tag
        dw 0x0                  ; type
        dw 0x0                  ; flags
        dd 0x8                  ; size
_mbi2_e:

_start:
        mov     esp, _stack
        mov     edx, ebx
        push    __exit
        jmp      main

jmp_multiboot:
        xchg    eax, ebx
        mov     eax, 2BADB002h
        jmp     edx
        
        SECTION .bss
        resb 4096
_stack: 
        
        ;; EOF

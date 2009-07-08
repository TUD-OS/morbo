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

_start:
        lea     esp, [_stack]
        mov     edx, ebx
        call    main
        call    __exit


jmp_multiboot:
        xchg    eax, ebx
        mov     eax, 2BADB002h
        jmp     edx
        
        SECTION .bss
        resb 4096
_stack: 
        
        ;; EOF
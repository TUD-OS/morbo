        ;; Set stack and jump to _main

        CPU P3
        
        EXTERN main, __exit
        GLOBAL _mbheader, _start, jmp_multiboot
        
        SECTION .text._start EXEC NOWRITE ALIGN=4
_mbheader:
        align 4
        dd 1BADB002h            ; magic
        dd 3h                   ; features
        dd -(3h + 1BADB002h)    ; checksum

_start:
        lea     esp, [_stack]
        push    ebx
        call    main
        push    eax
        call    __exit


jmp_multiboot:
        mov     eax, 2BADB002h
        mov     ebx, [esp + 4]
        jmp     dword [esp + 8]
        
        SECTION .bss
        resb 4096
_stack: 
        
        ;; EOF
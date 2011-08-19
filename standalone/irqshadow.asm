        ;; Test interrupt shadow

        CPU PENTIUM

        GLOBAL main
        EXTERN serial_init, out_char, printf

        SECTION .data._dt

        %define SEL_RING0_CODE 8
        %define SEL_RING0_DATA 16

gdt:    dd 0
        dd 0
        ;; CODE
        dw 0xFFFF
        dw 0
        db 0
        db 10011010b
        db 11001111b
        db 0
        ;; DATA
        dw 0xFFFF
        dw 0
        db 0
        db 10010010b
        db 11001111b
        db 0
gdt_end:

        %macro idt_irq_gate 1
        dw 0
        dw %1
        dw 1000111000000000b   ; 32-bit Interrupt gate, DPL 0
        dw 0
        %endmacro

        align 32
idt:
        %rep 255
        dd 0
        dd 0
        %endrep
        %rep 1
        idt_irq_gate SEL_RING0_CODE
        %endrep
idt_end:

gdt_ptr:
        dw gdt_end - gdt  - 1
        dd gdt
idt_ptr:
        dw idt_end - idt - 1
        dd idt


        SECTION .text.main EXEC NOWRITE ALIGN=4

main:
        ;; Serial port setup
        call serial_init

        mov eax, '<'
        call out_char

        ;; Enable special purpose exception handlers
        mov eax, gp_handler
        mov [idt + 8*13 + 0], ax
        shr eax, 16
        mov [idt + 8*13 + 6], ax

        push 0
        popfd
        
        lgdt [gdt_ptr]
        lidt [idt_ptr]

        jmp SEL_RING0_CODE:.reload_cs
.reload_cs:

        mov ax, SEL_RING0_DATA
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
        mov ss, ax
        
        mov eax, '>'
        call out_char
        
        ;; Install IRQ handler (jmp [8] )
        mov dword [0], 0x000825ff
        mov dword [4], 0
        mov dword [8], irqhandler
        
        ;; Program APIC to periodic intervals

        ;; Enable in SVR
        mov eax, [0xFEE000F0]
        or  ah, 1
        mov [0xFEE000F0], eax


        mov dword [0xFEE003E0], 1010b            ; Tick with bus clock / 128
        mov dword [0xFEE00320], 0xFF | (1 << 17) ; Enable timer LVT, periodic

        mov dword [0xFEE00350], 0x10000          ; Mask LINT0/LINT1
        mov dword [0xFEE00360], 0x10000
        
        mov dword [0xFEE00380], 1                ; Initial count
        mov dword [0xFEE00080], 0xEF             ; TPR


lo:
never_here1:
        sti
never_here2:
        nop

        ;; The IRQ handler doesn't destroy EBX
        mov ebx, SEL_RING0_DATA
        mov ss, ebx
never_here3:
        nop
        nop
        cli
never_here4:
        jmp lo

irqhandler:
        ;; No point in saving registers. The test case doesn't use
	;; any.

        mov ebp, esp
        
        mov eax, 'I'
        call out_char

        mov edi, [esp]
        mov esi, bad_eips

        ;; push edi
        ;; push .msg
        ;; call printf
        ;; add esp, 8
        
.check:
        lodsd
        test eax, eax
        jz .done
        cmp edi, eax
        je .failed
        jmp .check
.done:

        mov eax, 'E'
        call out_char
        
        mov dword [0xFEE000B0], 0 ; EOI

        cmp esp, ebp
        jne .stack_fail
        

        mov edi, [esp]
        cmp edi, lo
        jbe .catastrophic_fail
        cmp edi, irqhandler
        ja  .catastrophic_fail

        iret
        
        ;; .msg db "eip %x", 13
.failed:
        mov eax, '!'
        call out_char
        cli
        hlt

.catastrophic_fail:
        mov eax, 'C'
        call out_char
        cli
        hlt

.stack_fail:
        mov eax, 'S'
        call out_char
        cli
        hlt

gp_handler:
        push .msg
        call printf
        cli
        hlt

.msg db "%x %x", 10, 0
        
bad_eips:
        dd never_here1
        dd never_here2
        dd never_here3
        dd never_here4
        dd 0
        
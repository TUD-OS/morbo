
        CPU P3

	%DEFINE RELBASE 0x9F000
        
        GLOBAL torealmode
        
        SECTION .text.torealmode EXEC NOWRITE ALIGN=16
	;; EAX = seg:off
	;; Overwrites memory at RELBASE
torealmode:
	mov [.target_off], ax
	shr eax, 16
	mov [.target_sel], ax
	
	call .get_addr
.get_addr:
	pop ebp

	cmp ebp, RELBASE + (.get_addr - torealmode)
	je .already_relocated
	mov esi, torealmode
	mov edi, RELBASE
	mov ecx, .end - torealmode
	rep movsb
	jmp RELBASE
.already_relocated:

	xor eax, eax
	xor ebx, ebx
	xor ecx, ecx
	xor ebx, ebx
	xor edx, edx
	xor esp, esp
	
	lidt [RELBASE + .idt_ptr - torealmode]

	lgdt [RELBASE + .gdt_ptr - torealmode]
	;jmp far 0x8:(RELBASE + .cs_switch - torealmode)
	jmp far 0x8:(.cs_switch - torealmode)
.cs_switch:
	BITS 16
	mov cx, 0x10
	mov ss, cx
	mov ds, cx
	mov es, cx
	mov fs, cx
	mov gs, cx
	
	mov eax, cr0
	and al, 0xFE
	mov cr0, eax

	jmp far (RELBASE >> 4):(.realmode_entry - torealmode)
.realmode_entry:
	BITS 16
	
	xor cx, cx
	mov ss, cx
	mov ds, cx
	mov es, cx
	mov gs, cx
	mov fs, cx

	;; JMP FAR imm
	db 0xea
.target_off	db 0, 0
.target_sel	db 0, 0

	BITS 32
	
	ALIGN 8
.gdt:
	;; Null descriptor
	dd 0
	dd 0
	;; 16-bit Code
	dd 0x0000FFFF | (RELBASE << 16)
	;dd 0x00409A00 (32-Bit code)
	dd 0x00009A00 | (RELBASE >> 16)
	;; Data
	dd 0x0000FFFF
	dd 0x00009300

.gdt_ptr dw 8*3-1
	dd (.gdt - torealmode) + RELBASE

.idt_ptr dw 256*4-1
	dd 0

.end:
	
        ;; EOF

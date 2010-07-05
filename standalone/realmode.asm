
        CPU P3
        
        GLOBAL torealmode
        
        SECTION .text.torealmode EXEC NOWRITE ALIGN=8
	;; EAX = seg:off
	;; Overwrite 7b00:0000
torealmode:
	mov [.target_off], ax
	shr eax, 16
	mov [.target_sel], ax
	
	call .get_addr
.get_addr:
	pop ebp

	cmp ebp, 0x7B00 + (.get_addr - torealmode)
	je .already_relocated
	mov esi, torealmode
	mov edi, 0x7B00
	mov ecx, .end - torealmode
	rep movsb
	jmp 0x7B00
.already_relocated:

	lgdt [0x7b00 + .gdt_ptr - torealmode]
	jmp far 0x8:(0x7b00 + .cs_switch - torealmode)
.cs_switch:
	mov cx, 0x10
	mov ss, cx
	mov ds, cx
	mov es, cx
	mov fs, cx
	mov gs, cx
	
	lidt [0x7b00 + .idt_ptr - torealmode]

	mov eax, cr0
	and al, 0xFE
	mov cr0, eax

	jmp far 0:(0x7b00 + .realmode_entry - torealmode)
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
	;; 32-bit Code
	dd 0x0000FFFF
	dd 0x00409A00
	;; Data
	dd 0x0000FFFF
	dd 0x00009300

.gdt_ptr dw 8*3
	dd (.gdt - torealmode) + 0x7B00

.idt_ptr dw 256*4
	dd 0

.end:
	
        ;; EOF
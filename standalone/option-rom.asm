	;; -*- Mode: Assembler -*-
	;;

	;; We want 16-bit code.
	BITS 16
	CPU 686

	SECTION .text._start
	EXTERN _init, _bev
	GLOBAL int10_putc

	;; ROM Header
	db 0x55, 0xAA
	db 0		      ; Written after we are assembled. length%512 == 0 is always true for us.
	jmp rom_init

	;; We use the last byte reserved for the initialization jump
	;; to fix up our checksum. This is written by another tool
	;; after we are assembled.
	times 0x6 - ($-$$) db 0
	checksum db 0

	times 0x1A - ($-$$) db 0
	dw pnp_header

old_ss 	dw 0
old_sp  dw 0

pnp_far	dw 0
	dw 0
	
rom_init:
	pushaw			; trash upper 16-bit of registers
	push ds
	push es
	push fs
	push gs
	cld

	mov cx, cs
 	mov ds, cx

	mov ax, es
	mov [pnp_far], ax
	mov [pnp_far + 2], di
	
 	mov es, cx

	call dword _init

	pop gs
	pop fs
	pop es
	pop ds
	popaw
	mov ax, 100000b		; IPL device attached
	retf

	;; AL = character
int10_putc:
	push bx
	push es
	push bp

	mov ah, 0x07
	push ax

	mov ah, 0x03
	mov bh, 0
	int 0x10		; cursor pos -> dx

	mov ax, ss
	mov es, ax
	mov bp, sp
	mov ax, 0x1303
	mov cx, 1
	int 0x10
	pop ax

	pop bp
	pop es
	pop bx
	o32 ret

bootstrap_entry_vector:
	;; Don't save anything. We return using int 18h.
	mov ax, cs
	mov ds, ax
	mov es, ax
	call dword _bev
	int 18h

manufacturer_name db "TUD", 0
product_name	  db "IEEE 1394 BOOT ROM", 0

	ALIGN 16
pci_header:
pnp_header:
	db '$PnP', 0x01				   ; Signature
	db (pnp_header_end - pnp_header + 15) >> 4 ; Length in 16-byte blocks
	dw 0					   ; Next header (0 if none)
	db 0					   ; Reserved
	;; XXXX Fix checksum so that BEV works?
	db 0					   ; Checksum
	dd 0					   ; Device ID
	dw manufacturer_name			   ; Manufacturer
	dw product_name				   ; Product
	db 0x02					   ; Device base type
	db 0x00					   ; Device sub type
	db 0x00					   ; Device interface type
	db 01110100b				   ; Device indicator
	dw 0					   ; BCV
	dw 0					   ; BDV
	dw bootstrap_entry_vector
	dw 0
	dw 0			; Static resource information vector

pnp_header_end:

	;; EOF
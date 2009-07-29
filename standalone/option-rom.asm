	;; -*- Mode: Assembler -*-
	;;

	;; We want 16-bit code.
	BITS 16
	CPU 686

	SECTION .text._start START=0

	;; ROM Header
	db 0x55, 0xAA
	db 0		      ; Written after we are assembled. length%512 == 0 is always true for us.
	jmp rom_init

	;; We use the last byte reserved for the initialization jump
	;; to fix up our checksum. This is written by another tool
	;; after we are assembled.
	times 0x6 - ($-$$) db 0
	checksum db 0

	times 0x18 - ($-$$) db 0
	dw 0			; pci_header

	times 0x1A - ($-$$) db 0
	dw pnp_header

rom_init:
	push es
	mov ax, 0xB800
	mov es, ax
	mov word [es:0], 0x0F30	; white 0
	pop es

	;; Do something...
	
	retf

bootstrap_entry_vector:
	mov ax, 0xB800
	mov es, ax
	mov word [es:0], 0x0F31	; white 1
	cli
	hlt

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
	db 0xF4					   ; Device indicator
	dw 0					   ; BCV
	dw 0					   ; BDV
	dw bootstrap_entry_vector
	dw 0
	dw 0			; Static resource information vector

pnp_header_end:

	;; EOF
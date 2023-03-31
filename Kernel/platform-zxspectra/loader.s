;
;	Boot block for +2A/+3
;

	.area BOOT (ABS)	; 0xFE00 max 512 bytes

	; Really at FE00 but will relocate
	.org 0xBE00

	.byte	0		; +3
	.byte	0		; single sided
	.byte	40		; 40 track
	.byte	9		; 9 spt
	.byte	2		; 512 byte sectors
	.byte	1		; reserved track (meaningless)
	.byte	3		; blocks (meaningless)
	.byte	2		; directory length (meaningless)
	.byte	0x2A		; gap length
	.byte	0x52		; page length
	.byte	0,0,0,0,0	; reserved
counter:.byte	0		; checksum (fixed up by tool)
	; we borrow the checksum space for our counter

;
;	Entered here in 4/7/6/3 mapping with SP FE00 and the disk motor
;	just turned off
;
start:
	di
	ld	bc,#0x7FFD
	ld	a,#0x1B		; 48K ROM, page 3 high when in normal paging
				; video in bank 7
	out	(c),a
	ld	b,#0x1F
	ld	a,#0x14		; Special paging off, motor on, 48K ROM
	out	(c),a
;
;	The boot loader turns the motor off, so turn it back on but wait
;	a bit otherwise some 3.5" disks won't boot right
;
motor_on:
	ld	hl,#0xFE00
	ld	de,#0xBE00
	ld	bc,#512
	ldir
	jp	remap
	; We are now at the expected address
remap:
	ld	sp,#0xBE00
	; Wait for the motor to catch back up - clear the screen !
	ld	b,#0x7F
	ld	a,#0x1F
	out	(c),a
	; Page 7 selected (will be screen)
	ld	hl,#0xC000
	ld	de,#0xC001
	ld	bc,#6911
	xor	a
	ld	(hl),a
	ldir

;
;	Ok now we can try and load stuff
;	9 sectors per track and we need to start from 0/2

	ld	de,#0x02
	ld	ix,#counter
;
;	Load the blocks
;
	ld	hl,#0x4000
	call	load_16k	; 4000-7FFF
	ld	hl,#0x8000
	ld	(ix),#30
	call	load_blocks	; 8000-BBFF
	ld	a,#0x19
	call	load_bank_a	; C000-FFFF CODE1 page 1
	ld	a,#0x1E
	call	load_bank_a	; C000-FFFF CODE2 page 6
	ld	a,#0x1F
	call	load_bank_a	; C000-FFFF CODE3 page 7
	ld	a,#0x18
	call	load_bank_a	; C000-FFFF CODE4 page 0
	ld	bc,#0x7FFD
	ld	a,#0x19
	out	(c),a		; CODE1 mapped
	jp	0xC000		; and run

load_bank_a:
	ld	hl,#0xC000
	ld	bc,#0x7FFD
	out	(c),a
load_16k:
	ld	(ix),#32
load_blocks:
	jr	dosector
;
;	Main track loading loop
;
load_track:
	inc	d		; Next track
	ld	e,#1		; Sectors start at 1
	ld	a,d		; Cycle border colour
	and	#0x03
	add	#4
	out	(0xFE),a
	push	de		; Seek to the needed track
	call	seek_track
	pop	de
dosector:			; Loop through track D loading sector E
	push	de
	call	load_sector	; Load 512 bytes into HL and adjust HL
	pop	de
	dec	(ix)		; counter of sectors
	ret	z
	inc	e		; Next sector
	ld	a,#10		; New track ?
	cp	e
	jr	nz, dosector	; Nope
	jr	load_track	; Move on and seek a track
;
;	Helpers - seek to track D
;
seek_track:
	ld	a,#0x0f
	call	fd765_tx
	xor	a
	call	fd765_tx	;	disk 0 side 0
	ld	a,d
	call	fd765_tx
wait_done:
	ld	a,#0x08
	call	fd765_tx
	call	read_status
	bit	5, a
	jr	z, wait_done
	ld	a, #30		;	30ms
;
;	Head settle
;
;	This assumes uncontended timing
;
wait_ms:
	ld	b,#0xDC
wait_ms_loop2:
	dec	b
	jr	nz, wait_ms_loop2
	dec	a
	jr	nz, wait_ms
	ret

;
;	Load sector E of track D into HL and adjust HL
;
;	Look at just doing track loads in one command! FIXME
;
load_sector:
	ld	a,#0x46
	call	fd765_tx	; read MFM
	xor	a		; Drive 0 head 0
	call	fd765_tx
	ld	a, d		; Track D
	call	fd765_tx
	xor	a		; Head 0
	call	fd765_tx
	ld	a,e		; Sector E
	call	fd765_tx
	ld	a,#2		; 512 bytes
	call	fd765_tx
	ld	a,e		; Last sector (inclusive)
	call	fd765_tx
	ld	a,#27
	call	fd765_tx
	xor	a
	call	fd765_tx

	ld	c,#0xfd		; low bits of I/O
	ld	d,#0x20		; mask
	jp	read_begin
read_loop:
	ld	b,#0x3f
	ini
read_begin:
	ld	b,#0x2f
read_wait:
	in	a,(c)
	jp	p,read_wait
	and	d
	jp	nz,read_loop
read_status:
	ld	c,#0xfd
	push	hl
	ld	hl,#status
read_status_loop:
	ld	b,#0x2f
	in	a,(c)
	rla
	jr	nc, read_status_loop
	rla
	jr	nc, done_status
	ld	b,#0x3f
	ini
	ex	(sp),hl
	ex	(sp),hl
	ex	(sp),hl
	ex	(sp),hl
	jr	read_status_loop
done_status:
	ld	a, (status)
	pop	hl
	ret
status: .ds	8

fd765_tx:
	ex	af, af'
	ld	bc,#0x2ffd	; floppy register (16bit access)
fd765_tx_loop:
	in	a, (c)
	add	a
	jr	nc, fd765_tx_loop
	add	a
	ret	c
	ex	af, af'
	ld	b,#0x3f
	out	(c), a
	ex	(sp),ix
	ex	(sp),ix
	ret

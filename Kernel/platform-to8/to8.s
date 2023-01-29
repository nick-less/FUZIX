	;
	; TO8/TO8D/T09+ systems
	;

	.module to8

	; exported
	.globl map_kernel
	.globl map_video
	.globl map_process
	.globl map_process_always
	.globl map_save
	.globl map_restore
	.globl map_for_swap
        .globl init_early
        .globl init_hardware
        .globl _program_vectors
	.globl _need_resched

	.globl _ramsize
	.globl _procmem

	; imported
	.globl unix_syscall_entry
	.globl null_handler

	.globl video_init

	; exported debugging tools
	.globl _plt_monitor
	.globl _plt_reboot
	.globl outchar
	.globl ___hard_di
	.globl ___hard_ei
	.globl ___hard_irqrestore

	include "kernel.def"
	include "../kernel09.def"


	.area .discard
;
;	Get some video up early for debug
;
init_early:
	lda	#$03
	sta	$E7E5			; A000-DFFF is now the new video space
	clra
	clrb
	ldx	#$A000
wipe80:
	std	,x++
	cmpx	#$E000
	bne	wipe80
	lda	#$04
	sta	$E7E5			; put the kernel back
	lda	#$E0
	sta	$E7DD			; video bank 3, black border
	lda	#$2A			; 80 column video
	sta	$E7DC
	; Do something about the colour
	lda	#1
	ldx	#0
	ldy	#$00F0
	jsr	$EC00
	rts

init_hardware:
	ldd	#512			; for now - need to size properly
	std	_ramsize
	ldd	#512-40			; Kernel has 2,4 and half of 0
	std	_procmem		; will be 2,3,4 eventually
	ldd	<$CF			; system font pointer
	subd	#0x00F8			; back 256 as starts at 32 and back
	std	_fontbase		; 8 because it is upside down
	jsr	video_init		; see the video code
	ldd	#unix_syscall_entry	; Hook SWI
	std	<$2F
	rts

        .area .common

_plt_reboot:
	; TODO
_plt_monitor:
	orcc #0x10
	bra _plt_monitor
;
;	Be nice to monitor
;
___hard_irqrestore:		; B holds the data
	tfr b,a
	anda #0x10
	bne hard_di_2
	; fall through
___hard_ei:
	lda $6019
	ora #$20
;	sta $6019
	andcc #0xef
	rts
___hard_di:
	tfr cc,b		; return the old irq state
hard_di_2
	lda $6019
	anda #$DF
;	sta $6019
	orcc #0x10
	rts

;
; COMMON MEMORY PROCEDURES FOLLOW
;

	.area .common

_program_vectors:
	; TODO set 2064 to JMP to our irq in setup code
	rts

map_kernel:
	pshs	d
map_kernel_1:
	; This is overkill somewhat but we do neeed to set the video bank
	; for irq cases interrupting video writes, ditto 0000-3FFF ?
	ldd	#0x0462
	std	kmap
	std	$E7E5		;	set the A000-DFFF and 0000-3FFF bank
	puls	d,pc

map_video:
	pshs	a
	lda	#0x63
	sta	kmap+1
	sta	$E7E6		;	Video in the low 16K bank
	puls	a,pc

	.area .commondata
kmap:
	.byte	0		; 	A000-DFFF
	.byte	0		;	0000-3FFF
savemap:
	.byte	0
	.byte	0

	.area .common
map_process_always
	pshs	a
	;	Set the upper page. The low 16K is kernel, the other chunk
	;	is fixed for now until we tackle video.
	lda	U_DATA__U_PAGE+1
	sta	kmap
	sta	$E7E5		;	Set A000-DFFF and video bank. Don't
				;	touch the 8K bank 0 map
	; TODO - map video on this switch
	puls	a,pc

map_save:
	pshs	d
	ldd	kmap
	std	savemap
	bra	map_kernel_1

map_restore:
	pshs	d
	ldd	savemap
	std	kmap
	std	$E7E5
	puls	d,pc

map_for_swap:
	; TODO
	rts


	.area .common
outchar:
	rts

	.area .common

_need_resched:
	.db 0

;
;	Interrupt glue
;
	.area	.common

;
;	6021 is the vector for irqpt 6027 for timept - seems it takes one or
;	the other according to what is going on. May need to dump and trace
;	monitor to see how to clean up the stack our way
;
irqhandler:
	; for a full frame pshs cc,a,b,dp,x,y,u,pc then fix up 10,s 0,s
	; but firstly try Bill  Astle's trick
	ldx	#interrupt_handler
	tfr	cc,a
	anda	#$7F
	pshs	a,x
	jmp	$E830	; will run and then end up in interrupt_handler

;
;	Keyboard glue
;
	.area .text2

	.globl _mon_keyboard

_mon_keyboard:
	jmp	$E806

	.area .common
;
;	Floppy glue
;
	.globl _fdbios_flop

_fdbios_flop:
	tst	_fd_map
	beq	via_kernel
	; Loading into a current user pages
	jsr	map_process_always
via_kernel:
	jsr	$E82A
	ldb	#0
	bcc	flop_good
	ldb	<$4E
flop_good
	; ensure map is correct
	tfr	d,x
	jmp	map_kernel

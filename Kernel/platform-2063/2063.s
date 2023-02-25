;
;	2063 specific hardware implementation/ Mostly
;	memory banking
;

        .module z80_2063

        ; exported symbols
        .globl init_hardware
	.globl _program_vectors
	.globl map_kernel
	.globl map_process
	.globl map_process_always
	.globl map_process_a
	.globl map_kernel_di
	.globl map_kernel_restore
	.globl map_process_di
	.globl map_process_always_di
	.globl map_save_kernel
	.globl map_restore
	.globl map_for_swap
	.globl map_buffers
	.globl plt_interrupt_all
	.globl _plt_reboot
	.globl _plt_monitor
	.globl _bufpool
	.globl _int_disabled
	.globl _gpio
	.globl _sd_busy
	.globl _sd_count

        ; imported symbols
        .globl _ramsize
        .globl _procmem
        .globl outhl
        .globl outnewline
	.globl interrupt_handler
	.globl unix_syscall_entry
	.globl nmi_handler
	.globl _udata
	.globl sio_install
	.globl _vectors

        .include "kernel.def"
        .include "../kernel-z80.def"

;=========================================================================
; Buffers
;=========================================================================
        .area	_BUFFERS
	.globl	kernel_endmark

_bufpool:
        .ds	(BUFSIZE * 4) ; adjust NBUFS in config.h in line with this
;
;	So we can check for overflow
;
kernel_endmark:

;=========================================================================
; Initialization code
;=========================================================================
        .area _DISCARD
init_hardware:
	ld	hl,#512
	ld	(_ramsize), hl
	ld	hl,#512-64
	ld	(_procmem), hl
	call	program_kvectors
	call	sio_install

        ; ---------------------------------------------------------------------
	; Initialize CTC
	;
	; We must initialize all channels of the CTC. The documentation
	; states that the initial CTC state is undefined and we don't want
	; random interrupt surprises
	;
	; ---------------------------------------------------------------------

	;
	; Defense in depth - shut everything up first
	;

	ld	a,#0x43
	out	(CTC_CH0),a	; set CH0 mode
	out	(CTC_CH1),a	; set CH1 mode
	out	(CTC_CH2),a	; set CH2 mode
	out	(CTC_CH3),a	; set CH3 mode

	xor a
	out	(CTC_CH0),a	; set the CTC vector to 0

	; CTC 1 and 2 drive the SIO are are on a fixed 1.8Mhz clock. The
	; timer though is off the CPU clock so we just have to hope
	; everyone stays with 10MHz
	;
	; This is btw a lousy choice because with rhe /256 divider we get
	; 39062 clocks and 19531 is prime...
	;

	ld	a,#0xB5
	out	(CTC_CH3),a
	ld	a,#217			; 180Hz ish (we'll drift slghly... )
	; TOOD add a subtle fudge factor to the clock to fix the drift
	out	(CTC_CH3),a

	ld	hl,#interrupt_hook
	ld	(_vectors + 6),hl

	ld	hl,#_vectors
	ld	a,h
	ld	i,a
	im	2			; set Z80 CPU interrupt mode 1 for now
	ret

        .area	_COMMONMEM

; No way to page the ROM back in
_plt_monitor:
_plt_reboot:
	di
	halt

_int_disabled:
	.db	1
pagereg:
	.db	0
pagesave:
	.db	0

_gpio:
	.db	0x05			; CS high clock low MOSI high

plt_interrupt_all:
	ret

; install interrupt vectors
_program_vectors:
	di
	pop	de			; temporarily store return address
	pop	hl			; function argument -- base page number
	push	hl			; put stack back as it was
	push	de

	call	map_process

	; write zeroes across all vectors
	ld	hl,#0
	ld	de,#1
	ld	bc,#0x007f		; program first 0x80 bytes only
	ld	(hl),#0x00
	ldir

program_kvectors:
	ld	a,#0xC3			; JP instruction
	ld	(0x0000),a		; Must be present for NULL checker

	; now install the interrupt vector at 0x0038 (shouldn't be use)
	ld	(0x0038),a
	ld	hl,#interrupt_hook
	ld	(0x0039),hl

	ld	(0x0030),a		; rst 30h is old func call
	ld	hl,#unix_syscall_entry
	ld	(0x0031),hl

	ld	(0x0066),a		; Set vector for NMI
	ld	hl,#nmi_handler
	ld	(0x0067),hl

	jr	map_kernel

;
;	Wrap the system interrupt code to deal with the brain-dead
;	SPI/banking conflict
;
interrupt_hook:
	push	af
	ld	a,(_sd_busy)
	or	a
	jr	nz, contended
	pop	af
	jp	interrupt_handler
contended:
	ld	a,(_sd_count)
	inc	a
	ld	(_sd_count),a
	pop	af
	ei
	reti

_sd_busy:
	.db	0		; SD is bitbanging the GPIO
_sd_count:
	.db	0		; Number of timer interrupts lost
				; to SD bitbanging

;=========================================================================
; Memory management
;=========================================================================

map_process:
map_process_di:
	ld	a,h
	or	l			; HL == 0?
	jr	z,map_kernel		; HL == 0 - map the kernel
	ld	a,(hl)
map_for_swap:
map_process_a:
	push	bc
	ld	(pagereg),a
	ld	c,a
	ld	a,(_gpio)
	and	#0x0F
	or	c
	ld	(_gpio),a
	out	(0x10),a
	pop	bc
	ret

map_process_always:
map_process_always_di:
	push	af
	ld	a,(_udata + U_DATA__U_PAGE)
	call	map_process_a
	pop	af
	ret

map_buffers:
map_kernel:
map_kernel_di:
map_kernel_restore:
	push	af
map_kernel_a:
	xor	a
	call	map_process_a
	pop	af
	ret

map_restore:
	push	af
	ld	a,(pagesave)
	call	map_process_a
	pop	af
	ret

map_save_kernel:
	push	af
	ld	a,(pagereg)
	ld	(pagesave),a
	jr	map_kernel_a


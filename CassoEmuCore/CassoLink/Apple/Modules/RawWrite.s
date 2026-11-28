; ---------------------------------------------------------------------------
; RawWrite: a module of the link program. A track written bit for bit from a
; stream the PC builds, at any quarter-track position, so a copy-protected
; disk is written over the link.
;
; Write raw track (CMDWRAW): the drive, the quarter-track position, 0 to 139,
; the count of bytes, 1 to RPAGES * 256, low byte first, and the two
; percentages. The stream is RLEN bytes: RPAGES pages of the bytes' values,
; then RPAGES pages of the zero bits written after each byte, 0 to 7. It is
; requested in pieces of RPIECE pages (nask). Each value is stored in the data register
; 32 cycles plus 4 for each zero bit after the one before, so the drive
; writes its eight bits and then shifts in zeros until the next store: the
; track's bits as the PC split them (RawWriteStream). An odd position is
; written with the arm between the half tracks on either side, both their
; phases on, as the capture module reads one.
;
; The loop is one copy for each page of the stream (rbuild), RCOPY bytes
; each, so that the store and the reads of the page take no extra cycle. Each
; byte's zeros are turned into the place its copy jumps to (rconv): into a
; run of nops, two for each zero bit, so that every path from one store to
; the next is 32 cycles plus 4 for each zero bit. The last byte of a page
; jumps into the run that ends in the next page's copy, and the last byte of
; the stream into the program's end of a write, which writes two self-sync
; bytes before write mode is turned off.
;
; A byte whose count of zeros is RGAP is a stretch the drive doesn't write
; (rgap). Its value, a self-sync byte, is the last the drive writes, and
; after it the stream has three bytes for each part of the stretch: a wait,
; as rounds of 256 and rounds of one, then what follows the wait. 0 to 7 is
; a phase of the arm turned off or on, the low bits of its switch; RGUP or
; RGDOWN counts the arm a half track up or down; RGWRITE is write mode again,
; with a self-sync byte ahead of the stream's next byte. In the stream the
; PC builds, a stretch's bytes are short of the end of their page. So a
; track's arcs are written with the turn between them left as it was, and
; arcs a half track apart in one pass, each at its angle from the one before.
; ---------------------------------------------------------------------------

; CMDWRAW and the other commands are assigned ahead of this source by
; LinkProgram, from CassoLinkCodec::kCommands.
NBUF       = $5500              ; the stream, page-aligned, after the module
TRIES      = 8
RPAGES     = 26                 ; the pages of a raw track's values and of its zeros
RVALS      = NBUF
RZERO      = RVALS + RPAGES * 256
RCODE      = RZERO + RPAGES * 256 ; the raw loop's copies, after the stream
RCOPY      = 64                 ; the bytes for each copy, so none crosses a page
RLEN       = RPAGES * 512
RPIECE     = 4                  ; the pages of a piece of the stream
RPIECES    = RPAGES * 2 / RPIECE
RNOPS      = 14                 ; the nops in each run: two for each of 7 zero bits
RGAP       = 8                  ; the count of zeros of a stretch not written
RGUP       = 8                  ; after a wait: the arm is a half track up
RGDOWN     = 9                  ; or down
RGWRITE    = $FF                ; or the drive writes again

        .org $5000
        .byte 'C', 'M', 4, 2    ; raw write, version 2
        .word PROGVER
        .word modend - $5000
        .word 0                 ; the CRC, which the PC fills in
        .byte 1
        .byte CMDWRAW
        .word wraw

; Zero page the program doesn't use.
npiece   = $50                  ; the piece of the stream requested
nsrc     = $52                  ; and $53: the next byte of NBUF
ncrc     = $54                  ; and $55: the CRC the host sent
ntry     = $56                  ; the attempts left for the stream
nspun    = $57                  ; the drive whose motor ran before the request
wcount   = $63                  ; and $64: a raw track's count of bytes
wpos     = $65                  ; its quarter-track position
wptr     = $66                  ; and $67: the copy rbuild builds, or rconv's page of zeros
wpage    = $68                  ; the page of the stream
wslot    = $69                  ; the low byte of the page's copy
wtmp     = $6A
gptr     = $6C                  ; and $6D: the copy that called rgap
vptr     = $6E                  ; and $6F: that copy's page of values
gy       = $70                  ; the stream's place past the stretch's bytes
ghi      = $71                  ; a wait's rounds of 256
nhalf    = $6B                  ; the half track nseek takes the arm to

; nprep: an operation's fields cleared, and the drive, the track and the
; bar's percentages from the request. The carry is set when the drive isn't
; 1 or 2.
nprep:  lda #0
        sta p_err
        sta p_misses
        sta p_index
        lda p_PKTBUF+4
        sta p_plo
        lda p_PKTBUF+5
        sta p_phi
        lda p_PKTBUF
        sta p_drive
        lda p_PKTBUF+1
        sta p_track
        lda p_drive
        beq npbad
        cmp #3
        rts
npbad:  sec
        rts

; nseek: the operation on the screen, the motor on and up to speed, and the
; arm on the track; nseekh: the same, to half track A.
nseek:  lda p_track
        asl a
nseekh: sta nhalf
        jsr p_showop
        lda p_spinning          ; a read or write just after the motor starts
        sta nspun               ; waits another 750 ms for the disk's speed
        jsr p_motoron
        lda nspun
        cmp p_drive
        beq nsknown
        lda #250
        jsr p_delayms
        lda #250
        jsr p_delayms
        lda #250
        jsr p_delayms
nsknown: ldx p_drive            ; an arm whose place isn't known goes to track 0
        lda p_curhalf-1,x
        cmp #$FF
        bne nstrack
        lda #80
        sta p_curhalf-1,x
        lda #0
        sta p_target
        jsr p_stepto
nstrack: lda nhalf
        sta p_target
        jmp p_stepto

; getnib: a piece of the stream, RPIECE pages of Nibble data, at its place in
; NBUF. C is set on a timeout, a packet that isn't Nibble data or a failed
; CRC. At 115,200 baud a byte comes every 89 cycles, too few for the CRC as
; well, so the bytes are stored as they arrive and checked after.
getnib: jsr p_getenvt
        bcs gnlost
        cmp #CMDCANCEL
        bne gncmd
        jmp p_cancel
gncmd:  cmp #CMDNIBDATA
        beq gnread
gnlost: sec
        rts
gnread: jsr gnstart
gnpage: jsr p_getbtq
        bcs gnlost
        sta (nsrc),y
        iny
        bne gnpage
        inc nsrc+1
        dex
        bne gnpage
        jsr p_getbtq
        bcs gnlost
        sta ncrc
        jsr p_getbtq
        bcs gnlost
        sta ncrc+1
        jsr p_getbtq            ; the payload's EOR; the CRC checks the piece
        bcs gnlost
        lda #0
        sta p_crclo
        sta p_crchi
        jsr gnstart
gnspage: lda (nsrc),y
        jsr p_crcupd
        iny
        bne gnspage
        inc nsrc+1
        dex
        bne gnspage
        lda p_crclo
        cmp ncrc
        bne gnlost
        lda p_crchi
        cmp ncrc+1
        bne gnlost
        clc
        rts

; gnstart: nsrc at piece npiece of NBUF, Y 0, and X the pages of a piece.
gnstart: lda #0
        sta nsrc
        lda npiece
        asl a
        asl a
        clc
        adc #>NBUF
        sta nsrc+1
        ldy #0
        ldx #RPIECE
        rts

; nask: the stream requested a piece at a time, so that a byte lost on the
; line costs one piece and not the stream. Each piece is requested with an
; Acknowledge whose second byte is the piece, and again with a NAK when it
; arrives damaged, TRIES times at most. C is set when a piece never arrives.
nask:   lda #0
        sta npiece
napiece: lda #TRIES
        sta ntry
        lda #p_ACK
naloop: ldx npiece
        ldy #0
        jsr p_sendack
        jsr getnib
        bcc nagot
        jsr p_drain
        dec ntry
        beq nafail
        lda #p_NAK
        jmp naloop
nagot:  inc npiece
        lda npiece
        cmp #RPIECES
        bne napiece
        clc
        rts
nafail: sec
        rts

; wraw: Write raw track.
wraw:   lda #p_OPWRITE
        sta p_op
        jsr nprep
        bcs wrbad
        lda p_PKTBUF+1
        sta wpos
        cmp #140
        bcs wrbad
        lsr a
        lsr a
        sta p_track
        lda p_PKTBUF+2          ; a count of 1 to RPAGES * 256
        sec
        sbc #1
        sta wcount
        lda p_PKTBUF+3
        sbc #0
        sta wcount+1            ; the last byte's place
        cmp #RPAGES
        bcc wrshow
wrbad:  lda #p_ERRDRIVE
        sta p_err
        jmp p_opdone
wrshow: lda wpos
        lsr a
        jsr nseekh
        jsr p_protected
        bcs wrdone
        jsr nask
        bcc wrgot
        lda #p_ERRLINK
        sta p_err
        jmp p_opdone
wrgot:  jsr rbuild
        jsr rconv
        lda wpos
        and #1
        beq wrplay
        jsr rquarter
wrplay: php                     ; no interrupt while the track is written
        sei
        jsr rplay
        plp
        lda wpos
        and #1
        beq wrok
        jsr rqoff
wrok:   lda #1                  ; the one stream of one, for the bar's end
        sta p_count
        sta p_index
        jsr p_showop            ; the bar at the track's end
wrdone: jmp p_opdone

; rplay: write mode with a self-sync byte loaded, as the program's player
; starts, and the first copy 40 cycles after that store.
rplay:  ldx p_diskx
        lda p_Q6H,x
        lda p_Q7L,x
        lda #$FF
        sta p_Q7H,x
        ora p_Q6L,x             ; 4
        ldy #0                  ; 2
        jsr p_dly12             ; 12
        nop                     ; 10
        nop
        nop
        nop
        nop
        jmp RCODE               ; 3, and 9 in the copy to its store: 40

; rbuild: a copy of wlmpl for each page at RCODE, RCOPY bytes apart, with
; the page's values and zeros, the address of the jump in it, and the next copy.
rbuild: lda #<RCODE
        sta wptr
        lda #>RCODE
        sta wptr+1
        lda #0
        sta wpage
rbcopy: ldy #wlend - wlmpl - 1
rbbyte: lda wlmpl,y
        sta (wptr),y
        dey
        bpl rbbyte
        ldy #wlvals + 2 - wlmpl
        lda wpage
        clc
        adc #>RVALS
        sta (wptr),y
        ldy #wlzero + 2 - wlmpl
        lda wpage
        clc
        adc #>RZERO
        sta (wptr),y
        ldy #wlset + 1 - wlmpl  ; the store into the copy's jump
        lda wptr
        clc
        adc #wljump + 1 - wlmpl
        sta (wptr),y
        ldy #wlrun + 1 - wlmpl  ; the run's jump to the branch after it
        lda wptr
        clc
        adc #wlnext - wlmpl
        sta (wptr),y
        ldy #wlwjmp + 1 - wlmpl ; the last byte's run, to the next copy
        lda wptr
        clc
        adc #RCOPY
        sta (wptr),y
        lda wptr+1
        adc #0
        iny
        sta (wptr),y
        lda wptr+1              ; the copy's page, in each jump within it
        ldy #wlset + 2 - wlmpl
        sta (wptr),y
        ldy #wljump + 2 - wlmpl
        sta (wptr),y
        ldy #wlrun + 2 - wlmpl
        sta (wptr),y
        lda wptr
        clc
        adc #RCOPY
        sta wptr
        bcc rbnext
        inc wptr+1
rbnext: inc wpage
        lda wpage
        cmp #RPAGES
        bne rbcopy
        rts

; rconv: each byte's zeros, 0 to 7, turned into the low byte of the place
; its copy jumps to: the run after wljump, or for the last byte of a page
; the run after wlexit, entered 2 nops later for each zero bit fewer than 7;
; for RGAP, the copy's call of rgap; and for the stream's last byte, wlexit.
rconv:  lda #0
        sta wptr
        sta wpage
        lda #>RZERO
        sta wptr+1
rcpage: lda wpage
        jsr rslotlo
        ldy #0
rcbyte: lda (wptr),y
        cmp #RGAP
        bcc rczero
        lda #wlgap - wlmpl
        bne rcadd
rczero: asl a
        sta wtmp
        cpy #$FF
        beq rcwrap
        lda #wlslide + RNOPS - wlmpl
        bne rcset
rcwrap: lda #wlwrap + RNOPS - wlmpl
rcset:  sec
        sbc wtmp
rcadd:  clc
        adc wslot
        sta (wptr),y
        iny
        bne rcbyte
        inc wptr+1
        inc wpage
        lda wpage
        cmp #RPAGES
        bne rcpage
        lda wcount+1            ; the last byte, to wlexit
        jsr rslotlo
        lda wcount+1
        clc
        adc #>RZERO
        sta wptr+1
        ldy wcount
        lda wslot
        clc
        adc #wlexit - wlmpl
        sta (wptr),y
        rts

; rslotlo: the low byte of page A's copy, in wslot.
rslotlo: asl a
        asl a
        asl a
        asl a
        asl a
        asl a
        clc
        adc #<RCODE
        sta wslot
        rts

; rquarter: the half track's phase and the next one's on, and a pause while
; the arm settles between them; rqoff: both off.
rquarter: jsr rqfirst
        lda p_PHASEON,x
        jsr rqsecond
        lda p_PHASEON,x
        lda #25
        jmp p_delayms
rqoff:  jsr rqfirst
        lda p_PHASEOFF,x
        jsr rqsecond
        lda p_PHASEOFF,x
        rts
rqfirst: ldx p_drive
        lda p_curhalf-1,x
        jmp rqphase
rqsecond: ldx p_drive
        lda p_curhalf-1,x
        clc
        adc #1
rqphase: and #3
        asl a
        ora p_diskx
        tax
        rts

; rgap: a stretch not written. The copy's call has its address on the
; stack, and the copy has the page of values it reads. Read mode is 100
; cycles after the store of the stretch's byte, so that byte and the one
; before it are out of the //c's IWM. Each wait is 5 cycles for each round
; of one, 256 for a count of 0, and 1284 for each round of 256, of which
; there is one at least, less 2.
rgap:   tsx                     ; 23 cycles from the store to here
        lda $0102,x
        sta gptr+1
        lda $0101,x
        and #$100 - RCOPY       ; the copy's first byte
        sta gptr
        sty gy
        ldy #wlvals + 2 - wlmpl
        lda (gptr),y
        sta vptr+1
        lda #0
        sta vptr
        ldy gy
        ldx p_diskx
        jsr p_dly12
        jsr p_dly12
        nop
        nop
        lda p_Q7L,x             ; read mode
        lda p_Q6L,x
rgseg:  lda (vptr),y            ; the rounds of 256
        sta ghi
        iny
        lda (vptr),y            ; and of one
        iny
        sty gy
        tay
        ldx ghi
rgone:  dey
        bne rgone
rglong: dey
        bne rglong
        dex
        bne rglong
        ldy gy
        lda (vptr),y            ; what follows the wait
        iny
        cmp #RGUP
        bcs rgmoved
        ora p_diskx
        tax
        lda p_PHASEOFF,x        ; a phase off, or on
        jmp rgseg
rgmoved: bmi rgwrite
        ldx p_drive
        lsr a
        bcs rgdown
        inc p_curhalf-1,x
        bcc rgseg
rgdown: dec p_curhalf-1,x
        bcs rgseg
rgwrite: sty gy
        ldx p_diskx
        lda p_Q6H,x
        lda p_Q7L,x
        lda #$FF
        sta p_Q7H,x             ; write mode, a self-sync byte loaded
        ora p_Q6L,x             ; 4
        jsr p_dly12             ; 12
        bit $00                 ; 3
        ldy gy                  ; 3, and never 0, so the copy's branch is taken
        rts                     ; 6, 3 in the copy and 9 to its store: 40

; rdone: the end of a raw write, 20 cycles after the last store, to the
; program's end of a write 31 after it, as its player has it.
rdone:  nop                     ; 8
        nop
        nop
        nop
        jmp p_pdone             ; 3

; wlmpl: the loop for one page, copied by rbuild. Cycles from the store to
; the jump into a run are 17; each run is 2 for each nop, then 6 more, and
; the copy's first instructions 9 to the store: 32 plus 4 for each zero bit.
; Every branch and jump stays within the copy's page.
wlmpl:
wlvals: lda RVALS,y             ; 4   (the page is set by rbuild)
        sta p_Q6H,x             ; 5: the store
        ora p_Q6L,x             ; 4   at once, or the latch loads twice
wlzero: lda RZERO,y             ; 4   the place to jump to
wlset:  sta wljump+1            ; 4
        iny                     ; 2
wljump: jmp wlslide             ; 3
wlslide: nop                    ; RNOPS of them
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
wlrun:  jmp wlnext              ; 3
wlgap:  jsr rgap                ; a stretch not written, then the next byte
wlnext: bne wlmpl               ; 3
wlexit: jmp rdone               ; 3
wlwrap: nop                     ; RNOPS of them, for the page's last byte
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        bit $00                 ; 3
wlwjmp: jmp wlmpl               ; 3, to the next copy
wlend:

modend:

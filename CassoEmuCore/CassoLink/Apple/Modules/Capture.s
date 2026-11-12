; ---------------------------------------------------------------------------
; Capture: a module of the link program. Raw capture of one quarter-track
; position of a 5.25" disk, for a WOZ of a copy-protected disk.
;
; Request (CMDCAPTURE): the percentages at the request's start and end, the
; drive, the quarter-track position, 0 to 139, and the delay of the second
; read of each byte: 0 for 8 cycles, 1 for 12. The arm goes to the
; position: to the half track with the program's stepto, and for an odd
; position, to the quarter between it and the next half track, with both
; their phases on while the read runs.
;
; The read fills CAPBUF to CAPEND with the bytes as the data latch has them,
; with no gap between bytes: a little over two revolutions of a 16-sector
; track. By Sather's Table 9.5, a finished byte is about 8 cycles in the latch,
; and 4 more for each zero bit after it, so the loop reads the latch again
; 8 or 12 cycles after it finds a byte; when the latch has the same byte
; then, the byte is followed by zero bits, as a sync byte is, and it is
; stored with its top bit clear. A disk byte always has its top bit set, so
; the bit is free. The read that finds a byte is up to 7 cycles after the
; byte is finished, so one read at 8 marks a byte followed by one zero bit
; about four times in seven, and one at 12 marks one followed by two zero
; bits as often: the PC counts the zero bits from the marks of several reads.
;
; The loop is one copy per page, made by capbuild, so that no page crossing
; adds cycles to a byte.
;
; Reply: each page as Capture data (CMDCAPDATA): the page's index, its 256
; bytes and their CRC-16/XMODEM, low byte first; then a Status.
; ---------------------------------------------------------------------------

CMDCAPTURE = $EA
CMDCAPDATA = $EB
CAPBUF     = $5800
CAPEND     = $9000
CAPPAGES   = (CAPEND - CAPBUF) / 256
CAPCODE    = $5200              ; the loop, a copy for each page

; The page index and the 256 bytes and the CRC: the payload's length.
CAPLEN     = 1 + 256 + 2

        .org $5000
        .byte 'C', 'M', 1, 2    ; capture, version 2
        .word PROGVER
        .word modend - $5000
        .word 0                 ; the CRC, which the PC fills in
        .byte 1
        .byte CMDCAPTURE
        .word capture

; Zero page the program doesn't use.
cpos     = $40                  ; the position
cpage    = $41                  ; capsend's page
ctempl   = $42                  ; and $43: capbuild's copy of the loop
clen     = $44                  ; its length
ccmp     = $45                  ; the offset of its second read's address
csta     = $46                  ; the offset of its store's page

capture: lda #p_OPREAD
        sta p_op
        lda p_PKTBUF
        sta p_plo
        lda p_PKTBUF+1
        sta p_phi
        lda p_PKTBUF+2
        sta p_drive
        lda p_PKTBUF+4          ; the second read's delay
        jsr cpdelay
        lda p_PKTBUF+3
        sta cpos
        lsr a
        lsr a
        sta p_track
        lda #0
        sta p_err
        sta p_misses
        sta p_index
        lda p_drive             ; drive 1 or 2
        beq cpbaddrive
        cmp #3
        bcc cpdrive
cpbaddrive: lda #p_ERRDRIVE
        sta p_err
        jmp p_opdone
cpdrive: lda cpos
        cmp #140
        bcs cpbaddrive
        jsr p_showop
        jsr capbuild
        jsr p_motoron
        lda cpos
        lsr a
        sta p_target
        jsr p_stepto
        lda cpos
        and #1
        beq cpread
        jsr cpquarter
cpread: ldx p_diskx
        lda p_Q6L,x
        lda p_Q7L,x
        ldy #0
        jsr CAPCODE
        lda cpos                ; the quarter's two phases off
        and #1
        beq cpsend
        jsr cpoff
cpsend: jsr capsend
        jmp p_opdone

; cpquarter: the half track's phase and the next one's on, and a pause
; while the arm settles between them.
cpquarter: ldx p_drive
        lda p_curhalf-1,x
        and #3
        asl a
        ora p_diskx
        tax
        lda p_PHASEON,x
        ldx p_drive
        lda p_curhalf-1,x
        clc
        adc #1
        and #3
        asl a
        ora p_diskx
        tax
        lda p_PHASEON,x
        lda #25
        jmp p_delayms

; cpoff: both of the quarter's phases off. The arm is left between the half
; tracks; the program's next step is from the half track before it.
cpoff:  ldx p_drive
        lda p_curhalf-1,x
        and #3
        asl a
        ora p_diskx
        tax
        lda p_PHASEOFF,x
        ldx p_drive
        lda p_curhalf-1,x
        clc
        adc #1
        and #3
        asl a
        ora p_diskx
        tax
        lda p_PHASEOFF,x
        rts

; cpdelay: the copy of the loop for the delay in A, 0 for 8 cycles and 1 for
; 12, into ctempl, clen, ccmp and csta.
cpdelay: cmp #1
        beq cpd12
        lda #<copy8
        sta ctempl
        lda #>copy8
        sta ctempl+1
        lda #copy8end-copy8
        sta clen
        lda #cmp8+1-copy8
        sta ccmp
        lda #sta8+2-copy8
        sta csta
        rts
cpd12:  lda #<copy12
        sta ctempl
        lda #>copy12
        sta ctempl+1
        lda #copy12end-copy12
        sta clen
        lda #cmp12+1-copy12
        sta ccmp
        lda #sta12+2-copy12
        sta csta
        rts

; capbuild: the loop's copy for each page at CAPCODE, each storing to its
; page, then an RTS. The latch's address is the slot's.
capbuild: lda #<CAPCODE
        sta p_ptr
        lda #>CAPCODE
        sta p_ptr+1
        lda #>CAPBUF
        sta cpage
cbpage: ldy #0
cbbyte: lda (ctempl),y
        sta (p_ptr),y
        iny
        cpy clen
        bne cbbyte
        ldy #1                  ; the latch's address in the slot, twice
        lda p_diskx
        clc
        adc #<p_Q6L
        sta (p_ptr),y
        ldy ccmp
        sta (p_ptr),y
        ldy csta                ; the page this copy stores to
        lda cpage
        sta (p_ptr),y
        lda p_ptr
        clc
        adc clen
        sta p_ptr
        bcc cbnext
        inc p_ptr+1
cbnext: inc cpage
        lda cpage
        cmp #>CAPEND
        bne cbpage
        ldy #0
        lda #$60                ; RTS
        sta (p_ptr),y
        rts

; The loop, with the second read 8 cycles after the first: the BPL not
; taken, a NOP and the CMP's four.
copy8:
cpoll8: lda $C000               ; patched: the slot's latch
        bpl cpoll8
        nop
cmp8:   cmp $C000               ; patched: the slot's latch
        bne cplain8
        and #$7F                ; zero bits followed it
cplain8:
sta8:   sta CAPBUF,y            ; patched: the copy's page
        iny
        bne cpoll8
copy8end:

; And 12 cycles after it, with three NOPs. From the read that finds a byte
; to the next poll is 26 cycles, under the 32 of the next byte.
copy12:
cpoll12: lda $C000              ; patched: the slot's latch
        bpl cpoll12
        nop
        nop
        nop
cmp12:  cmp $C000               ; patched: the slot's latch
        bne cplain12
        and #$7F                ; zero bits followed it
cplain12:
sta12:  sta CAPBUF,y            ; patched: the copy's page
        iny
        bne cpoll12
copy12end:

; capsend: each page as Capture data.
capsend: lda #0
        sta cpage
        lda #<CAPBUF
        sta p_ptr
        lda #>CAPBUF
        sta p_ptr+1
csloop: lda #$C1
        jsr p_putb
        lda #<CAPLEN
        jsr p_putb
        lda #>CAPLEN
        jsr p_putb
        lda #CMDCAPDATA
        jsr p_putb
        lda #$C1 ^ <CAPLEN ^ >CAPLEN ^ CMDCAPDATA
        jsr p_putb
        lda #0
        sta p_peor
        sta p_crclo
        sta p_crchi
        lda cpage
        jsr p_putpe
        ldy #0
csbyte: lda (p_ptr),y
        jsr p_crcupd
        lda (p_ptr),y
        jsr p_putpe
        iny
        bne csbyte
        lda p_crclo
        jsr p_putpe
        lda p_crchi
        jsr p_putpe
        lda p_peor
        jsr p_putb
        inc p_ptr+1
        inc cpage
        lda cpage
        cmp #CAPPAGES
        bne csloop
        rts

modend:

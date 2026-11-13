; ---------------------------------------------------------------------------
; Capture: a module of the link program. Raw capture of one quarter-track
; position of a 5.25" disk, for a WOZ of a copy-protected disk.
;
; Request (CMDCAPTURE): the percentages at the request's start and end, the
; drive, the quarter-track position, 0 to 139, and the delay of the second
; read of each byte: 0 for 8 cycles, 1 for 12, with $80 added to take the arm
; to track 0 first (cpknown); then the position to wait on,
; $FF for none, the marker's length, up to CMARKMAX, and its bytes. The arm
; goes to the position: to the half track with the program's stepto, and for
; an odd position, to the quarter between it and the next half track, with
; both their phases on while the read runs.
;
; With a position to wait on, the arm goes there first and the latch is read
; (after another 750 ms when the motor has just started, as a disk coming up
; to speed would make the wait's timing wrong)
; until the marker passes (cpfind), and the arm then steps to the position and
; the read starts. Every instruction from the marker's last byte to the read's
; first is the same each time, so the read starts a fixed number of cycles
; after the marker passes, and the PC can place the track's data relative to
; the track the marker is on, as a nibble copier's synchronized tracks are
; placed. Interrupts are off from before the wait to the end of the read.
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
; Reply: the bytes encoded (encode), in Capture data packets (CMDCAPDATA) of
; a sequence number, 256 bytes of the encoding and their CRC-16/XMODEM, low
; byte first; then a Status, whose count of sectors done is 1 when the disk
; is write-protected. The last packet is padded with zeros.
;
; The encoding begins with the revolution's length in bytes, low byte first,
; 0 when findrev doesn't find one. Then each byte other than 0 is itself, and
; 0 begins a token, until every byte is given:
;
;   0 0      a 0
;   0 n      n of 1 to 127: n + 2 bytes the same as those a revolution earlier
;   0 $80+n  n of 0 to 127: the next byte, repeated n + 3 times
;
; A byte is the same only with its mark the same, so the PC has every byte
; as the read stored it.
; ---------------------------------------------------------------------------

CMDCAPTURE = $EA
CMDCAPDATA = $EB
CAPBUF     = $5900
CAPEND     = $9000
CAPPAGES   = (CAPEND - CAPBUF) / 256
CAPCODE    = $5470              ; the loop, a copy for each page, up to CAPBUF

; The sequence number and the 256 bytes and the CRC: the payload's length.
CAPLEN     = 1 + 256 + 2

; The range of revolution lengths for findrev, and its second sample's offset.
LMIN       = 4800
LMAX       = 7000
SAMPLE     = 1024
SAMPLEN    = 32

RUNMAX     = 129                ; the most bytes in a repeat or a copy
CMARKMAX   = 16                 ; the longest marker
CFMAX      = 20000              ; the bytes cpfind reads before giving up, about 3 revolutions
CAPPAGE1   = CAPBUF / 256 + 1   ; the page after CAPBUF's first

        .org $5000
        .byte 'C', 'M', 1, 9    ; capture, version 9
        .word PROGVER
        .word modend - $5000
        .word 0                 ; the CRC, which the PC fills in
        .byte 1
        .byte CMDCAPTURE
        .word capture

; Zero page the program doesn't use.
cpos     = $40                  ; the position
cpage    = $41                  ; capbuild's page
ctempl   = $42                  ; and $43: capbuild's copy of the loop
clen     = $44                  ; its length
ccmp     = $45                  ; the offset of its second read's address
csta     = $46                  ; the offset of its store's page
esrc     = $47                  ; and $48: the next byte to encode
epred    = $49                  ; and $4A: the byte a revolution before it
elen     = $4B                  ; and $4C: the revolution's length, or 0
ecount   = $51                  ; the bytes left in the open packet
eopen    = $52                  ; nonzero while a packet is open
eseq     = $53                  ; the next packet's sequence number
ecap     = $54                  ; the most bytes a token can have here
en       = $55                  ; a token's count
etp      = $56                  ; and $57: a token's first byte
etq      = $58                  ; and $59: the byte a revolution before it
eyy      = $5A                  ; esrc's offset of the byte being encoded
ebyte    = $5B                  ; that byte
evalid   = $5C                  ; $80 once copies are allowed
etmp     = $5D
evpage   = $5E                  ; the page copies are allowed from
eend     = $5F                  ; nonzero once every byte is given
csync    = $60                  ; the position to wait on, $FF for none
cmlen    = $61                  ; the marker's length
cfcnt    = $62                  ; and $63: the bytes cpfind may still read
cspun    = $64                  ; the drive whose motor ran before the request
crecal   = $65                  ; $80 when the arm is taken to track 0 first

capture: lda #p_OPCAPT
        sta p_op
        lda p_PKTBUF
        sta p_plo
        lda p_PKTBUF+1
        sta p_phi
        lda p_PKTBUF+2
        sta p_drive
        lda p_PKTBUF+4          ; the second read's delay, and in its top
        and #$80                ; bit the arm taken to track 0 first
        sta crecal
        lda p_PKTBUF+4
        and #$7F
        jsr cpdelay
        lda p_PKTBUF+3
        sta cpos
        and #3
        sta p_sector            ; the bar's quarter
        lda cpos
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
        lda p_PKTBUF+5          ; the position to wait on and the marker
        sta csync
        lda p_PKTBUF+6
        cmp #CMARKMAX+1
        bcc cpmlen
        lda #CMARKMAX
cpmlen: sta cmlen
        ldy #0
cpmark: cpy cmlen
        beq cpshow
        lda p_PKTBUF+7,y
        sta cmark,y
        iny
        bne cpmark
cpshow: jsr p_showop
        jsr capbuild
        lda p_spinning          ; whether the motor runs already
        sta cspun
        jsr p_motoron
        lda csync               ; a wait just after the motor starts is timed
        cmp #$FF                ; while the disk comes up to speed
        beq cpsafe
        lda cspun
        cmp p_drive
        beq cpsafe
        lda #250
        jsr p_delayms
        lda #250
        jsr p_delayms
        lda #250
        jsr p_delayms
cpsafe: jsr cpknown
        php                     ; no interrupt from here to the read's end:
        sei                     ; one costs bytes, and the wait's timing
        lda csync
        cmp #$FF
        beq cpgo
        lsr a
        sta p_target
        jsr p_stepto
        jsr cpfind
        bcc cpgo
        plp
        lda #p_ERRNOTFND
        sta p_err
        jmp p_opdone
cpgo:   lda cpos
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
        plp
        ldx p_diskx
        lda p_Q6H,x             ; the write-protect sense, as the program's
        lda p_Q7L,x             ; protected reads it: 1 in the Status's count
        asl a                   ; of sectors done when the disk is protected
        lda p_Q6L,x
        lda #0
        rol a
        sta p_index
        lda cpos                ; the quarter's two phases off
        and #1
        beq cpsend
        jsr cpoff
cpsend: jsr encode
        jmp p_opdone

; cpfind: the latch read until the cmlen bytes of cmark pass in a row,
; returning with C clear right after the last; with C set after CFMAX bytes
; without them. From the read that finds a byte to the next poll is 26 cycles
; on each path, so the same byte is never read twice and the next is never
; missed.
cpfind: ldx p_diskx
        lda p_Q6L,x
        lda p_Q7L,x
        lda #<CFMAX
        sta cfcnt
        lda #>CFMAX
        sta cfcnt+1
        ldy #0
cfpoll: lda p_Q6L,x
        bpl cfpoll
        cmp cmark,y
        bne cfmiss
        iny
        cpy cmlen
        beq cffound
        bne cfnext
cfmiss: ldy #0                  ; the byte may start the marker again
        cmp cmark
        bne cfnext
        iny
cfnext: dec cfcnt
        bne cfpoll
        dec cfcnt+1
        bne cfpoll
        sec
        rts
cffound: clc
        rts

; cpknown: the arm taken to track 0 when the request has $80 in its delay, as
; a capture's first read has, or when its place is $FF, as after the program
; is loaded. A step from a wrong place in curhalf ends at the stop or on
; another track, and curhalf is then wrong too.
cpknown: ldx p_drive
        lda crecal
        bne ckzero
        lda p_curhalf-1,x
        cmp #$FF
        bne ckdone
ckzero: lda #80
        sta p_curhalf-1,x
        lda #0
        sta p_target
        jmp p_stepto
ckdone: rts

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

; encode: CAPBUF's bytes in the encoding above: at each byte, a copy of
; the revolution before when the next 3 match it, else a repeat when the next
; 3 are the same, else the byte as it is. Copies start only from the page
; after the one with the first byte a revolution in, so the check costs a
; BIT. Each byte of the encoding goes out as it is made, so the work overlaps
; the chip's sending.
encode: lda #0
        sta eopen
        sta eseq
        sta evalid
        sta eend
        jsr findrev
        lda elen
        jsr emitb
        lda elen+1
        jsr emitb
        lda #$FF                ; no copies without a revolution
        sta evpage
        lda elen
        ora elen+1
        beq ensrc
        lda elen+1
        clc
        adc #CAPPAGE1
        sta evpage
ensrc:  lda #<CAPBUF
        sta esrc
        sec
        sbc elen
        sta epred
        lda #>CAPBUF
        sta esrc+1
        sbc elen+1
        sta epred+1
        ldy #0
enloop: bit evalid
        bpl enrun
        lda (esrc),y            ; a copy may start here
        cmp (epred),y
        bne enrun
        sty eyy
        jsr tokcopy
        bcs ennext
        ldy eyy
enrun:  sty eyy                 ; a repeat may start here; at a page's end
        lda (esrc),y            ; the check is skipped
        sta ebyte
        iny
        beq enlit
        cmp (esrc),y
        bne enlit
        iny
        beq enlit
        cmp (esrc),y
        bne enlit
        ldy eyy
        jsr tokrun
        bcs ennext
enlit:  ldy eyy
        lda ebyte               ; the byte as it is, a 0 as 0 0
        bne enone
        jsr emitb
        lda #0
enone:  jsr emitb
        iny
        bne enloop
        jsr newpage
        beq enpad
        jmp enloop
ennext: lda eend
        beq enloop
enpad:  lda eopen               ; zeros to the end of the last packet
        beq endone
        lda #0
        jsr emitb
        jmp enpad
endone: rts

; tokcopy: from esrc+eyy, a copy token when 3 or more bytes match those a
; revolution before, returning with C set and Y past them; else C clear.
tokcopy: jsr setptrs
        ldy #0
tcloop: cpy ecap
        beq tcend
        lda (etp),y
        cmp (etq),y
        bne tcend
        iny
        bne tcloop
tcend:  cpy #3
        bcc tcnone
        sty en
        lda #0
        jsr emitb
        lda en
        sec
        sbc #2
        jsr emitb
        jmp advance
tcnone: clc
        rts

; tokrun: from esrc+eyy, a repeat token when 3 or more bytes are ebyte,
; returning with C set and Y past them; else C clear.
tokrun: jsr setptrs
        ldy #1
trloop: cpy ecap
        beq trend
        lda (etp),y
        cmp ebyte
        bne trend
        iny
        bne trloop
trend:  cpy #3
        bcc trnone
        sty en
        lda #0
        jsr emitb
        lda en
        sec
        sbc #3
        ora #$80
        jsr emitb
        lda ebyte
        jsr emitb
        jmp advance
trnone: clc
        rts

; setptrs: etp at esrc+eyy, etq at epred+eyy, and ecap the most a token can
; have here: RUNMAX, or the bytes left.
setptrs: lda esrc
        clc
        adc eyy
        sta etp
        lda esrc+1
        adc #0
        sta etp+1
        lda epred
        clc
        adc eyy
        sta etq
        lda epred+1
        adc #0
        sta etq+1
        lda #>CAPEND
        sec
        sbc esrc+1
        cmp #2
        bcs spmax
        lda #0                  ; the last page: 256 less eyy
        sec
        sbc eyy
        beq spmax
        cmp #RUNMAX
        bcc spset
spmax:  lda #RUNMAX
spset:  sta ecap
        rts

; advance: Y at eyy plus en, onto the next page when it passes one,
; returning with C set.
advance: lda eyy
        clc
        adc en
        tay
        bcc avdone
        jsr newpage
        bne avdone
        lda #1
        sta eend
avdone: sec
        rts

; newpage: esrc and epred on a page, copies allowed from evpage, and Z set
; at CAPEND.
newpage: inc esrc+1
        inc epred+1
        lda esrc+1
        cmp evpage
        bcc npend
        lda #$80
        sta evalid
npend:  lda esrc+1
        cmp #>CAPEND
        rts

; findrev: in elen, the first length from LMIN to LMAX at which CAPBUF's
; first SAMPLEN bytes repeat, and those from SAMPLE too, with the marks
; ignored; 0 when none does. A wrong length makes the encoding longer, not
; wrong.
findrev: lda #<LMIN
        sta elen
        lda #>LMIN
        sta elen+1
frloop: lda #<CAPBUF
        clc
        adc elen
        sta etp
        lda #>CAPBUF
        adc elen+1
        sta etp+1
        ldy #0
fr1:    lda CAPBUF,y
        eor (etp),y
        and #$7F
        bne frnext
        iny
        cpy #SAMPLEN
        bne fr1
        lda etp+1
        clc
        adc #>SAMPLE
        sta etp+1
        ldy #0
fr2:    lda CAPBUF+SAMPLE,y
        eor (etp),y
        and #$7F
        bne frnext
        iny
        cpy #SAMPLEN
        bne fr2
        rts
frnext: inc elen
        bne fr3
        inc elen+1
fr3:    lda elen
        cmp #<(LMAX+1)
        bne frloop
        lda elen+1
        cmp #>(LMAX+1)
        bne frloop
        lda #0
        sta elen
        sta elen+1
        rts

; emitb: A as the encoding's next byte, in the open packet or a new one,
; with the CRC by the program's tables; a full packet is ended. Y is saved.
emitb:  sta etmp
        lda eopen
        bne ebopen
        jsr pktbegin
ebopen: lda etmp
        eor p_peor
        sta p_peor
        lda etmp
        eor p_crchi
        tax
        lda p_crclo
        eor p_CRCHI,x
        sta p_crchi
        lda p_CRCLO,x
        sta p_crclo
        lda etmp
        jsr p_putb
        dec ecount
        bne ebdone
        jsr pktend
ebdone: rts

; pktbegin: a Capture data packet's header and sequence number.
pktbegin: lda #$C1
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
        sta p_crclo
        sta p_crchi
        sta ecount              ; 256 bytes
        lda eseq
        sta p_peor
        jsr p_putb
        lda #1
        sta eopen
        rts

; pktend: the packet's CRC and check byte.
pktend: lda p_crclo
        jsr p_putpe
        lda p_crchi
        jsr p_putpe
        lda p_peor
        jsr p_putb
        inc eseq
        lda #0
        sta eopen
        rts

cmark:  .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

modend:

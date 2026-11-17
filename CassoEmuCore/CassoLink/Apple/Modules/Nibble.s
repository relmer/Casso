; ---------------------------------------------------------------------------
; Nibble: a module of the link program. A track written from a stream of
; nibbles the host builds, so a disk of any format, 13 sectors among them,
; is written over the link (R49).
;
; Write nibble track (CMDWNIBS): the drive, the track and the stream's
; length, low byte first. An Acknowledge requests the stream, and the host's
; Nibble data packet (CMDNIBDATA) has it and its CRC-16/XMODEM, low byte
; first; a NAK requests it again, 8 times at most, then ERRLINK. In the
; stream, as the program's player takes it, each byte with its top bit set is
; a nibble and a 0 is a self-sync byte. The player writes the stream from the
; arm's place on the track, in one pass, then the module sends Status.
; ---------------------------------------------------------------------------

; CMDWNIBS and CMDNIBDATA are assigned ahead of this source by LinkProgram,
; from CassoLinkCodec::kCommands.
NBUF       = $5400              ; the stream, page-aligned for the player
NEND       = $9000
TRIES      = 8

        .org $5000
        .byte 'C', 'M', 3, 1    ; nibble, version 1
        .word PROGVER
        .word modend - $5000
        .word 0                 ; the CRC, which the PC fills in
        .byte 1
        .byte CMDWNIBS
        .word wtrack

; Zero page the program doesn't use.
nlen     = $50                  ; and $51: the stream's length
nsrc     = $52                  ; and $53: the next byte of NBUF
ncrc     = $54                  ; and $55: the CRC the host sent
ntry     = $56                  ; the attempts left for the stream
nspun    = $57                  ; the drive whose motor ran before the request

wtrack: lda #p_OPWRITE
        sta p_op
        lda #0
        sta p_err
        sta p_misses
        sta p_index
        sta p_plo
        sta p_phi
        lda p_PKTBUF
        sta p_drive
        lda p_PKTBUF+1
        sta p_track
        lda p_PKTBUF+2
        sta nlen
        lda p_PKTBUF+3
        sta nlen+1
        lda p_drive             ; drive 1 or 2, and a stream that fits
        beq wtbad
        cmp #3
        bcs wtbad
        lda nlen+1
        cmp #(NEND - NBUF) / 256
        bcc wtshow
wtbad:  lda #p_ERRDRIVE
        sta p_err
        jmp p_opdone
wtshow: jsr p_showop
        lda p_spinning          ; a write just after the motor starts waits
        sta nspun               ; another 750 ms for the disk's speed
        jsr p_motoron
        lda nspun
        cmp p_drive
        beq wtknown
        lda #250
        jsr p_delayms
        lda #250
        jsr p_delayms
        lda #250
        jsr p_delayms
wtknown: ldx p_drive            ; an arm whose place isn't known goes to track 0
        lda p_curhalf-1,x
        cmp #$FF
        bne wtseek
        lda #80
        sta p_curhalf-1,x
        lda #0
        sta p_target
        jsr p_stepto
wtseek: lda p_track
        asl a
        sta p_target
        jsr p_stepto
        jsr p_protected
        bcs wtdone
        lda #TRIES
        sta ntry
        lda #p_ACK
wtask:  ldx #0
        ldy #0
        jsr p_sendack
        jsr getnib
        bcc wtgot
        jsr p_drain
        dec ntry
        beq wtlink
        lda #p_NAK
        jmp wtask
wtlink: lda #p_ERRLINK
        sta p_err
        jmp p_opdone
wtgot:  lda #>NBUF
        ldy #0
        jsr p_play
        lda #1
        sta p_index
wtdone: jmp p_opdone

; getnib: the host's Nibble data into NBUF, nlen bytes, then PEND after them
; for the player. C is set on a timeout, a packet that isn't Nibble data or a
; failed CRC. At 115,200 baud a byte comes every 89 cycles, too few for the
; CRC as well, so the bytes are stored as they arrive and checked after.
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
        beq gnpart
gnpage: jsr p_getbtq            ; the whole pages
        bcs gnlost
        sta (nsrc),y
        iny
        bne gnpage
        inc nsrc+1
        dex
        bne gnpage
gnpart: cpy nlen                ; then the rest
        beq gnend
        jsr p_getbtq
        bcs gnlost
        sta (nsrc),y
        iny
        jmp gnpart
gnend:  lda #p_PEND
        sta (nsrc),y
        jsr p_getbtq
        bcs gnlost
        sta ncrc
        jsr p_getbtq
        bcs gnlost
        sta ncrc+1
        jsr p_getbtq            ; the payload's EOR; the CRC checks the stream
        bcs gnlost
        lda #0
        sta p_crclo
        sta p_crchi
        jsr gnstart
        beq gnspart
gnspage: lda (nsrc),y
        jsr p_crcupd
        iny
        bne gnspage
        inc nsrc+1
        dex
        bne gnspage
gnspart: cpy nlen
        beq gncheck
        lda (nsrc),y
        jsr p_crcupd
        iny
        jmp gnspart
gncheck: lda p_crclo
        cmp ncrc
        bne gnlost
        lda p_crchi
        cmp ncrc+1
        bne gnlost
        clc
        rts

; gnstart: nsrc at NBUF, Y 0, and X the stream's whole pages, Z set for none.
gnstart: lda #<NBUF
        sta nsrc
        lda #>NBUF
        sta nsrc+1
        ldy #0
        ldx nlen+1
        rts

modend:

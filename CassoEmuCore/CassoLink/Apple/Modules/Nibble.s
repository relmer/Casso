; ---------------------------------------------------------------------------
; Nibble: a module of the link program. 13-sector tracks read, and a track
; written from a stream of nibbles the host builds, so a disk of any format,
; 13 sectors among them, is written over the link (R49).
;
; Read 13-sector track (CMDRD13): the drive, the track and the sectors to
; read, a bit each, sector 0 in bit 0 of the first byte and sector 12 in
; bit 4 of the second. Each address field that passes, D5 AA B5 or RDOS's
; D4 AA B7, is read, and for a sector requested the data field after it:
; its 410 nibbles and checksum nibble are stored as the latch has them, as
; decoding takes longer than the gap to the next sector, and the host
; decodes them. The read ends when every requested sector is read or after
; about six revolutions. Then, for each sector in physical order, a 13-sector
; data packet (CMDSEC13): the sector, its 411 nibbles and their
; CRC-16/XMODEM, low byte first; or, for a sector whose address field passed
; with no data field after it, as on a track DOS 3.2's INIT formatted and
; never wrote, the sector with its top bit set alone. Then Status, whose
; count is the packets sent.
;
; Write nibble track (CMDWNIBS): the drive, the track and the stream's
; length, low byte first. An Acknowledge requests the stream, and the host's
; Nibble data packet (CMDNIBDATA) has it and its CRC-16/XMODEM, low byte
; first; a NAK requests it again, 8 times at most, then ERRLINK. In the
; stream, as the program's player takes it, each byte with its top bit set is
; a nibble and a 0 is a self-sync byte. The player writes the stream from the
; arm's place on the track, in one pass, then the module sends Status.
; ---------------------------------------------------------------------------

; CMDRD13 and the other commands are assigned ahead of this source by
; LinkProgram, from CassoLinkCodec::kCommands.
NBUF       = $5400              ; the stream, page-aligned for the player
NEND       = $9000
SBUF       = $5400              ; a read's sectors, two pages each
SECTORS    = 13
FIELDLEN   = 411                ; a data field's nibbles and checksum nibble
FIELDHI    = FIELDLEN - 256     ; those in its second page
FIELDS     = 78                 ; address fields to look at: six revolutions
DATAGAP    = 40                 ; nibbles to the data prologue at most
TRIES      = 8

        .org $5000
        .byte 'C', 'M', 3, 2    ; nibble, version 2
        .word PROGVER
        .word modend - $5000
        .word 0                 ; the CRC, which the PC fills in
        .byte 2
        .byte CMDWNIBS
        .word wtrack
        .byte CMDRD13
        .word rtrack

; Zero page the program doesn't use.
nlen     = $50                  ; and $51: the stream's length
nsrc     = $52                  ; and $53: the next byte of NBUF
ncrc     = $54                  ; and $55: the CRC the host sent
ntry     = $56                  ; the attempts left for the stream
nspun    = $57                  ; the drive whose motor ran before the request
rptr     = $58                  ; and $59: a sector's place in SBUF
rbits    = $5A                  ; and $5B: the request's sectors
rleft    = $5C                  ; the sectors requested and not yet read
rtries   = $5D                  ; the address fields left to look at
rsec     = $5E                  ; the sector being read or sent
rvol     = $5F                  ; an address field: volume, track, sector,
rtrk     = $60                  ; checksum
rsct     = $61
rchk     = $62

wtrack: lda #p_OPWRITE
        sta p_op
        jsr nprep
        bcs wtbad
        lda p_PKTBUF+2
        sta nlen
        lda p_PKTBUF+3
        sta nlen+1
        cmp #(NEND - NBUF) / 256 ; a stream that fits
        bcc wtshow
wtbad:  lda #p_ERRDRIVE
        sta p_err
        jmp p_opdone
wtshow: jsr nseek
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

; rtrack: Read 13-sector track. Each sector's rstate is 0 when it isn't
; requested, 1 until its address field passes, 2 once it passes with no
; data field after it and 3 once its data field is read.
rtrack: lda #p_OPREAD
        sta p_op
        jsr nprep
        bcs wtbad
        lda p_PKTBUF+2
        sta rbits
        lda p_PKTBUF+3
        sta rbits+1
        ldx #0
        stx rleft
rtmask: lsr rbits+1
        ror rbits
        lda #0
        rol a
        sta rstate,x
        clc
        adc rleft
        sta rleft
        inx
        cpx #SECTORS
        bne rtmask
        lda rleft
        beq rtsend
        jsr nseek
        lda #FIELDS
        sta rtries
        php                     ; no interrupt while the fields pass
        sei
rtloop: jsr rdaddr
        bcc rthit
        jsr p_missed
        bcs rtend
        bcc rtnext
rthit:  lda #0
        sta p_misses
        lda rtrk
        cmp p_track
        bne rtnext
        ldx rsct
        cpx #SECTORS
        bcs rtnext
        lda rstate,x
        beq rtnext
        cmp #3
        beq rtnext
        stx rsec
        txa
        asl a
        adc #>SBUF
        sta rptr+1
        lda #0
        sta rptr
        jsr rddata
        ldx rsec
        bcc rtgot
        lda #2
        sta rstate,x
        bne rtnext
rtgot:  lda #3
        sta rstate,x
        dec rleft
        beq rtend
rtnext: dec rtries
        bne rtloop
rtend:  plp
rtsend: ldx #0
rsloop: stx rsec
        lda rstate,x
        cmp #2
        bcc rsnext
        beq rsnone
        jsr sendsec
        jmp rscount
rsnone: jsr sendnone
rscount: inc p_index
rsnext: ldx rsec
        inx
        cpx #SECTORS
        bne rsloop
        jmp p_opdone

; rdaddr: the next 13-sector address field, its volume, track, sector and
; checksum in rvol to rchk. The carry is set when none turns up in about a
; revolution, or its epilogue or its checksum is wrong. It times out as the
; program's rdaddr does, so the program's missed counts its misses.
rdaddr: ldx p_diskx
        lda #0
        sta p_tmp
        lda #32
        sta p_tmp2
ra1:    dec p_tmp
        bne ra1w
        dec p_tmp2
        beq rafail
ra1w:   lda p_Q6L,x
        bpl ra1w
ra1c:   cmp #$D5
        beq ra2
        cmp #$D4                ; RDOS's
        bne ra1
ra2:    lda p_Q6L,x
        bpl ra2
        cmp #$AA
        bne ra1c
ra3:    lda p_Q6L,x
        bpl ra3
        cmp #$B5
        beq ra3ok
        cmp #$B7
        bne ra1c
ra3ok:  ldy #0
ra4:    lda p_Q6L,x
        bpl ra4
        sec
        rol a
        sta p_tmp
ra5:    lda p_Q6L,x
        bpl ra5
        and p_tmp
        sta rvol,y
        iny
        cpy #4
        bne ra4
ra6:    lda p_Q6L,x             ; the epilogue's first nibble, before it passes
        bpl ra6
        cmp #$DE
        bne rafail
        lda rvol
        eor rtrk
        eor rsct
        cmp rchk
        bne rafail
        clc
        rts
rafail: sec
        rts

; rddata: the data field after the address field just read, its nibbles at
; (rptr), FIELDLEN of them over two pages. The carry is set when its
; prologue isn't within DATAGAP nibbles.
rddata: ldx p_diskx
        ldy #DATAGAP
rd1:    dey
        beq rdfail
rd1w:   lda p_Q6L,x
        bpl rd1w
rd1c:   cmp #$D5
        bne rd1
rd2:    lda p_Q6L,x
        bpl rd2
        cmp #$AA
        bne rd1c
rd3:    lda p_Q6L,x
        bpl rd3
        cmp #$AD
        bne rd1c
        ldy #0
rdlo:   lda p_Q6L,x
        bpl rdlo
        sta (rptr),y
        iny
        bne rdlo
        inc rptr+1
rdhi:   lda p_Q6L,x
        bpl rdhi
        sta (rptr),y
        iny
        cpy #FIELDHI
        bne rdhi
        clc
        rts
rdfail: sec
        rts

; sendsec: a 13-sector data packet with rsec's nibbles from SBUF.
sendsec: ldx #0
sshdr:  lda sechdr,x
        jsr p_putb
        inx
        cpx #5
        bne sshdr
        lda #0
        sta p_peor
        sta p_crclo
        sta p_crchi
        sta rptr
        lda rsec
        jsr p_putpe
        lda rsec
        asl a
        adc #>SBUF
        sta rptr+1
        ldy #0
sslo:   lda (rptr),y
        jsr p_crcupd
        lda (rptr),y
        jsr p_putpe
        iny
        bne sslo
        inc rptr+1
sshi:   lda (rptr),y
        jsr p_crcupd
        lda (rptr),y
        jsr p_putpe
        iny
        cpy #FIELDHI
        bne sshi
        lda p_crclo
        jsr p_putpe
        lda p_crchi
        jsr p_putpe
        lda p_peor
        jmp p_putb

; sendnone: a 13-sector data packet for rsec with no data field.
sendnone: ldx #0
snhdr:  lda nonehdr,x
        jsr p_putb
        inx
        cpx #5
        bne snhdr
        lda #0
        sta p_peor
        lda rsec
        ora #$80
        jsr p_putpe
        lda p_peor
        jmp p_putb

sechdr:  .byte $C1, <(FIELDLEN+3), >(FIELDLEN+3), CMDSEC13, $C1 ^ <(FIELDLEN+3) ^ >(FIELDLEN+3) ^ CMDSEC13
nonehdr: .byte $C1, 1, 0, CMDSEC13, $C1 ^ 1 ^ CMDSEC13

; nprep: an operation's fields cleared, and the drive and track from the
; request. The carry is set when the drive isn't 1 or 2.
nprep:  lda #0
        sta p_err
        sta p_misses
        sta p_index
        sta p_plo
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
; arm on the track.
nseek:  jsr p_showop
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
nstrack: lda p_track
        asl a
        sta p_target
        jmp p_stepto

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

rstate: .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

modend:

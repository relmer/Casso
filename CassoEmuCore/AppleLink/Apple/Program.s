; Casso's Apple II link: the program.
;
; Loaded at $0800 by the shim. It sends Ready, with its version, the shim's
; and its drive map, and the host's reply is the next operation:
;
;   Idle            nothing to do; Ready again after 100 ms
;   Set speed       a 6551 control byte; ACK, then the new speed
;   Read sectors    drive, track, count, sectors: Sector data for each
;   Write sectors   drive, track, count, sectors: for each, an Acknowledge
;                   with its number, ACK or NAK for another copy, and the
;                   host's Sector data, written and read back
;   Format track    drive, track, volume
;   Cancel          during an operation: it stops, and Ready follows
;   Label           the user's operation, as text: drawn, and Ready again
;   Load program    back to the shim, which sends Ready for the program
;
; Every operation ends with a Status packet: operation, drive, track,
; progress and an error code (0, or one of the ERR values below). A sector is
; the number in its address field, the physical sector; the host maps DOS 3.3
; and ProDOS numbering onto it.
;
; The status screen on the text page has the connection; the PC, connected
; or how long since its last reply; the drive, the track, the operation and
; the progress; the last error, until an operation succeeds; and the last
; Label. Ready is sent again after each second with no reply.
;
; The disk routines are Casso's own, from Apple's published description of
; the Disk II and the 6-and-2 encoding Casso's NibblizationLayer writes: the
; arm is stepped by its phases, sectors are found by their address fields,
; and data fields are read and written in 6-and-2. Writes go through one
; cycle-counted loop, the player, which writes a prepared stream of nibbles
; 32 cycles apart and each self-sync byte 40 or more cycles after the one
; before it.
;
; NMOS 6502 code throughout.

        ifndef PROGVER
PROGVER = 1
        endif
        ifndef SHIMVER
SHIMVER = 1
        endif

; The 6551, through slotx: slot * 16, $20 for the //c's port 2.
ACIADATA = $C088
ACIASTAT = $C089
ACIACMD  = $C08A
ACIACTL  = $C08B
RDRF     = $08
TDRE     = $10

; The Disk II, through diskx: slot * 16, $60 for slot 6.
PHASEOFF = $C080                ; + phase * 2
PHASEON  = $C081
MOTOROFF = $C088
MOTORON  = $C089
DRIVE1   = $C08A
DRIVE2   = $C08B
Q6L      = $C08C
Q6H      = $C08D
Q7L      = $C08E
Q7H      = $C08F

; Commands.
CMDREADY  = $D2
CMDIDLE   = $C9
CMDDATA   = $CA
CMDACK    = $CB
CMDSPEED  = $D7
CMDREAD   = $D5
CMDWRITE  = $D6
CMDFORMAT = $C8
CMDSTATUS = $D4
CMDCANCEL = $D1
CMDLOAD   = $CF
CMDLABEL  = $CE
SHIM      = $3D00
ACK       = $06
NAK       = $15

; Operations, as the Status packet and the screen report them.
OPIDLE   = 0
OPREAD   = 1
OPWRITE  = 2
OPFORMAT = 3
OPSPEED  = 4

; Errors.
ERRNONE    = $00
ERRPROTECT = $10                ; the disk is write-protected
ERRNOTFND  = $20                ; no address field for the sector
ERRDATA    = $30                ; the data field did not decode
ERRVERIFY  = $40                ; the sector read back differently
ERRLINK    = $50                ; the host's data did not arrive whole
ERRDRIVE   = $60                ; no such drive

; Buffers, page-aligned so indexed loads never cross a page.
INVTAB   = $4000                ; nibble -> 6-bit value
CRCLO    = $4100                ; CRC-16/XMODEM table
CRCHI    = $4200
SECBUF   = $4300                ; a sector to write
RDSEC    = $4400                ; a sector read
RDAUX    = $4500                ; read: the 86 two-bit groups
RDPRI    = $4600                ; read: the 256 six-bit values
WRAUX    = $4700                ; write: the 86 two-bit groups
WRPRI    = $4800                ; write: the 256 six-bit values
PKTBUF   = $4900                ; a command's payload
WSTREAM  = $4A00                ; a sector's write stream, two pages
FSTREAM  = $5000                ; a track's format stream, 25 pages
RAWBUF   = $5000                ; read: a track's raw sectors, two pages each, to $6FFF
PENDING  = $4980                ; read: a flag per physical sector not yet read
FPAGES   = 25
WSTART   = 512 - 354            ; the sector stream ends at its second page's end

; Zero page.
slotx    = $06                  ; as the shim set it
diskx    = $E0
drive    = $E1                  ; 1 or 2
track    = $E2
sector   = $E3
volume   = $E4
count    = $E5                  ; sectors in the request
index    = $E6                  ; the one being worked on
op       = $E7
err      = $E8
crclo    = $E9
crchi    = $EA
ptr      = $EB                  ; and $EC
tmp      = $ED
tmp2     = $EE
tmo      = $EF                  ; $EF-$F1: the receive timeout
avol     = $F2                  ; an address field: volume, track, sector,
atrk     = $F3                  ; checksum
asec     = $F4
achk     = $F5
target   = $F6                  ; the half-track the arm is going to
pagecnt  = $F7
tries    = $F8
peor     = $F9                  ; the EOR of a payload
savex    = $FA
envcmd   = $FB
lenlo    = $FC
lenhi    = $FD
curhalf  = $FE                  ; and $FF: each drive's half-track, $FF unknown

        .org $0800

        jmp entry
        .word PROGVER           ; $0803: the version the shim draws

entry:  sei
        cld
        ldx #$FF
        txs
        lda slotx
        bne slotok
        lda #$20
        sta slotx
slotok: clc
        adc #<ACIADATA
        sta pbsta+1
        lda #$60
        sta diskx
        lda #$FF
        sta curhalf
        sta curhalf+1
        lda #OPIDLE
        sta op
        lda #1
        sta drive
        lda #0
        sta track
        sta index
        sta count
        sta pcflag
        sta silent
        sta spinning
        lda #3
        sta tmolen
        jsr mktables
        jsr drawscreen

main:   lda #OPIDLE
        sta op
        jsr showop
        jsr sendready
        lda #1                  ; a second for the reply
        sta tmolen
        jsr getenvt
        lda #3                  ; and three for each byte of an operation
        sta tmolen
        bcc mreply
        jsr nopc
        jmp main
mreply: jsr getpayload
        bcs main
        jsr pcok
        lda envcmd
        cmp #CMDIDLE
        bne notidle
        jsr motoroff
        lda #100
        jsr delayms
        jmp main
notidle: cmp #CMDSPEED
        bne notspeed
        jmp dospeed
notspeed: cmp #CMDREAD
        bne notread
        jmp doread
notread: cmp #CMDWRITE
        bne notwrite
        jmp dowrite
notwrite: cmp #CMDFORMAT
        bne notformat
        jmp doformat
notformat: cmp #CMDLABEL
        bne notlabel
        jmp dolabel
notlabel: cmp #CMDLOAD
        bne tomain
        jmp SHIM
tomain: jmp main

; cancel: the operation stops where it is, and the loop starts again.
cancel: jsr motoroff
        ldx #$FF
        txs
        jmp main


; ---------------------------------------------------------------------------
; Operations
; ---------------------------------------------------------------------------

; Set speed: ACK at the old speed, wait until it is sent, then switch.
dospeed: lda #OPSPEED
        sta op
        jsr showop
        lda #ACK
        ldx #0
        ldy #0
        jsr sendack
        ldx slotx
spdwait: lda ACIASTAT,x
        and #TDRE
        beq spdwait
        lda #3                  ; the last byte's bits on the line
        jsr delayms
        lda PKTBUF
        sta ACIACTL,x
        jsr showconn
        jmp main

; Read sectors: the requested sectors read into TRKBUF in the order they pass
; under the head, then each one's data in the order of the request, then the status.
; Sending a sector takes longer than the gap to the next, so reading and
; sending in turn would wait most of a revolution for every sector.
doread: lda #OPREAD
        jsr begin
        bcs opdone
        jsr readtrack
        bcs opdone
rdnext: jsr showprog
        ldy index
        lda PKTBUF+3,y
        sta sector
        jsr rawslot
        jsr sendsector
        jsr pollcancel
        inc index
        lda index
        cmp count
        bne rdnext
        jsr showprog
; The motor runs on after an operation, so the next track's read does not wait
; for it to spin up again; Idle, with nothing more to do, stops it.
opdone: jsr showerr
        jsr sendstatus
        jmp main

; Write sectors: for each, an Acknowledge with its number, then the host's
; Sector data, checked by its CRC, with a NAK sent for another copy until it
; arrives whole; then written, read back and compared.
dowrite: lda #OPWRITE
        jsr begin
        bcs opdone
        jsr protected
        bcs opdone
wrnext: jsr showprog
        ldy index
        lda PKTBUF+3,y
        sta sector
        lda #8
        sta tries
        lda #ACK
wrask:  ldx sector
        ldy track
        jsr sendack
        jsr getsector
        bcc wrgot
        dec tries
        beq wrlink
        lda #NAK
        jmp wrask
wrlink: lda #ERRLINK
        sta err
        jmp opdone
wrgot:  jsr writesector
        bcs opdone
        jsr readsector
        bcs opdone
        ldy #0
wrcmp:  lda SECBUF,y
        cmp RDSEC,y
        bne wrbad
        iny
        bne wrcmp
        inc index
        lda index
        cmp count
        bne wrnext
        jsr showprog
        jmp opdone
wrbad:  lda #ERRVERIFY
        sta err
        jmp opdone

; Format track: 16 sectors of zeros, numbered 0 to 15 in order round the
; track, under the given volume.
doformat: lda PKTBUF+2
        sta volume
        lda #0
        sta PKTBUF+2            ; no sector count for begin
        lda #OPFORMAT
        jsr begin
        bcs fmtdone
        jsr protected
        bcs fmtdone
        jsr buildformat
        lda #>FSTREAM
        ldy #0
        ldx #FPAGES
        jsr play
        lda #1
        sta count
        sta index
        jsr showprog
fmtdone: jmp opdone

; begin: the operation in A; drive and track from the payload, the sector
; count after them. Draw them, start the motor and seek. The carry is set
; with err when the drive is not 1 or 2.
begin:  sta op
        lda #ERRNONE
        sta err
        lda #0
        sta index
        lda PKTBUF
        sta drive
        lda PKTBUF+1
        sta track
        lda PKTBUF+2
        sta count
        jsr showop
        jsr showdrive
        jsr showtrack
        lda drive
        cmp #1
        beq drvok
        cmp #2
        beq drvok
        lda #ERRDRIVE
        sta err
        sec
        rts
drvok:  jsr motoron
        jsr seek
        clc
        rts


; ---------------------------------------------------------------------------
; Packets
; ---------------------------------------------------------------------------

; sendready: Ready with this program's version, the shim's and the drives:
; bit 0 for drive 1, bit 1 for drive 2.
sendready: ldx #0
srloop: lda readypkt,x
        jsr putb
        inx
        cpx #readyend-readypkt
        bne srloop
        rts

readypkt: .byte $C1, $04, $00, CMDREADY, $C1 ^ $04 ^ $00 ^ CMDREADY
          .byte <PROGVER, >PROGVER, SHIMVER, $03
          .byte <PROGVER ^ >PROGVER ^ SHIMVER ^ $03
readyend:

; sendack: Acknowledge with the kind in A, then X and Y.
sendack: sta tmp
        stx tmp2
        sty acky
        ldx #0
saloop: lda ackhdr,x
        jsr putb
        inx
        cpx #ackend-ackhdr
        bne saloop
        lda tmp
        jsr putb
        lda tmp2
        jsr putb
        lda acky
        jsr putb
        lda tmp
        eor tmp2
        eor acky
        jmp putb

ackhdr: .byte $C1, $03, $00, CMDACK, $C1 ^ $03 ^ $00 ^ CMDACK
ackend:

; sendstatus: Status with the operation, drive, track, progress and error.
sendstatus: ldx #0
ssloop: lda stathdr,x
        jsr putb
        inx
        cpx #statend-stathdr
        bne ssloop
        lda #0
        sta peor
        lda op
        jsr putpe
        lda drive
        jsr putpe
        lda track
        jsr putpe
        lda index
        jsr putpe
        lda err
        jsr putpe
        lda peor
        jmp putb

stathdr: .byte $C1, $05, $00, CMDSTATUS, $C1 ^ $05 ^ $00 ^ CMDSTATUS
statend:

; sendsector: Sector data with the track, the sector, RDSEC and its CRC.
; sendsector: Sector data with the track, the sector, the raw sector at (dst)
; and (dstp) decoded, and its CRC. Each byte is decoded and added to the CRC
; while the 6551 sends the one before, so a track read does not decode at all.
sendsector: ldy #85
ssaux:  lda (dst),y              ; the groups, where the decode can index them
        sta RDAUX,y
        dey
        bpl ssaux
        ldy #0
        sty crclo
        sty crchi
        ldx #0
sdloop: lda datahdr,x
        jsr putb
        inx
        cpx #dataend-datahdr
        bne sdloop
        lda #0
        sta peor
        lda track
        jsr putpe
        lda sector
        jsr putpe
        ldy #0
        ldx #0
sddata: lda RDAUX,x              ; the byte: RDPRI[i] << 2 | two bits from
        and #3                  ; RDAUX[i mod 86], shifted as used
        sta tmp
        lsr RDAUX,x
        lsr RDAUX,x
        lda (dstp),y
        asl a
        asl a
        stx tmp2
        ldx tmp
        ora swap2,x
        sta sbyte
        jsr putpe
        lda sbyte
        jsr crcupd
        ldx tmp2
        inx
        cpx #86
        bne sdnext
        ldx #0
sdnext: iny
        bne sddata
        lda crclo
        jsr putpe
        lda crchi
        jsr putpe
        lda peor
        jmp putb

datahdr: .byte $C1, $04, $01, CMDDATA, $C1 ^ $04 ^ $01 ^ CMDDATA
dataend:

; getsector: the host's Sector data into SECBUF. The carry is set when it
; times out, is not Sector data, is for another sector or fails its CRC.
getsector: jsr getenvt
        bcs gsfail
        cmp #CMDCANCEL
        bne gsdatacmd
        jmp cancel
gsdatacmd: cmp #CMDDATA
        bne gsfail
        jsr getbt               ; track
        bcs gsfail
        jsr getbt               ; sector
        bcs gsfail
        cmp sector
        bne gsfail
        ldy #0
gsdata: jsr getbt
        bcs gsfail
        sta SECBUF,y
        iny
        bne gsdata
        jsr getbt
        bcs gsfail
        sta tmp
        jsr getbt
        bcs gsfail
        sta tmp2
        jsr getbt               ; the payload's EOR; the CRC checks the data
        bcs gsfail
        ldy #0
        sty crclo
        sty crchi
gscrc:  lda SECBUF,y
        jsr crcupd
        iny
        bne gscrc
        lda crclo
        cmp tmp
        bne gsfail
        lda crchi
        cmp tmp2
        bne gsfail
        clc
        rts
gsfail: sec
        rts

; getenvt: an envelope, with a timeout. A and envcmd are the command and
; lenlo and lenhi the payload length, with the carry clear; the carry is set
; on a timeout or a bad check.
getenvt: jsr getbt
        bcs gefail
        cmp #$C1
        bne getenvt
        sta tmp
        jsr getbt
        bcs gefail
        sta lenlo
        eor tmp
        sta tmp
        jsr getbt
        bcs gefail
        sta lenhi
        eor tmp
        sta tmp
        jsr getbt
        bcs gefail
        sta envcmd
        eor tmp
        sta tmp
        jsr getbt
        bcs gefail
        cmp tmp
        bne gefail
        lda envcmd
        clc
        rts
gefail: sec
        rts

; getpayload: a command's payload, at most 255 bytes, into PKTBUF, and its
; EOR. The carry is set on a timeout, a bad EOR or a longer payload.
getpayload: lda lenhi
        bne gpfail
        lda lenlo
        beq gpdone
        ldy #0
        sty peor
gploop: jsr getbt
        bcs gpfail
        sta PKTBUF,y
        eor peor
        sta peor
        iny
        cpy lenlo
        bne gploop
        jsr getbt
        bcs gpfail
        cmp peor
        bne gpfail
gpdone: clc
        rts
gpfail: sec
        rts

; pollcancel: a Cancel the host sent during a read. The host sends nothing
; else then, and while the program is busy with the drive the 6551 has room
; for only the packet's first byte, so any byte is taken as Cancel.
pollcancel: ldx slotx
        lda ACIASTAT,x
        and #RDRF
        beq pcdone
        lda ACIADATA,x
        jmp cancel
pcdone: rts

; putpe: putb, adding A into peor.
putpe:  pha
        eor peor
        sta peor
        pla
        jmp putb

; crcupd: CRC-16/XMODEM of A into crclo and crchi, by the table. Y is saved and restored.
crcupd: sty savex
        eor crchi
        tay
        lda crclo
        eor CRCHI,y
        sta crchi
        lda CRCLO,y
        sta crclo
        ldy savex
        rts

; getbt: a byte through the 6551, waiting about three seconds. The carry is
; set on a timeout. X and Y are saved and restored.
getbt:  stx savex
        ldx slotx
        lda #0
        sta tmo
        sta tmo+1
        lda tmolen
        sta tmo+2
gbwait: lda ACIASTAT,x
        and #RDRF
        bne gbgot
        inc tmo
        bne gbwait
        inc tmo+1
        bne gbwait
        dec tmo+2
        bne gbwait
        ldx savex
        sec
        rts
gbgot:  lda ACIADATA,x
        ldx savex
        clc
        rts

; putb: a byte through the 6551. X and Y are saved and restored. The store
; is absolute, set for the port at startup: an indexed store reads its
; address first, and a read of the 6551's data register clears its receive
; flag.
putb:   stx savex
        ldx slotx
        pha
pbwait: lda ACIASTAT,x
        and #TDRE
        beq pbwait
        pla
pbsta:  sta ACIADATA
        ldx savex
        rts


; ---------------------------------------------------------------------------
; Tables
; ---------------------------------------------------------------------------

; mktables: INVTAB from the write table, and the CRC table.
mktables: ldy #0
        lda #0
mkinv0: sta INVTAB,y
        iny
        bne mkinv0
        ldx #63
mkinv1: lda wrtab,x
        tay
        txa
        sta INVTAB,y
        dex
        bpl mkinv1

        ldx #0
mkcrc:  lda #0
        sta crclo
        stx crchi
        ldy #8
mkbit:  asl crclo
        rol crchi
        bcc mknext
        lda crchi
        eor #$10
        sta crchi
        lda crclo
        eor #$21
        sta crclo
mknext: dey
        bne mkbit
        lda crclo
        sta CRCLO,x
        lda crchi
        sta CRCHI,x
        inx
        bne mkcrc
        rts

; The 6-and-2 write table: the 64 values a data nibble can have.
wrtab:  .byte $96, $97, $9A, $9B, $9D, $9E, $9F, $A6
        .byte $A7, $AB, $AC, $AD, $AE, $AF, $B2, $B3
        .byte $B4, $B5, $B6, $B7, $B9, $BA, $BB, $BC
        .byte $BD, $BE, $BF, $CB, $CD, $CE, $CF, $D3
        .byte $D6, $D7, $D9, $DA, $DB, $DC, $DD, $DE
        .byte $DF, $E5, $E6, $E7, $E9, $EA, $EB, $EC
        .byte $ED, $EE, $EF, $F2, $F3, $F4, $F5, $F6
        .byte $F7, $F9, $FA, $FB, $FC, $FD, $FE, $FF

; A byte's two low bits, swapped, in the auxiliary groups' order.
swap2:  .byte $00, $02, $01, $03


; ---------------------------------------------------------------------------
; The drive
; ---------------------------------------------------------------------------

; motoron: select the drive, start its motor, read mode, and wait for it to
; come up to speed, unless that drive's motor is running already.
motoron: ldx diskx
        lda drive
        cmp spinning
        bne monstart
        lda Q6L,x
        lda Q7L,x
        rts
monstart: sta spinning
        lda drive
        cmp #2
        beq mon2
        lda DRIVE1,x
        jmp mon
mon2:   lda DRIVE2,x
mon:    lda MOTORON,x
        lda Q6L,x
        lda Q7L,x
        lda #250
        jmp delayms

motoroff: ldx diskx
        lda MOTOROFF,x
        lda #0
        sta spinning
        rts

; seek: the arm to `track`. Unknown at first, the arm is taken to track 0
; from as far out as it can be, as the boot ROM does. Then the track is
; checked against an address field, and when the arm turns out to be on
; another track, as DOS also allows for, it steps on from there. On a track
; with no address fields, such as an unformatted one, the arm is not moved
; further.
seek:   jsr seekto
        lda #3
        sta vtries
skcheck: jsr rdaddr
        bcs sknext
        lda atrk
        cmp track
        beq skdone
        asl a
        ldx drive
        sta curhalf-1,x
        jsr seekto
sknext: dec vtries
        bne skcheck
skdone: rts

seekto: ldx drive
        lda curhalf-1,x
        cmp #$FF
        bne seekknown
        lda #80
        sta curhalf-1,x
        lda #0
        sta target
        jsr stepto
seekknown: lda track
        asl a
        sta target
        jsr stepto
        rts

; stepto: one half-track at a time to `target`: the next phase on, a pause,
; the last one off; at the end, a settling pause and the phase off.
stepto: ldx drive
        lda curhalf-1,x
        cmp target
        beq stepdone
        bcc stepup
        sec
        sbc #1
        jmp stepmove
stepup: clc
        adc #1
stepmove: ldy curhalf-1,x       ; the phase that is on
        sta curhalf-1,x
        and #3
        asl a
        ora diskx
        tax
        lda PHASEON,x
        lda #12
        jsr delayms
        tya
        and #3
        asl a
        ora diskx
        tax
        lda PHASEOFF,x
        jmp stepto
stepdone: lda #25
        jsr delayms
        lda curhalf-1,x
        and #3
        asl a
        ora diskx
        tax
        lda PHASEOFF,x
        rts

; protected: the carry is set, with err, when the disk is write-protected.
protected: ldx diskx
        lda Q6H,x
        lda Q7L,x
        asl a
        lda Q6L,x
        bcc wpok
        lda #ERRPROTECT
        sta err
        sec
        rts
wpok:   clc
        rts

; findsector: the address field of `sector` on `track`. The carry is set,
; with err, after enough tries to have passed it several times over.
findsector: lda #48
        sta tries
fsloop: jsr rdaddr
        bcs fsnext
        lda atrk
        cmp track
        bne fsnext
        lda asec
        cmp sector
        bne fsnext
        clc
        rts
fsnext: dec tries
        bne fsloop
        lda #ERRNOTFND
        sta err
        sec
        rts

; rdaddr: the next address field, its volume, track, sector and checksum in
; avol to achk. The carry is set when none turns up in about a revolution, or
; its epilogue or its checksum is wrong.
rdaddr: ldx diskx
        lda #0
        sta tmp
        lda #32
        sta tmp2
ra1:    dec tmp
        bne ra1w
        dec tmp2
        beq rafail
ra1w:   lda Q6L,x
        bpl ra1w
ra1c:   cmp #$D5
        bne ra1
ra2:    lda Q6L,x
        bpl ra2
        cmp #$AA
        bne ra1c
ra3:    lda Q6L,x
        bpl ra3
        cmp #$96
        bne ra1c
        ldy #0
ra4:    lda Q6L,x
        bpl ra4
        sec
        rol a
        sta tmp
ra5:    lda Q6L,x
        bpl ra5
        and tmp
        sta avol,y
        iny
        cpy #4
        bne ra4
ra6:    lda Q6L,x               ; the epilogue's first nibble, before it passes
        bpl ra6
        cmp #$DE
        bne rafail
        lda avol
        eor atrk
        eor asec
        cmp achk
        bne rafail
        clc
        rts
rafail: sec
        rts

; readsector: `sector`'s data into RDSEC. The carry is set, with err, when
; the address field is not found or the data field does not decode.
readsector: lda #4
        sta pagecnt             ; data-field attempts
rsagain: jsr findsector
        bcc rsfound
        rts
rsfound: jsr readdata
        bcc rsdone
rsretry: dec pagecnt
        beq rsgive
        jmp rsagain
rsgive: lda #ERRDATA
        sta err
rsfail: sec
        rts
rsdone: rts

; readraw: the data field after the address field just read, its 86 two-bit
; groups at (dst) and its 256 six-bit values at (dstp), not yet decoded. The
; carry is set when it is not found or its checksum is wrong. Decoding takes
; longer than the gap to the next sector, so a track read stores the raw form.
readraw: ldx diskx
        ldy #32                 ; the data prologue comes within a few nibbles
rd1:    dey
        beq rrbad
rd1w:   lda Q6L,x
        bpl rd1w
rd1c:   cmp #$D5
        bne rd1
rd2:    lda Q6L,x
        bpl rd2
        cmp #$AA
        bne rd1c
rd3:    lda Q6L,x
        bpl rd3
        cmp #$AD
        bne rd1c

        lda #0                  ; the running value
        ldy #0
rdaux:  sty tmp
rdauxw: ldy Q6L,x
        bpl rdauxw
        eor INVTAB,y
        ldy tmp
        sta (dst),y
        iny
        cpy #86
        bne rdaux
        ldy #0
rdpri:  sty tmp
rdpriw: ldy Q6L,x
        bpl rdpriw
        eor INVTAB,y
        ldy tmp
        sta (dstp),y
        iny
        bne rdpri
rdchk:  ldy Q6L,x
        bpl rdchk
        eor INVTAB,y
        bne rrbad
        clc
        rts
rrbad:  sec
        rts

; readdata: the data field into RDAUX and RDPRI, decoded into RDSEC. The
; carry is set when it is not found or does not decode.
readdata: lda #<RDAUX
        sta dst
        lda #>RDAUX
        sta dst+1
        lda #<RDPRI
        sta dstp
        lda #>RDPRI
        sta dstp+1
        jsr readraw
        bcs rdbad

        ldy #0                  ; RDSEC[i] = RDPRI[i] << 2 | the two bits
        ldx #0                  ; from RDAUX[i mod 86], shifted as used
rddec:  lda RDAUX,x
        and #3
        sta tmp
        lsr RDAUX,x
        lsr RDAUX,x
        lda RDPRI,y
        asl a
        asl a
        stx tmp2
        ldx tmp
        ora swap2,x
        ldx tmp2
        sta RDSEC,y
        inx
        cpx #86
        bne rddnext
        ldx #0
rddnext: iny
        bne rddec
        clc
        rts
rdbad:  sec
        rts

; readtrack: each requested sector of `track` into RAWBUF, raw, two pages per
; physical sector, taken as its address field passes. The carry is set, with
; err, when some are not read after several revolutions.
readtrack: ldx #15
        lda #0
rtclear: sta PENDING,x
        dex
        bpl rtclear
        ldy #0
rtmark: lda PKTBUF+3,y
        and #15
        tax
        lda #1
        sta PENDING,x
        iny
        cpy count
        bne rtmark
        lda count
        sta left
        lda #96                 ; address fields to look at: six revolutions
        sta tries
rtloop: jsr rdaddr
        bcs rtnext
        lda atrk
        cmp track
        bne rtnext
        ldx asec
        cpx #16
        bcs rtnext
        lda PENDING,x
        beq rtnext
        stx sector
        jsr rawslot
        jsr readraw
        bcs rtnext
        ldx sector
        lda #0
        sta PENDING,x
        dec left
        beq rtdone
rtnext: dec tries
        bne rtloop
        lda #ERRNOTFND
        sta err
        sec
        rts
rtdone: clc
        rts

; rawslot: dst and dstp at `sector`'s two pages in RAWBUF, its 86 groups and
; its 256 values.
rawslot: lda sector
        asl a
        clc
        adc #>RAWBUF
        sta dst+1
        clc
        adc #1
        sta dstp+1
        lda #0
        sta dst
        sta dstp
        rts
; writesector: SECBUF to `sector`. The stream is built first, so writing can
; start the moment the sector's address field has passed.
writesector: jsr encode
        jsr buildwrite
        jsr findsector
        bcs wsfail
        lda #>WSTREAM
        ldy #<WSTART
        ldx #2
        jsr play
        clc
        rts
wsfail: sec
        rts

; encode: SECBUF as 6-and-2: WRAUX's 86 two-bit groups, then WRPRI's 256
; six-bit values, as NibblizationLayer lays them out.
encode: ldx #0
enaux:  lda SECBUF,x
        and #3
        tay
        lda swap2,y
        sta tmp
        lda SECBUF+86,x
        and #3
        tay
        lda swap2,y
        asl a
        asl a
        ora tmp
        sta tmp
        cpx #84
        bcs enaux2
        lda SECBUF+172,x
        and #3
        tay
        lda swap2,y
        asl a
        asl a
        asl a
        asl a
        ora tmp
        sta tmp
enaux2: lda tmp
        sta WRAUX,x
        inx
        cpx #86
        bne enaux
        ldx #0
enpri:  lda SECBUF,x
        lsr a
        lsr a
        sta WRPRI,x
        inx
        bne enpri
        rts

; buildwrite: WSTREAM from WSTART: five self-sync bytes, the data prologue,
; the 343 data nibbles and the epilogue, ending at the second page's end.
buildwrite: lda #<(WSTREAM+WSTART)
        sta ptr
        lda #>(WSTREAM+WSTART)
        sta ptr+1
        ldx #5
bwsync: lda #0
        jsr putstream
        dex
        bne bwsync
        jsr putdata
        rts

; putdata: a data field into the stream: D5 AA AD, the 342 nibbles from WRAUX
; and WRPRI, each the EOR of its value with the one before, the checksum and
; DE AA EB.
putdata: lda #$D5
        jsr putstream
        lda #$AA
        jsr putstream
        lda #$AD
        jsr putstream
        lda #0
        sta tmp2                ; the value before
        ldx #0
pdaux:  lda WRAUX,x
        eor tmp2
        tay
        lda wrtab,y
        jsr putstream
        lda WRAUX,x
        sta tmp2
        inx
        cpx #86
        bne pdaux
        ldx #0
pdpri:  lda WRPRI,x
        eor tmp2
        tay
        lda wrtab,y
        jsr putstream
        lda WRPRI,x
        sta tmp2
        inx
        bne pdpri
        ldy tmp2
        lda wrtab,y
        jsr putstream
        lda #$DE
        jsr putstream
        lda #$AA
        jsr putstream
        lda #$EB
        jmp putstream

; putstream: A at (ptr), and ptr on. X and Y are saved and restored.
putstream: sty savex
        ldy #0
        sta (ptr),y
        ldy savex
        inc ptr
        bne psdone
        inc ptr+1
psdone: rts

; buildformat: FSTREAM, a whole track of 25 pages: 288 self-sync bytes, then
; for each sector 0 to 15 its address field, 5 self-sync bytes, a data field
; of zeros and 14 self-sync bytes.
buildformat: ldy #0
        lda #0
bfzero: sta SECBUF,y
        iny
        bne bfzero
        jsr encode
        lda #<FSTREAM
        sta ptr
        lda #>FSTREAM
        sta ptr+1
        ldx #0                  ; 288: past a revolution, so the stream's end
bfgap1: lda #0                  ; overwrites only this gap
        jsr putstream
        dex
        bne bfgap1
        ldx #32
bfgap1b: lda #0
        jsr putstream
        dex
        bne bfgap1b
        lda #0
        sta sector
bfsec:  lda #$D5
        jsr putstream
        lda #$AA
        jsr putstream
        lda #$96
        jsr putstream
        lda volume
        jsr put44
        lda track
        jsr put44
        lda sector
        jsr put44
        lda volume
        eor track
        eor sector
        jsr put44
        lda #$DE
        jsr putstream
        lda #$AA
        jsr putstream
        lda #$EB
        jsr putstream
        ldx #5
bfgap2: lda #0
        jsr putstream
        dex
        bne bfgap2
        jsr putdata
        ldx #14
bfgap3: lda #0
        jsr putstream
        dex
        bne bfgap3
        inc sector
        lda sector
        cmp #16
        bne bfsec
        rts

; put44: A as two 4-and-4 nibbles: the odd bits, then the even.
put44:  sta tmp
        lsr a
        ora #$AA
        jsr putstream
        lda tmp
        ora #$AA
        jmp putstream

; play: the stream from page A, offset Y, through the end of the X-th page,
; written by the drive. A 0 in the stream is a self-sync byte.
;
; Cycles from one store to the next: data to data 32; data to sync 40; sync
; to sync 43; sync to data 35; 3 more where the stream crosses a page. The
; extra time puts zero bits between nibbles, which the read hardware skips.
play:   sta plda+2
        stx pagecnt
        ldx diskx
        lda Q6H,x
        lda Q7L,x
        lda #$FF
        sta Q7H,x               ; write mode, a self-sync byte loaded
        ora Q6L,x
        jsr dly12               ; the first store comes 40 cycles on
        nop
        jmp plda
plda:   lda WSTREAM,y           ; 4   (the page is set above)
        beq psync               ; 2 / 3
        sta Q6H,x               ; 5
        ora Q6L,x               ; 4   at once, or the latch loads twice
pnext:  iny                     ; 2
        bne ppad                ; 3 / 2
        inc plda+2              ; 6
        dec pagecnt             ; 5
        beq pdone               ; 2 / 3
        jmp plda                ; 3
ppad:   nop                     ; 2
        nop                     ; 2
        nop                     ; 2
        bit $00                 ; 3
        jmp plda                ; 3
psync:  nop                     ; 2
        bit $00                 ; 3
        lda #$FF                ; 2
        sta Q6H,x               ; 5
        ora Q6L,x               ; 4
        jmp pnext               ; 3
pdone:  jsr dly12               ; the last nibble shifts out
        jsr dly12
        lda Q7L,x               ; read mode
        lda Q6L,x
        rts

dly12:  rts                     ; jsr and rts: 12 cycles


; ---------------------------------------------------------------------------
; Timing
; ---------------------------------------------------------------------------

; delayms: about A milliseconds at the //c's 1.02 MHz. Y is saved and
; restored, and X is not used.
delayms: sty savex
dlouter: ldy #203
dlinner: dey
        bne dlinner
        sec
        sbc #1
        bne dlouter
        ldy savex
        rts


; ---------------------------------------------------------------------------
; The status screen
; ---------------------------------------------------------------------------

ROW0 = $0400
ROW2 = $0500
ROW3 = $0580
ROW4 = $0600
ROW5 = $0680
ROW6 = $0700
ROW7 = $0780
ROW8 = $0428
ROW10 = $0528
FIELD = 12
FIELDLEN = 20
scr = $13                       ; and $14: the field being drawn
vtries = $15                    ; seek: address fields left to check
acky = $16                      ; sendack: its third byte
pcflag = $17                    ; nonzero once the PC has replied
silent = $18                    ; seconds since its last reply
tmolen = $19                    ; getbt's timeout, in about seconds
left = $1A                      ; readtrack: sectors not yet read
spinning = $1B                  ; the drive whose motor is running, or 0
dst = $1C                       ; and $1D: a raw sector's 86 groups
dstp = $1E                      ; and $1F: its 256 values
sbyte = $20                     ; sendsector: the byte being sent

; drawscreen: clear the text page and draw the labels.
drawscreen: lda #$A0
        ldy #0
dsclear: sta $0400,y
        sta $0500,y
        sta $0600,y
        sta $0700,y
        iny
        bne dsclear
        ldx #0
dsline: lda labels+1,x          ; row low, row high, text, 0; a 0 row ends
        beq dsdone
        sta ptr+1
        lda labels,x
        sta ptr
        inx
        inx
        ldy #0
dstext: lda labels,x
        beq dsnext
        ora #$80
        sta (ptr),y
        inx
        iny
        bne dstext
dsnext: inx
        jmp dsline
dsdone: jsr showconn
        jsr showpc
        jsr showop
        jsr showdrive
        jsr showtrack
        jmp showprog

labels: .byte <ROW0, >ROW0
        .text "CASSO APPLE II LINK"
        .byte 0
        .byte <ROW2, >ROW2
        .text "CONNECTION"
        .byte $3A, 0
        .byte <ROW3, >ROW3
        .text "PC"
        .byte $3A, 0
        .byte <ROW4, >ROW4
        .text "DRIVE"
        .byte $3A, 0
        .byte <ROW5, >ROW5
        .text "TRACK"
        .byte $3A, 0
        .byte <ROW6, >ROW6
        .text "OPERATION"
        .byte $3A, 0
        .byte <ROW7, >ROW7
        .text "PROGRESS"
        .byte $3A, 0
        .byte <ROW8, >ROW8
        .text "ERROR"
        .byte $3A, 0
        .byte 0
        .byte 0, 0

; field: scr at the field of the row whose address is in X (low) and A
; (high), and the field cleared. Y is 0.
field:  stx scr
        sta scr+1
        lda scr
        clc
        adc #FIELD
        sta scr
        ldy #FIELDLEN-1
        lda #$A0
fdclear: sta (scr),y
        dey
        bpl fdclear
        iny
        rts

; puttext: the 0-terminated text at (ptr) into the field from Y.
puttext: sty tmp
        ldy #0
pttext: lda (ptr),y
        beq ptdone
        sty tmp2
        ldy tmp
        jsr putch
        sty tmp
        ldy tmp2
        iny
        bne pttext
ptdone: ldy tmp
        rts

; putch: A into the field at Y, and Y on.
putch:  ora #$80
        sta (scr),y
        iny
        rts

; putdec: A in decimal, without leading zeros, into the field from Y.
putdec: ldx #0                  ; hundreds
pdh:    cmp #100
        bcc pdh2
        sbc #100
        inx
        bne pdh
pdh2:   sta tmp2
        txa
        beq pdt
        ora #'0'
        jsr putch
        lda #1                  ; a hundreds digit forces the tens
        sta tmp
        bne pdt1
pdt:    sta tmp
pdt1:   lda tmp2
        ldx #0
pdtl:   cmp #10
        bcc pdt2
        sbc #10
        inx
        bne pdtl
pdt2:   sta tmp2
        txa
        ora tmp
        beq pdu
        txa
        ora #'0'
        jsr putch
pdu:    lda tmp2
        ora #'0'
        jmp putch

; showconn: the port and the speed the 6551 is set to.
showconn: ldx #<ROW2
        lda #>ROW2
        jsr field
        lda #<conntext
        sta ptr
        lda #>conntext
        sta ptr+1
        jsr puttext
        ldx slotx
        lda ACIACTL,x
        and #$0F
        ldx #0
scfind: cmp speeds,x
        beq scfound
        inx
        inx
        inx
        cpx #speedend-speeds
        bne scfind
scfound: lda speeds+1,x
        sta ptr
        lda speeds+2,x
        sta ptr+1
        jmp puttext

conntext: .text "PORT 2, "
        .byte 0


s9600:   .text "9600"
         .byte 0
s19200:  .text "19200"
         .byte 0
s115200: .text "115200"
         .byte 0
s4800:   .text "4800"
         .byte 0
s2400:   .text "2400"
         .byte 0
s1200:   .text "1200"
         .byte 0
sother:  .text "OTHER"
         .byte 0
; The 6551's rate bits and their text; the last entry matches any other.
; After the text, as the assembler sizes a forward < or > in .byte wrongly.
speeds: .byte $0E, <s9600, >s9600
        .byte $0F, <s19200, >s19200
        .byte $00, <s115200, >s115200
        .byte $0C, <s4800, >s4800
        .byte $0A, <s2400, >s2400
        .byte $08, <s1200, >s1200
speedend:
        .byte $FF, <sother, >sother

; showop: the operation.
showop: ldx #<ROW6
        lda #>ROW6
        jsr field
        lda op
        asl a
        tax
        lda optext,x
        sta ptr
        lda optext+1,x
        sta ptr+1
        jsr puttext
        lda err
        beq sodone
        lda op
        beq sodone
        lda #' '
        jsr putch
        lda #'E'
        jsr putch
        lda err
        lsr a
        lsr a
        lsr a
        lsr a
        ora #'0'
        jsr putch
sodone: rts

optext: .word oidle, oread, owrite, oformat, ospeed

oidle:   .text "WAITING"
         .byte 0
oread:   .text "READING"
         .byte 0
owrite:  .text "WRITING"
         .byte 0
oformat: .text "FORMATTING"
         .byte 0
ospeed:  .text "SETTING SPEED"
         .byte 0

; showdrive: the drive.
showdrive: ldx #<ROW4
        lda #>ROW4
        jsr field
        lda drive
        jmp putdec

; showtrack: the track.
showtrack: ldx #<ROW5
        lda #>ROW5
        jsr field
        lda track
        jmp putdec

; showprog: sectors done, of the request's count.
showprog: ldx #<ROW7
        lda #>ROW7
        jsr field
        lda index
        jsr putdec
        lda #' '
        jsr putch
        lda #'O'
        jsr putch
        lda #'F'
        jsr putch
        lda #' '
        jsr putch
        lda count
        jmp putdec

; showpc: the PC: waiting for its first reply, connected, or how long since
; its last reply.
showpc: ldx #<ROW3
        lda #>ROW3
        jsr field
        lda silent
        bne spquiet
        lda #<pcwait
        ldx #>pcwait
        ldy pcflag
        beq sptext
        lda #<pcconn
        ldx #>pcconn
sptext: sta ptr
        stx ptr+1
        ldy #0
        jmp puttext
spquiet: lda #<pcnone
        sta ptr
        lda #>pcnone
        sta ptr+1
        ldy #0
        jsr puttext
        lda silent
        jsr putdec
        lda #' '
        jsr putch
        lda #'S'
        jmp putch

; pcok: a reply arrived. The line is drawn again only when it changes, as a
; command's reply follows.
pcok:   lda pcflag
        beq pcnew
        lda silent
        beq pcsame
pcnew:  lda #1
        sta pcflag
        lda #0
        sta silent
        jmp showpc
pcsame: rts

; nopc: a second with no reply.
nopc:   inc silent
        bne npdraw
        dec silent              ; it stays at 255
npdraw: jmp showpc

pcwait: .text "WAITING"
        .byte 0
pcconn: .text "CONNECTED"
        .byte 0
pcnone: .text "NO REPLY FOR "
        .byte 0

; showerr: the last operation's error and its track, or the line cleared
; when it succeeded.
showerr: ldx #<ROW8
        lda #>ROW8
        jsr field
        lda err
        beq sedone
        lsr a
        lsr a
        lsr a
        cmp #errtextend-errtext
        bcc seknown
        lda #0
seknown: tax
        lda errtext,x
        sta ptr
        lda errtext+1,x
        sta ptr+1
        jsr puttext
        lda #' '
        jsr putch
        lda #'T'
        jsr putch
        lda track
        jmp putdec
sedone: rts

; The error codes are $10 apart, so the code shifted right three times is
; the index of its text's address; the first entry is for any other code.
errtext: .word eother, eprotect, enotfnd, edata, everify, elink, edrive
errtextend:

eother:   .text "FAILED"
          .byte 0
eprotect: .text "WRITE-PROTECTED"
          .byte 0
enotfnd:  .text "NO SECTOR"
          .byte 0
edata:    .text "BAD DATA"
          .byte 0
everify:  .text "VERIFY FAILED"
          .byte 0
elink:    .text "LINK DATA DAMAGED"
          .byte 0
edrive:   .text "NO SUCH DRIVE"
          .byte 0

; dolabel: the host's text for the user's operation across the label row,
; then Ready again.
dolabel: ldy #39
        lda #$A0
lbclear: sta ROW10,y
        dey
        bpl lbclear
        ldy #0
lbtext: cpy lenlo
        beq lbdone
        cpy #40
        beq lbdone
        lda PKTBUF,y
        ora #$80
        sta ROW10,y
        iny
        bne lbtext
lbdone: jmp main

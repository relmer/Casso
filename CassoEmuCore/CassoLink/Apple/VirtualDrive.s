; ---------------------------------------------------------------------------
; Casso's CassoLink: the virtual drives' block driver and its installer
; (R44, FR-200).
;
; The CassoLink ProDOS disk's STARTUP runs CASSOLINK, this installer and the
; driver after it, from $2000. BASIC.SYSTEM has started by then, so its
; GETBUFR allocates five pages below HIMEM for the driver: the driver in the
; first three and its CRC tables in the others. The driver is assembled at DRVORG
; = $9200, and each byte that has its high byte is in the relocation list
; after it, which the PC makes by assembling it at two pages; the installer
; adds GETBUFR's page less DRVPAGE to each.
;
; The serial port is the first slot from 7 down with the Super Serial
; Card's identification bytes, set to SPEED's 6551 control byte, as the
; connection's speed. A //c has a cable on port 1 or port 2, so both are
; set up, and the driver switches to the other port after each request
; with no reply, so it stays on the port with the cable. The drives are drive 1 and 2 of the first
; slot from 7 down, other than the serial port's, whose two entries in
; ProDOS's device table are both the no-device entry, slot 0's.
;
; Each call sends Virtual block (CMDVBLOCK) with the drive, the operation,
; the block and for a write the block's bytes and their CRC, and waits about
; three seconds for the PC's reply: ProDOS's error, then for a status the
; drive's blocks and for a read the block and its CRC. A reply that doesn't
; arrive or arrives damaged is requested again, 4 times in all, then the
; call returns ProDOS's I/O error. When no byte came back for any of them,
; CASSOLINK ISN'T CONNECTED is also written straight to the text screen's
; bottom line, since ProDOS banks its memory over the ROM's COUT while a
; driver runs. Each status request has the ROM's identification bytes after
; the block, for the PC's page. The installer calls the driver once for
; drive 1's status,
; sent once on each port and the screen message off, and prints the
; same message when no reply arrives. Interrupts are off for the call. The
; driver doesn't use the zero page except ProDOS's request, at $42 to $47.
; ---------------------------------------------------------------------------

ACIADATA = $C088
ACIASTAT = $C089
ACIACMD  = $C08A
ACIACTL  = $C08B
RDRF     = $08
TDRE     = $10

PDCMD    = $42                  ; ProDOS's request: the command
PDUNIT   = $43                  ; the unit, bit 7 set for drive 2
PDBUF    = $44                  ; and $45: the buffer
PDBLK    = $46                  ; and $47: the block

ERRIO    = $27
TRIES    = 4
STARTB   = $C1                  ; a packet's first byte
WRLEN    = 4 + 512 + 2          ; a write's payload
IDLEN    = 5                    ; the ROM's identification bytes

DRVPAGE  = $92

RD80VID  = $C01F                ; bit 7 set in 80 columns
RD80STOR = $C018
CLR80STOR = $C000
SET80STOR = $C001
PAGE2OFF = $C054
PAGE2ON  = $C055
BOTLINE  = $07D0                ; the text screen's line 24

        ifdef DRIVER

        .org DRVORG

CRCLO    = DRVORG + $300
CRCHI    = DRVORG + $400

driver: php
        sei
        lda PDBUF+1
        sta dvbufh
        lda PDCMD               ; status, read and write; FORMAT isn't served
        cmp #3
        bcc dvok
        lda #ERRIO
        bne dvend               ; always
dvok:   sta dvop
        cmp #2
        bne dvtries
        jsr bcrc
dvtries: lda dvmax
        sta dvtry
        lda #0
        sta dvhear
dvagain: jsr request
        jsr reply
        bcc dvend
        jsr drain
        lda sltx                ; a //c's other port next
        eor altx
        sta sltx
        dec dvtry
        bne dvagain
        lda dvhear              ; nothing from the PC at all
        ora dvquiet
        bne dvio
        jsr shownc
dvio:   lda #ERRIO
dvend:  sta dverr
        lda dvbufh
        sta PDBUF+1
        plp
        ldx dvcnt
        ldy dvcnt+1
        lda dverr
        cmp #1                  ; C set for an error
        rts

; request: Virtual block, with the block for a write. The header's EOR is
; sent through putp, so peor is 0 after it, for the payload's.
request: lda dvbufh
        sta PDBUF+1
        ldx sltx
        lda #0
        sta peor
        lda #STARTB
        jsr putp
        ldy dvop
        lda rqlens,y
        ldy dvop                ; a status has the ROM's bytes
        bne rqlen
        clc
        adc #IDLEN
rqlen:  ldy dvop
        jsr putp
        lda rqlenh,y
        jsr putp
        lda #CMDVBLOCK
        jsr putp
        lda peor
        jsr putp
        lda PDUNIT              ; drive 1, or 2 with bit 7 set
        asl a
        lda #0
        rol a                   ; C clear after
        adc #1
        jsr putp
        lda dvop
        jsr putp
        lda PDBLK
        jsr putp
        lda PDBLK+1
        jsr putp
        lda dvop
        bne rqwrite
        ldy #0
rqid:   lda dvid,y
        jsr putp
        iny
        cpy #IDLEN
        bne rqid
rqwrite: lda dvop
        cmp #2
        bne rqend
        ldy #0
        lda #2
        sta pages
rqdata: lda (PDBUF),y
        jsr putp
        iny
        bne rqdata
        inc PDBUF+1
        dec pages
        bne rqdata
        lda crclo
        jsr putp
        lda crchi
        jsr putp
rqend:  lda peor
        jmp putb

rqlens: .byte 4, 4, <WRLEN
rqlenh: .byte 0, 0, >WRLEN

; reply: the PC's Virtual block. C is set on a timeout or a damaged reply;
; otherwise A is ProDOS's error from the PC. A timeout in getbt returns
; from here, by the stack pointer saved on entry.
reply:  tsx
        stx rpsp
        ldx sltx
rpstart: jsr getbt
        stx dvhear              ; X is the slot times 16, never 0
        cmp #STARTB
        bne rpstart
        sta peor
        jsr getp                ; the length, low and high
        jsr getp
        jsr getp                ; the command
        cmp #CMDVBLOCK
        bne rpfail
        jsr getbt               ; the header's EOR
        cmp peor
        bne rpfail
        jsr getbt               ; ProDOS's error
        sta dverr
        lda dvop
        beq rpstat
        cmp #1
        beq rpread
rpdone: jsr getbt               ; the payload's EOR
        lda dverr
        clc
        rts
rpstat: jsr getbt
        sta dvcnt
        jsr getbt
        sta dvcnt+1
        jmp rpdone
rpread: ldy #0
        lda #2
        sta pages
rpdata: jsr getbt
        sta (PDBUF),y
        iny
        bne rpdata
        inc PDBUF+1
        dec pages
        bne rpdata
        jsr getbt               ; the block's CRC
        sta rqcrc
        jsr getbt
        sta rqcrc+1
        lda dvbufh
        sta PDBUF+1
        jsr bcrc                ; while the payload's EOR waits in the 6551
        ldx sltx
        lda crclo
        cmp rqcrc
        bne rpfail
        lda crchi
        cmp rqcrc+1
        beq rpdone
rpfail: sec
        rts

; getp: getbt, with the byte added to the header's EOR.
getp:   jsr getbt
        pha
        eor peor
        sta peor
        pla
        rts

; getbt: a byte from the PC in A. After about three seconds with none, the
; stack is as reply's entry had it, and reply returns with C set. getbtq
; waits about a seventh of a second, for drain.
getbt:  lda #0
        sta tmo+1
        lda #3
        bne gbset               ; always
getbtq: lda #$E0
        sta tmo+1
        lda #1
gbset:  sta tmo+2
        lda #0
        sta tmo
gbwait: lda ACIASTAT,x
        and #RDRF
        bne gbgot
        inc tmo
        bne gbwait
        inc tmo+1
        bne gbwait
        dec tmo+2
        bne gbwait
        ldx rpsp
        txs
        sec
        rts
gbgot:  lda ACIADATA,x
        rts

; putp: A sent and added to peor. putb: A sent.
putp:   pha
        eor peor
        sta peor
        pla
putb:   pha
pbwait: lda ACIASTAT,x
        and #TDRE
        beq pbwait
        pla
        sta ACIADATA,x
        rts

; drain: the rest of a damaged reply dropped, until the line is quiet; the
; timeout returns from here as from reply.
drain:  tsx
        stx rpsp
        ldx sltx
drloop: jsr getbtq
        jmp drloop

; bcrc: the CRC-16/XMODEM of the block at PDBUF into crclo and crchi, by the
; tables. X is changed, and PDBUF's high byte is a page on.
bcrc:   lda #0
        sta crclo
        sta crchi
        tay
        lda #2
        sta pages
bcdata: lda (PDBUF),y
        eor crchi
        tax
        lda crclo
        eor CRCHI,x
        sta crchi
        lda CRCLO,x
        sta crclo
        iny
        bne bcdata
        inc PDBUF+1
        dec pages
        bne bcdata
        rts

; shownc: CASSOLINK ISN'T CONNECTED on the text screen's bottom line. In 80
; columns, an even column is in auxiliary memory, written with 80STORE and
; PAGE2 on, and an odd one in main memory.
shownc: ldy #0
        lda RD80VID
        bmi sn80
sn40:   lda dvmsg,y
        beq sndone
        ora #$80
        sta BOTLINE,y
        iny
        bne sn40                ; always
sn80:   lda RD80STOR
        sta snst
        sta SET80STOR
snloop: lda dvmsg,y
        beq sn80end
        ora #$80
        pha
        tya
        lsr a
        tax
        pla
        bcs snmain
        sta PAGE2ON
        sta BOTLINE,x
        sta PAGE2OFF
        jmp snnext
snmain: sta BOTLINE,x
snnext: iny
        bne snloop              ; always
sn80end: lda snst
        bmi sndone
        sta CLR80STOR
sndone: rts

dvmsg:  .text "CASSOLINK ISN'T CONNECTED"
        .byte 0

; The installer sets these, from SLTXOFF on.
sltx:   .byte 0                 ; the 6551's slot times 16
altx:   .byte 0                 ; and $30 on a //c, so sltx EOR altx is its other port
dvmax:  .byte TRIES             ; the times a call's request is sent
dvquiet: .byte 0                ; not 0: no message on the screen
dvhear: .byte 0                 ; not 0: a byte came from the PC in this call
dvid:   .byte 0, 0, 0, 0, 0     ; $FBB3, $FBC0, $FBBF, $FB1E, and 1 for a IIgs, from the installer
snst:   .byte 0
dvop:   .byte 0
dvtry:  .byte 0
dverr:  .byte 0
dvcnt:  .byte 0, 0
dvbufh: .byte 0
rqcrc:  .byte 0, 0
peor:   .byte 0
crclo:  .byte 0
crchi:  .byte 0
tmo:    .byte 0, 0, 0
pages:  .byte 0
rpsp:   .byte 0
drvend:

        else

; ---------------------------------------------------------------------------
; The installer. DRVLEN, RELOCN, SLTXOFF and SPEED are assigned ahead of this
; source by LinkProgram: the driver's length, its relocation list's count,
; the offset of sltx in it and the 6551's control byte.
; ---------------------------------------------------------------------------

DEVADR   = $BF10                ; ProDOS's driver addresses: slot * 2, drive 2's $10 on
DEVCNT   = $BF31                ; the devices listed, less 1
DEVLST   = $BF32
GETBUFR  = $BEF5                ; BASIC.SYSTEM's: A pages below HIMEM, the first's page in A
MACHID2C = $FBC0                ; 0 on a //c
COUT     = $FDED
SRC      = $06                  ; and $07: zero page Applesoft doesn't use
DST      = $08                  ; and $09

        .org $2000

inst:   ldx #2                  ; a //c's port 2
        lda MACHID2C
        beq isfound
        ldx #7
isscan: txa
        ora #$C0
        sta SRC+1
        lda #0
        sta SRC
        ldy #5
        lda (SRC),y
        cmp #$38
        bne isnext
        ldy #7
        lda (SRC),y
        cmp #$18
        bne isnext
        ldy #$0B
        lda (SRC),y
        cmp #$01
        bne isnext
        ldy #$0C
        lda (SRC),y
        cmp #$31
        beq isfound
isnext: dex
        bne isscan
        ldx #msgport-msgs
        jmp fail
isfound: stx srslot
        txa
        asl a
        asl a
        asl a
        asl a
        sta slotx
        ldy #7                  ; a slot with neither drive in the table
fsloop: cpy srslot
        beq fsnext
        tya
        asl a
        tax
        lda DEVADR,x
        cmp DEVADR
        bne fsnext
        lda DEVADR+1,x
        cmp DEVADR+1
        bne fsnext
        lda DEVADR+$10,x
        cmp DEVADR
        bne fsnext
        lda DEVADR+$11,x
        cmp DEVADR+1
        beq fsfound
fsnext: dey
        bne fsloop
        ldx #msgslot-msgs
        jmp fail
fsfound: sty dslot
        lda #5
        jsr GETBUFR
        bcc gotbuf
        ldx #msgmem-msgs
        jmp fail
gotbuf: sta bufpg
        jsr copy
        jsr reloc
        jsr crctab
        jsr setdst              ; the 6551's slot into the driver
        ldy #0
        lda slotx
        sta (DST),y
        ldx slotx
        jsr acinit
        lda MACHID2C            ; a //c's port 1 too, the driver's other port
        bne nalt
        ldy #1
        lda #$30
        sta (DST),y
        ldx #$10
        jsr acinit
nalt:
        lda dslot               ; the driver for both drives
        asl a
        tax
        lda #0
        sta DEVADR,x
        sta DEVADR+$10,x
        lda bufpg
        sta DEVADR+1,x
        sta DEVADR+$11,x
        lda dslot
        asl a
        asl a
        asl a
        asl a
        inc DEVCNT
        ldx DEVCNT
        sta DEVLST,x
        ora #$80
        inc DEVCNT
        ldx DEVCNT
        sta DEVLST,x
        ldx #msgok-msgs
        jsr print
        lda dslot
        ora #$B0
        jsr COUT
        lda #$8D
        jsr COUT
        jsr ping
        bne pingok
        ldx #msgnc-msgs
        jmp fail
pingok: rts

; ping: drive 1's status from the driver, sent once on each //c port, no
; screen message; Z set when no byte came back. ProDOS's request bytes at
; $42 to $47 are put back after.
ping:   ldx #5
pgsave: lda PDCMD,x
        sta pdsave,x
        dex
        bpl pgsave
        jsr setdst
        ldy #2
        lda #2
        sta (DST),y             ; dvmax
        iny
        sta (DST),y             ; dvquiet
        ldy #5
        lda $FBB3
        sta (DST),y             ; dvid
        iny
        lda $FBC0
        sta (DST),y
        iny
        lda $FBBF
        sta (DST),y
        iny
        lda $FB1E
        sta (DST),y
        iny
        jsr isgs
        sta (DST),y
        lda #0
        sta PDCMD
        lda dslot
        asl a
        asl a
        asl a
        asl a
        sta PDUNIT
        lda #0
        sta pgcall+1
        lda bufpg
        sta pgcall+2
        jsr pgcall
        jsr setdst
        ldy #2
        lda #TRIES
        sta (DST),y
        iny
        lda #0
        sta (DST),y
        iny
        lda (DST),y             ; dvhear
        sta pgheard
        ldx #5
pgrest: lda pdsave,x
        sta PDCMD,x
        dex
        bpl pgrest
        lda pgheard
        rts
pgcall: jmp $0000               ; the driver's entry, set above

; isgs: 1 in A on a IIgs, else 0. Only an enhanced //e's ID bytes can be a
; IIgs, whose IDROUTINE at $FE1F clears C; the //e's is an RTS.
isgs:   lda $FBB3
        cmp #$06
        bne isno
        lda $FBC0
        cmp #$E0
        bne isno
        sec
        jsr $FE1F
        bcs isno
        lda #1
        rts
isno:   lda #0
        rts

; setdst: DST at the driver's sltx in the buffer.
setdst: lda #<SLTXOFF
        sta DST
        lda bufpg
        clc
        adc #>SLTXOFF
        sta DST+1
        rts

fail:   jsr print
        lda #$8D
        jmp COUT

; print: a blank line, then the message at msgs + X, up to its 0. The blank
; line sets it apart from BASIC.SYSTEM's prompt above it.
print:  lda #$8D
        jsr COUT
prchar: lda msgs,x
        beq prdone
        ora #$80
        jsr COUT
        inx
        bne prchar
prdone: rts

; acinit: the 6551 at slot * 16 in X reset, with no interrupts, DTR on and
; SPEED's control byte.
acinit: sta ACIASTAT,x
        lda #$0B
        sta ACIACMD,x
        lda #SPEED
        sta ACIACTL,x
        rts

; copy: the driver, after the installer, to the buffer.
copy:   lda #<drvimg
        sta SRC
        lda #>drvimg
        sta SRC+1
        lda #0
        sta DST
        lda bufpg
        sta DST+1
        ldx #>DRVLEN
        ldy #0
cpage:  cpx #0
        beq cpart
cpall:  lda (SRC),y
        sta (DST),y
        iny
        bne cpall
        inc SRC+1
        inc DST+1
        dex
        jmp cpage
cpart:  cpy #<DRVLEN
        beq cpdone
        lda (SRC),y
        sta (DST),y
        iny
        jmp cpart
cpdone: rts

; reloc: each byte in the list, an offset in the driver, raised by the
; buffer's page less DRVPAGE.
reloc:  lda #<rltab
        sta SRC
        lda #>rltab
        sta SRC+1
        lda #<RELOCN
        sta rlleft
        lda #>RELOCN
        sta rlleft+1
rlnext: lda rlleft
        ora rlleft+1
        beq rldone
        ldy #0
        lda (SRC),y
        sta DST
        iny
        lda (SRC),y
        clc
        adc bufpg
        sta DST+1
        ldy #0
        lda (DST),y
        clc
        adc bufpg
        sec
        sbc #DRVPAGE
        sta (DST),y
        lda SRC
        clc
        adc #2
        sta SRC
        bcc rlcount
        inc SRC+1
rlcount: lda rlleft
        bne rldec
        dec rlleft+1
rldec:  dec rlleft
        jmp rlnext
rldone: rts

; crctab: CRC-16/XMODEM's tables, low bytes and high, in the buffer's fourth
; and fifth pages, as the program's.
crctab: lda bufpg
        clc
        adc #3
        sta SRC+1
        adc #1
        sta DST+1
        lda #0
        sta SRC
        sta DST
        ldx #0
ctbyte: stx cthi
        lda #0
        sta ctlo
        ldy #8
ctbit:  asl ctlo
        rol cthi
        bcc ctnext
        lda ctlo
        eor #$21
        sta ctlo
        lda cthi
        eor #$10
        sta cthi
ctnext: dey
        bne ctbit
        txa
        tay
        lda ctlo
        sta (SRC),y
        lda cthi
        sta (DST),y
        inx
        bne ctbyte
        rts

srslot: .byte 0
slotx:  .byte 0
dslot:  .byte 0
bufpg:  .byte 0
rlleft: .byte 0, 0
ctlo:   .byte 0
cthi:   .byte 0
pdsave: .byte 0, 0, 0, 0, 0, 0
pgheard: .byte 0

msgs:
msgok:   .text "CASSOLINK DRIVES IN SLOT "
        .byte 0
msgport: .text "CASSOLINK: NO SERIAL PORT"
        .byte 0
msgslot: .text "CASSOLINK: NO FREE SLOT"
        .byte 0
msgmem:  .text "CASSOLINK: NO MEMORY"
        .byte 0
msgnc:   .text "CASSOLINK ISN'T CONNECTED"
        .byte 0

drvimg:
rltab    = drvimg + DRVLEN

        endif

; ---------------------------------------------------------------------------
; Casso's CassoLink: the virtual drives' block driver and its installer
; (R44, FR-200).
;
; The CassoLink ProDOS disk's STARTUP runs CASSOLINK, this installer and the
; driver after it, from $2000. BASIC.SYSTEM has started by then, so its
; GETBUFR allocates four pages below HIMEM for the driver: the driver in the first
; two and its CRC tables in the others. The driver is assembled at DRVORG
; = $9200, and each byte that has its high byte is in the relocation list
; after it, which the PC makes by assembling it at two pages; the installer
; adds GETBUFR's page less DRVPAGE to each.
;
; The serial port is the //c's port 2, or the first slot from 7 down with
; the Super Serial Card's identification bytes, set to SPEED's 6551 control
; byte, as the connection's speed. The drives are drive 1 and 2 of the first
; slot from 7 down, other than the serial port's, whose two entries in
; ProDOS's device table are both the no-device entry, slot 0's.
;
; Each call sends Virtual block (CMDVBLOCK) with the drive, the operation,
; the block and for a write the block's bytes and their CRC, and waits about
; three seconds for the PC's reply: ProDOS's error, then for a status the
; drive's blocks and for a read the block and its CRC. A reply that doesn't
; arrive or arrives damaged is requested again, 4 times in all, then the
; call returns ProDOS's I/O error. Interrupts are off for the call. The
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

DRVPAGE  = $92

        ifdef DRIVER

        .org DRVORG

CRCLO    = DRVORG + $200
CRCHI    = DRVORG + $300

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
dvtries: lda #TRIES
        sta dvtry
dvagain: jsr request
        jsr reply
        bcc dvend
        jsr drain
        dec dvtry
        bne dvagain
        lda #ERRIO
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

sltx:   .byte 0                 ; the installer's: the 6551's slot times 16
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
        lda #4
        jsr GETBUFR
        bcc gotbuf
        ldx #msgmem-msgs
        jmp fail
gotbuf: sta bufpg
        jsr copy
        jsr reloc
        jsr crctab
        lda #0                  ; the 6551's slot into the driver
        sta DST
        lda bufpg
        clc
        adc #>SLTXOFF
        sta DST+1
        ldy #<SLTXOFF
        lda slotx
        sta (DST),y
        ldx slotx               ; the 6551 reset, no interrupts, DTR on
        sta ACIASTAT,x
        lda #$0B
        sta ACIACMD,x
        lda #SPEED
        sta ACIACTL,x
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
        jmp COUT

fail:   jsr print
        lda #$8D
        jmp COUT

; print: the message at msgs + X, up to its 0.
print:  lda msgs,x
        beq prdone
        ora #$80
        jsr COUT
        inx
        bne print
prdone: rts

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

; crctab: CRC-16/XMODEM's tables, low bytes and high, in the buffer's third
; and fourth pages, as the program's.
crctab: lda bufpg
        clc
        adc #2
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

msgs:
msgok:   .text "CASSOLINK DRIVES IN SLOT "
        .byte 0
msgport: .text "CASSOLINK: NO SERIAL PORT"
        .byte 0
msgslot: .text "CASSOLINK: NO FREE SLOT"
        .byte 0
msgmem:  .text "CASSOLINK: NO MEMORY"
        .byte 0

drvimg:
rltab    = drvimg + DRVLEN

        endif

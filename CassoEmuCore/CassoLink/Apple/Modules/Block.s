; ---------------------------------------------------------------------------
; Block: a module of the link program. ProDOS block devices and SmartPort
; units, read and written through their slot firmware (R41).
;
; A slot's firmware is a block device's when it has $20, $00 and $03 at
; $Cn01, $Cn03 and $Cn05 and isn't a Disk II's, whose byte at $CnFF is $00
; ($FF for 13 sectors). Its ProDOS entry is $Cn00 plus the byte at $CnFF. One
; with $00 at $Cn07 is a SmartPort, whose dispatch is three bytes past the
; ProDOS entry and whose STATUS lists its units.
;
; List block devices (CMDBLKLIST): no payload. Reply: Block devices
; (CMDBLKDEVS), a count, then per unit: its slot; its unit, a SmartPort
; unit's number or a ProDOS drive, 1 or 2; 1 for a SmartPort unit; its
; blocks, three bytes, low first; its kind, a SmartPort unit's type or $FF;
; and its volume name, 16 bytes as block 2's volume header has them, the
; first the name's length, or zeros when block 2 isn't a volume header.
;
; Read blocks (CMDBLKREAD): the slot, the unit, the SmartPort flag, the first
; block (three bytes), the count, the verify flag, and the percentages at the
; request's start and end. Each block is read, and with verify read again and
; compared, then sent in a Block data packet (CMDBLKDATA): its number (three
; bytes), its 512 bytes and their CRC-16/XMODEM, low byte first. Then Status:
; the drive is the slot, the track the unit, the count the blocks done, and
; the error the firmware's ($27 I/O error, $28 no device, $2B write
; protected, $2F offline) or ERRVERIFY.
;
; Write blocks (CMDBLKWRITE): as Read blocks. For each block an Acknowledge
; with its index in the request is the request for it, and the host's Block data
; packet is checked by its CRC, with a NAK for another copy, up to 8 times;
; ERRLINK after that. The block is written, and with verify read back and
; compared. Then Status.
;
; A firmware call saves the zero page and restores it after, as a card's
; firmware may use any of it; $42 to $47 have a ProDOS driver's request.
; ---------------------------------------------------------------------------

; CMDBLKLIST and the other commands are assigned ahead of this source by
; LinkProgram, from CassoLinkCodec::kCommands.
BLKBUF     = $5C00              ; a block
VFYBUF     = $5E00              ; and the same block read again
ZPBUF      = $6000              ; the zero page during a firmware call
DEVTAB     = $6100              ; each unit's entry in List's reply
STBUF      = $6400              ; a SmartPort STATUS's reply
ENTRYLEN   = 23                 ; an entry: slot, unit, SmartPort, blocks (3), kind, name (16)
MAXUNITS   = 32
TRIES      = 8

        .org $5000
        .byte 'C', 'M', 2, 1    ; block, version 1
        .word PROGVER
        .word modend - $5000
        .word 0                 ; the CRC, which the PC fills in
        .byte 3
        .byte CMDBLKLIST
        .word blist
        .byte CMDBLKREAD
        .word bread
        .byte CMDBLKWRITE
        .word bwrite

; Zero page the program doesn't use, past the ProDOS driver's request.
bslot    = $50                  ; the slot
bunit    = $51                  ; the unit
bsp      = $52                  ; nonzero for a SmartPort unit
bblk     = $53                  ; and $54, $55: the block
bvfy     = $56                  ; nonzero to verify
bptr     = $57                  ; and $58: the block's buffer
bdev     = $59                  ; and $5A: the next entry in DEVTAB
bn       = $5B                  ; the units listed
bu       = $5C                  ; a SmartPort's unit being listed
bus      = $5D                  ; and its count of units
btry     = $5E                  ; the tries left for a block
bdrv     = $5F                  ; a ProDOS drive being listed
bcnt     = $60                  ; and $61, $62: a unit's blocks
bkind    = $63                  ; and its kind
blen     = $64                  ; and $65: a reply's length, or a block's CRC

; ---------------------------------------------------------------------------
; List block devices
; ---------------------------------------------------------------------------

blist:  lda #0
        sta bn
        lda #<DEVTAB
        sta bdev
        lda #>DEVTAB
        sta bdev+1
        lda #1
        sta bslot
blslot: jsr isblock
        bcs blnext
        pha
        jsr entry
        pla
        bne blpd
        jsr listsp
        jmp blnext
blpd:   jsr listpd
blnext: inc bslot
        lda bslot
        cmp #8
        bne blslot
        jsr senddevs
        jmp p_main

; listsp: each block unit of bslot's SmartPort, from STATUS of unit 0 for
; the count and of each unit for its device information block.
listsp: lda #1
        sta bsp
        lda #0
        sta bunit
        jsr spstatus
        bcs lsdone
        lda STBUF
        sta bus
        lda #1
        sta bu
lsunit: lda bu
        cmp bus
        beq lsone
        bcs lsdone
lsone:  lda bu
        sta bunit
        lda #3                  ; the device information block
        jsr spstat
        bcs lsnext
        lda STBUF               ; a block device's status has bit 7 set
        bpl lsnext
        lda STBUF+1
        sta bcnt
        lda STBUF+2
        sta bcnt+1
        lda STBUF+3
        sta bcnt+2
        lda STBUF+21            ; its type
        sta bkind
        jsr addunit
        jsr addname
lsnext: inc bu
        lda bn
        cmp #MAXUNITS
        bne lsunit
lsdone: rts

; listpd: drives 1 and 2 of bslot's ProDOS driver, each that STATUS finds,
; with the block count STATUS has in X and Y.
listpd: lda #0
        sta bsp
        sta bblk
        sta bblk+1
        sta bblk+2
        lda #1
        sta bdrv
lpdrv:  lda bdrv
        sta bunit
        lda #<BLKBUF
        sta bptr
        lda #>BLKBUF
        sta bptr+1
        lda #0                  ; STATUS
        jsr pdcall
        bcs lpnext
        stx bcnt
        sty bcnt+1
        lda #0
        sta bcnt+2
        lda #$FF
        sta bkind
        jsr addunit
        jsr addname
lpnext: inc bdrv
        lda bdrv
        cmp #3
        beq lpdone
        lda bn
        cmp #MAXUNITS
        bne lpdrv
lpdone: rts

; addunit: the entry at bdev of bslot's bunit, with bsp, bcnt and bkind;
; bn counted. The name is addname's.
addunit: ldy #0
        lda bslot
        sta (bdev),y
        iny
        lda bunit
        sta (bdev),y
        iny
        lda bsp
        sta (bdev),y
        iny
        lda bcnt
        sta (bdev),y
        iny
        lda bcnt+1
        sta (bdev),y
        iny
        lda bcnt+2
        sta (bdev),y
        iny
        lda bkind
        sta (bdev),y
        inc bn
        rts

; addname: block 2's volume name into the entry at bdev, or zeros when the
; block can't be read or isn't a volume header; then bdev past the entry.
addname: lda #2
        sta bblk
        lda #0
        sta bblk+1
        sta bblk+2
        lda #<BLKBUF
        sta bptr
        lda #>BLKBUF
        sta bptr+1
        lda #1
        jsr devio
        ldy #7
        bcs anzero
        lda BLKBUF+4            ; storage type $F: a volume header
        and #$F0
        cmp #$F0
        bne anzero
        lda BLKBUF+4
        and #$0F
        sta (bdev),y
        ldx #0
ancopy: iny
        lda BLKBUF+5,x
        sta (bdev),y
        inx
        cpx #15
        bne ancopy
        jmp annext
anzero: lda #0
anz:    sta (bdev),y
        iny
        cpy #ENTRYLEN
        bne anz
annext: lda bdev
        clc
        adc #ENTRYLEN
        sta bdev
        bcc andone
        inc bdev+1
andone: rts

; senddevs: Block devices with bn entries from DEVTAB.
senddevs: lda #0
        sta blen
        sta blen+1
        ldx bn
        beq sdlen
sdmul:  lda blen
        clc
        adc #ENTRYLEN
        sta blen
        bcc sdnext
        inc blen+1
sdnext: dex
        bne sdmul
sdlen:  inc blen                ; and the count
        bne sdhdr
        inc blen+1
sdhdr:  lda #$C1
        jsr p_putb
        lda blen
        jsr p_putb
        lda blen+1
        jsr p_putb
        lda #CMDBLKDEVS
        jsr p_putb
        lda #$C1 ^ CMDBLKDEVS
        eor blen
        eor blen+1
        jsr p_putb
        lda #0
        sta p_peor
        lda bn
        jsr p_putpe
        lda #<DEVTAB
        sta bdev
        lda #>DEVTAB
        sta bdev+1
        ldx bn
        beq sdend
sdunit: ldy #0
sdbyte: lda (bdev),y
        jsr p_putpe
        iny
        cpy #ENTRYLEN
        bne sdbyte
        lda bdev
        clc
        adc #ENTRYLEN
        sta bdev
        bcc sdunext
        inc bdev+1
sdunext: dex
        bne sdunit
sdend:  lda p_peor
        jmp p_putb

; ---------------------------------------------------------------------------
; Read and write blocks
; ---------------------------------------------------------------------------

bread:  lda #p_OPREAD
        jsr bargs
        bcs brdone
brloop: lda p_index
        cmp p_count
        beq brdone
        lda #<BLKBUF
        sta bptr
        lda #>BLKBUF
        sta bptr+1
        lda #1
        jsr devio
        bcs brfail
        lda bvfy
        beq brsend
        jsr reread
        bcs brfail
brsend: jsr sendblk
        jsr p_pollcancel
        jsr nextblk
        jmp brloop
brfail: sta p_err
brdone: jmp p_opdone

bwrite: lda #p_OPWRITE
        jsr bargs
        bcs bwdone
bwloop: lda p_index
        cmp p_count
        beq bwdone
        lda #TRIES
        sta btry
        lda #p_ACK
bwask:  ldx p_index
        ldy #0
        jsr p_sendack
        jsr getblk
        bcc bwgot
        jsr p_drain
        dec btry
        beq bwlink
        lda #p_NAK
        jmp bwask
bwlink: lda #p_ERRLINK
        bne bwfail              ; always
bwgot:  lda #<BLKBUF
        sta bptr
        lda #>BLKBUF
        sta bptr+1
        lda #2
        jsr devio
        bcs bwfail
        lda bvfy
        beq bwnext
        jsr reread
        bcs bwfail
bwnext: jsr nextblk
        jmp bwloop
bwfail: sta p_err
bwdone: jmp p_opdone

; bargs: the request's fields, for the operation in A; C set, with
; ERRDRIVE, when the slot's firmware isn't a block device's. Errors aren't
; drawn on the Apple, as the PC reports them in words.
bargs:  sta p_op
        lda #0
        sta p_err
        sta p_index
        sta p_wpbit
        sta p_misses
        lda #1
        sta p_quiet
        lda p_PKTBUF
        sta bslot
        sta p_drive
        lda p_PKTBUF+1
        sta bunit
        sta p_track
        lda p_PKTBUF+2
        sta bsp
        lda p_PKTBUF+3
        sta bblk
        lda p_PKTBUF+4
        sta bblk+1
        lda p_PKTBUF+5
        sta bblk+2
        lda p_PKTBUF+6
        sta p_count
        lda p_PKTBUF+7
        sta bvfy
        lda p_PKTBUF+8
        sta p_plo
        lda p_PKTBUF+9
        sta p_phi
        jsr p_showop
        lda bslot
        beq badrive
        cmp #8
        bcs badrive
        jsr isblock
        bcs badrive
        jsr entry
        clc
        rts
badrive: lda #p_ERRDRIVE
        sta p_err
        sec
        rts

; nextblk: the next block, and the request's progress drawn.
nextblk: inc bblk
        bne nbcount
        inc bblk+1
        bne nbcount
        inc bblk+2
nbcount: inc p_index
        jmp p_showprog

; reread: the block read again into VFYBUF and compared with BLKBUF; the
; C set with the firmware's error or ERRVERIFY in A.
reread: lda #<VFYBUF
        sta bptr
        lda #>VFYBUF
        sta bptr+1
        lda #1
        jsr devio
        bcs rrdone
        ldy #0
rrlo:   lda BLKBUF,y
        cmp VFYBUF,y
        bne rrbad
        iny
        bne rrlo
rrhi:   lda BLKBUF+256,y
        cmp VFYBUF+256,y
        bne rrbad
        iny
        bne rrhi
        clc
rrdone: rts
rrbad:  lda #p_ERRVERIFY
        sec
        rts

; sendblk: Block data with bblk and BLKBUF's 512 bytes and their CRC.
sendblk: ldx #0
sbhdr:  lda blkhdr,x
        jsr p_putb
        inx
        cpx #5
        bne sbhdr
        lda #0
        sta p_peor
        sta p_crclo
        sta p_crchi
        lda bblk
        jsr p_putpe
        lda bblk+1
        jsr p_putpe
        lda bblk+2
        jsr p_putpe
        ldy #0
sblo:   lda BLKBUF,y
        jsr p_crcupd
        lda BLKBUF,y
        jsr p_putpe
        iny
        bne sblo
sbhi:   lda BLKBUF+256,y
        jsr p_crcupd
        lda BLKBUF+256,y
        jsr p_putpe
        iny
        bne sbhi
        lda p_crclo
        jsr p_putpe
        lda p_crchi
        jsr p_putpe
        lda p_peor
        jmp p_putb

blkhdr: .byte $C1, <517, >517, CMDBLKDATA, $C1 ^ <517 ^ >517 ^ CMDBLKDATA

; getblk: the host's Block data for bblk into BLKBUF. C is set on a
; timeout, a packet that isn't Block data, another block or a failed CRC. The
; bytes are stored as they arrive and checked after, as at the line's speed there are too
; few cycles a byte for the CRC as well.
getblk: jsr p_getenvt
        bcs gbfail
        cmp #CMDCANCEL
        bne gbcmd
        jmp p_cancel
gbcmd:  cmp #CMDBLKDATA
        bne gbfail
        ldx #0
gbnum:  jsr p_getbtq
        bcs gbfail
        cmp bblk,x
        bne gbfail
        inx
        cpx #3
        bne gbnum
        ldy #0
gblo:   jsr p_getbtq
        bcs gbfail
        sta BLKBUF,y
        iny
        bne gblo
gbhi:   jsr p_getbtq
        bcs gbfail
        sta BLKBUF+256,y
        iny
        bne gbhi
        jsr p_getbtq
        bcs gbfail
        sta blen
        jsr p_getbtq
        bcs gbfail
        sta blen+1
        jsr p_getbtq            ; the payload's EOR; the CRC checks the block
        bcs gbfail
        lda #0
        sta p_crclo
        sta p_crchi
        ldy #0
gbclo:  lda BLKBUF,y
        jsr p_crcupd
        iny
        bne gbclo
gbchi:  lda BLKBUF+256,y
        jsr p_crcupd
        iny
        bne gbchi
        lda p_crclo
        cmp blen
        bne gbfail
        lda p_crchi
        cmp blen+1
        bne gbfail
        clc
        rts
gbfail: sec
        rts

; ---------------------------------------------------------------------------
; Firmware
; ---------------------------------------------------------------------------

; isblock: C clear when bslot's firmware is a block device's, with A
; 0 for a SmartPort and nonzero for a ProDOS driver alone.
isblock: lda bslot
        ora #$C0
        sta bptr+1
        lda #0
        sta bptr
        ldy #1
        lda (bptr),y
        cmp #$20
        bne ibno
        ldy #3
        lda (bptr),y
        bne ibno
        ldy #5
        lda (bptr),y
        cmp #$03
        bne ibno
        ldy #$FF
        lda (bptr),y
        beq ibno                ; a Disk II
        cmp #$FF
        beq ibno                ; a 13-sector Disk II
        ldy #7
        lda (bptr),y
        beq ibsp
        lda #1
ibsp:   clc
        rts
ibno:   sec
        rts

; entry: pdjmp at bslot's ProDOS entry, $Cn00 plus the byte at $CnFF, and
; spjsr at its SmartPort dispatch, three bytes past it.
entry:  lda bslot
        ora #$C0
        sta bptr+1
        sta pdjmp+2
        sta spjsr+2
        lda #$FF
        sta bptr
        ldy #0
        lda (bptr),y
        sta pdjmp+1
        clc
        adc #3
        sta spjsr+1
        rts

; devio: block bblk of the unit read (A = 1) or written (A = 2) at bptr,
; through its SmartPort or its ProDOS driver, whose commands have the same
; numbers. C is set with the error in A.
devio:  ldx bsp
        bne dvsp
        jmp pdcall
dvsp:   sta spcmd
        lda #3
        sta splist
        lda bunit
        sta splist+1
        lda bptr
        sta splist+2
        lda bptr+1
        sta splist+3
        lda bblk
        sta splist+4
        lda bblk+1
        sta splist+5
        lda bblk+2
        sta splist+6
        lda spcmd
        jmp spcall

; spstatus: STATUS of bunit's count of units, code 0, into STBUF; spstat:
; STATUS with the code in A. C is set with the error in A.
spstatus: lda #0
spstat: sta splist+4
        lda #3
        sta splist
        lda bunit
        sta splist+1
        lda #<STBUF
        sta splist+2
        lda #>STBUF
        sta splist+3
        lda #0
        jmp spcall

; pdcall: the ProDOS driver's command A for bunit's drive, block bblk, at
; bptr. C is set with the error in A; after STATUS, X and Y have
; the block count, low byte first.
pdcall: sta fwcmd
        jsr zpsave
        lda fwcmd
        sta $42
        lda bslot               ; the unit: drive 2 in bit 7, the slot in bits 4 to 6
        asl a
        asl a
        asl a
        asl a
        ldx bunit
        cpx #2
        bne pdunit
        ora #$80
pdunit: sta $43
        lda bptr
        sta $44
        lda bptr+1
        sta $45
        lda bblk
        sta $46
        lda bblk+1
        sta $47
        php
        sei
        jsr pdjmp
        jmp fwdone
pdjmp:  jmp $C000               ; patched: the ProDOS entry

; spcall: the SmartPort command A with splist.
spcall: sta spcmd
        jsr zpsave
        php
        sei
spjsr:  jsr $C000               ; patched: the SmartPort dispatch
spcmd:  .byte 0
        .word splist
; fwdone: a call's results saved, the zero page put back, and the interrupt
; flag restored; C is the call's, with its error in A and X and Y.
fwdone: sta fwres
        stx fwx
        sty fwy
        php
        pla
        sta fwflags
        jsr zprest
        plp
        lda fwflags
        lsr a                   ; the call's carry
        lda fwres
        ldx fwx
        ldy fwy
        rts

; zpsave and zprest: the zero page into ZPBUF and back.
zpsave: ldx #0
zsloop: lda $00,x
        sta ZPBUF,x
        inx
        bne zsloop
        rts
zprest: ldx #0
zrloop: lda ZPBUF,x
        sta $00,x
        inx
        bne zrloop
        rts

fwcmd:   .byte 0
fwres:   .byte 0
fwx:     .byte 0
fwy:     .byte 0
fwflags: .byte 0
splist:  .byte 3, 0, 0, 0, 0, 0, 0

modend:

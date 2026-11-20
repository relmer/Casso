; Casso's CassoLink for the Apple ///: SOS.INTERP.
;
; SOS loads this program from the CassoLink SOS disk's SOS.INTERP, at SOSORG
; in the highest bank, and runs it. It talks to the PC through the .RS232
; driver and reads and writes the Apple ///'s drives through SOS, with the
; calls Apple's SOS Reference Manual documents: OPEN, READ, WRITE, CLOSE,
; GET_DEV_NUM, D_INFO, D_STATUS and D_CONTROL, and TERMINATE; and D_READ and
; D_WRITE, device calls $80 and $81, from Rick Sidwell's notes on SOS 1.3.
;
; The framing, the Ready packet and the Status packet are the Apple II
; program's (Program.s), and the block requests are the block module's
; (Block.s), so the PC side is the same for both:
;
;   Ready           the version, SOSFLAG in the shim version's byte, no
;                   Disk II drives, five zero bytes for the ROM's
;                   identification, and the CRC of the program up to progend
;   Idle            Ready again after 100 ms
;   Set speed       a 6551 control byte: ACK, then .RS232 at its rate; NAK
;                   for the external clock's rate, which the driver can't set
;   Label, Log      a line on the console, and Ready again
;   Load program    Ready again: the program is on the disk
;   Exit            .RS232 closed, and TERMINATE
;   Load module     the block module's id and version: the block requests
;                   are in this program, so no module is loaded
;
; The block requests, as Block.s documents them, with three differences: a
; device's slot byte is its SOS device number and its unit SOS's unit
; number; each entry of Block devices has the device's name after the
; volume's, 16 bytes, the first its length; and the errors are SOS's ($27
; I/O error, $28 no drive, $2B write protected, $2D block too large, $2E
; disk switched), which are the firmware's numbers for the same errors.
; The Status for a Disk II request has ERRDRIVE, as the program has no Disk
; II drives.
;
; .RS232 is opened with 8 data bits, no parity, no protocol and no hardware
; handshake, at the rate in the byte after progend, which the CassoLink SOS
; disk sets from the link's speed. Immediate read is on, so READ returns the
; bytes the driver has received, at most its buffer's 255, without waiting.
;
; The interpreter's zero page is SOS's page $1A, whose X-bytes select another
; bank for an indirect address; this program doesn't use indirect
; addressing, so the X-bytes are left alone. Its buffers follow its code, inside the bytes
; SOS.INTERP loads, below $A000, so each SOS call's buffer is in the current
; bank.
;
; NMOS 6502 code throughout.

        ifndef PROGVER
PROGVER = 1
        endif
        ifndef SOSFLAG
SOSFLAG = $20
        endif
        ifndef BLKMODID
BLKMODID = 2
        endif
        ifndef BLKMODVER
BLKMODVER = 1
        endif

; SOS calls.
SOSDREAD   = $80
SOSDWRITE  = $81
SOSDSTATUS = $82
SOSDCTL    = $83
SOSDEVNUM  = $84
SOSDINFO   = $85
SOSOPEN    = $C8
SOSREAD    = $CA
SOSWRITE   = $CB
SOSCLOSE   = $CC
SOSTERM    = $65

; SOS's errors this program tests for.
SOSDISKSW  = $2E                ; the disk was switched; the call is made again

; .RS232's requests and its control parameters, numbered from 1 as Apple's
; Standard Device Drivers Manual numbers them; the list's first byte is its
; length, so parameter N is at offset N.
RSCONTROL  = 1                  ; status and control: the 15 parameters
RSNEWLINE  = 2                  ; control: newline mode and character
RSBUFFERS  = 3                  ; status: the buffers' sizes and counts
RSPARMS    = $0F
RSBAUD     = 1
RSFORMAT   = 2
RSPROTOCOL = 6
RSHANDSHK  = 12
RSIMREAD   = 13
RS8N1      = $00                ; 8 data bits, no parity
RSON       = $80

ACK      = $06
NAK      = $15
CR       = $0D
LF       = $0A

; Operations and errors, as the Status packet reports them.
OPIDLE   = 0
OPREAD   = 1
OPWRITE  = 2
ERRNONE  = $00
ERRVERIFY = $40
ERRLINK  = $50
ERRDRIVE = $60
ERRBLOCK = $2D                  ; SOS's error for a block past the device's end

DEVMAX   = $18                  ; SOS's highest device number
ENTRYLEN = 39                   ; slot, unit, SmartPort, blocks (3), kind, volume (16), device (16)
ATTEMPTS = 8
RXMAX    = 255                  ; .RS232's input buffer
TXMAX    = 240                  ; bytes put before they are written
POLLS    = 1000                 ; polls in a second, a poll READ and half a millisecond

; Buffers, after the code, page-aligned so indexed loads never cross a page.
; SOS.INTERP's code length covers them, so SOS loads zeros over them.
SOSORG   = $8000
BUFBASE  = $9000
PKTBUF   = BUFBASE              ; a request's payload
RXBUF    = BUFBASE + $100       ; the bytes READ returned
TXBUF    = BUFBASE + $200       ; the bytes put and not yet written
CRCLO    = BUFBASE + $300       ; CRC-16/XMODEM table
CRCHI    = BUFBASE + $400
BLKBUF   = BUFBASE + $500       ; a block
VFYBUF   = BUFBASE + $700       ; and the same block read again
LINEBUF  = BUFBASE + $900       ; a console line
DEVTAB   = BUFBASE + $A00       ; each device's entry in List's reply, to $9DA7
BUFEND   = BUFBASE + $E00

; Zero page.
stack    = $10                  ; S at entry, for a Cancel
op       = $11
drive    = $12                  ; the SOS device number
track    = $13                  ; and its unit
index    = $14                  ; the blocks done
count    = $15                  ; the blocks requested
err      = $16
crclo    = $17
crchi    = $18
peor     = $19                  ; the EOR of a payload
envcmd   = $1A
lenlo    = $1B
lenhi    = $1C
tmp      = $1D
tmp2     = $1E
tmp3     = $1F
rxn      = $20                  ; bytes in RXBUF
rxpos    = $21                  ; and the next one to take
txn      = $22                  ; bytes in TXBUF
tmolen   = $23                  ; a wait's seconds
pollo    = $24                  ; and its polls left in the second
polhi    = $25
savex    = $26
savey    = $27
bblk     = $28                  ; and $29, $2A: the block
bvfy     = $2B                  ; nonzero to verify
bn       = $2C                  ; devices listed
btry     = $2D                  ; attempts left for a block
bdev     = $2E                  ; the device being listed
linen    = $2F                  ; the console line's length
linex    = $30                  ; lineput: X as it came

        .org SOSORG

entry:  cld
        tsx
        stx stack
        lda #0
        sta rxn
        sta rxpos
        sta txn
        sta op
        jsr mktables
        jsr selfcrc
        jsr opencons
        ldx #msgtitle-msgs
        jsr showmsg
        jsr openrs
        bcc rsok
        ldx #msgnors-msgs
        jsr showmsg
halt:   jmp halt                ; Control-Reset starts the Apple /// again
rsok:   ldx #msgwait-msgs
        jsr showmsg

main:   lda #OPIDLE
        sta op
        jsr sendready
        lda #1                  ; a second for the reply
        sta tmolen
        jsr getenvt
        lda #3                  ; and three for each byte of an operation
        sta tmolen
        bcs main
        jsr getpayload
        bcs main
        lda envcmd
        cmp #CMDIDLE
        bne notidle
        lda #100
        jsr delayms
        jmp main
notidle: cmp #CMDSPEED
        bne notspeed
        jmp dospeed
notspeed: cmp #CMDLABEL
        bne notlabel
        jmp dolabel
notlabel: cmp #CMDLOG
        bne notlog
        jmp dolog
notlog: cmp #CMDEXIT
        bne notexit
        jmp doexit
notexit: cmp #CMDMODULE
        bne notmodule
        jmp domodule
notmodule: cmp #CMDBLKLIST
        bne notlist
        jmp blist
notlist: cmp #CMDBLKREAD
        bne notbread
        jmp bread
notbread: cmp #CMDBLKWRITE
        bne notbwrite
        jmp bwrite
notbwrite: ldx #nodrvend-nodrv-1
ndfind: cmp nodrv,x
        beq ndstat
        dex
        bpl ndfind
        jmp main                ; Load program, Cancel and the screen's requests
ndstat: lda #OPREAD
        sta op
        lda PKTBUF
        sta drive
        lda #0
        sta track
        sta index
        lda #ERRDRIVE
        sta err
        jmp opdone

; The Disk II's requests, each a request for a drive this program doesn't
; have.
nodrv:  .byte CMDREAD, CMDWRITE, CMDFORMAT, CMDWTRACK, CMDTIME, CMDMEASURE
        .byte CMDREADCRC, CMDKRUNCH, CMDCAPTURE, CMDWNIBS, CMDRD13
nodrvend:

; cancel: the operation stops where it is, and the loop starts again.
cancel: ldx stack
        txs
        jmp main

; opdone: Status, and the loop again.
opdone: jsr sendstatus
        jmp main

; ---------------------------------------------------------------------------
; Requests
; ---------------------------------------------------------------------------

; Set speed: ACK at the old rate, the output written out, then .RS232 at the
; new rate. The control byte's low four bits are the 6551's rate, which is
; the driver's baud rate parameter; 0, the external clock, is NAKed.
dospeed: lda PKTBUF
        and #$0F
        bne spdok
        lda #NAK
        ldx #0
        ldy #0
        jsr sendack
        jsr flush
        jmp main
spdok:  sta dcb+RSBAUD
        lda #ACK
        ldx #0
        ldy #0
        jsr sendack
        jsr flush
spdwait: jsr rsstatus3          ; the output buffer emptied
        bcs spdset
        lda rsbufs+2
        ora rsbufs+3
        bne spdwait
spdset: lda #12                 ; the last byte's bits on the line, at as
        jsr delayms             ; slow as 1,200 baud
        jsr rsconfig
        jmp main

; Label: the text on a line by itself.
dolabel: lda #0
        sta linen
        ldy #0
lbcopy: cpy lenlo
        beq lbsay
        lda PKTBUF,y
        jsr lineput
        iny
        jmp lbcopy
lbsay:  jsr lineend
        jmp main

; Log: the PC's hour and minute as HH:MM, then the text.
dolog:  lda #0
        sta linen
        lda PKTBUF
        jsr linedec
        lda #':'
        jsr lineput
        lda PKTBUF+1
        jsr linedec
        lda #' '
        jsr lineput
        ldy #2
lgcopy: cpy lenlo
        bcs lgsay
        lda PKTBUF,y
        jsr lineput
        iny
        jmp lgcopy
lgsay:  jsr lineend
        jmp main

; Exit: the output written, .RS232 closed, and the interpreter ended.
doexit: jsr flush
        lda rsref
        sta closelst+1
        .byte $00, SOSCLOSE
        .word closelst
termlst: .byte $00, SOSTERM      ; its parameter list is the call itself,
        .word termlst           ; whose first byte, 0, is the count
        jmp halt

; Load module: with no payload or the check, the block module's id and
; version, as its requests are this program's; a chunk is NAKed, as nothing
; is loaded.
domodule: lda lenlo
        beq dmsend
        cmp #2
        bne dmbad
        lda PKTBUF
        and PKTBUF+1
        cmp #$FF
        beq dmsend
dmbad:  lda #NAK
        ldx PKTBUF
        ldy PKTBUF+1
        jsr sendack
        jmp main
dmsend: ldx #0
dmhdr:  lda modhdr,x
        jsr putb
        inx
        cpx #5
        bne dmhdr
        lda #BLKMODID
        jsr putb
        lda #BLKMODVER
        jsr putb
        lda #BLKMODID ^ BLKMODVER
        jsr putb
        jmp main

modhdr: .byte $C1, $02, $00, CMDMODULE, $C1 ^ $02 ^ $00 ^ CMDMODULE

; ---------------------------------------------------------------------------
; Block devices
; ---------------------------------------------------------------------------

; List block devices: each device D_INFO finds with bit 7 of its type set,
; with its blocks, block 2's volume name and the device's name.
blist:  lda #0
        sta bn
        lda #<DEVTAB
        sta adput+1
        lda #>DEVTAB
        sta adput+2
        lda #1
        sta bdev
bldev:  lda bdev
        sta infolst+1
        .byte $00, SOSDINFO
        .word infolst
        bne blnext
        lda devinfo+2           ; a block device's type has bit 7 set
        bpl blnext
        lda bdev
        jsr addbyte
        lda devinfo+1           ; its unit
        jsr addbyte
        lda #0                  ; not a SmartPort
        jsr addbyte
        lda devinfo+5           ; its blocks
        jsr addbyte
        lda devinfo+6
        jsr addbyte
        lda #0
        jsr addbyte
        lda #$FF                ; no SmartPort type
        jsr addbyte
        jsr addvol
        ldx #0
bldname: lda devname,x
        jsr addbyte
        inx
        cpx #16
        bne bldname
        inc bn
blnext: inc bdev
        lda bdev
        cmp #DEVMAX+1
        bne bldev
        jsr senddevs
        jmp main

; addbyte: A into the next byte of DEVTAB.
addbyte:
adput:  sta DEVTAB
        inc adput+1
        bne abdone
        inc adput+2
abdone: rts

; addvol: block 2's volume name, 16 bytes with its length first, or zeros
; when the block can't be read or isn't a volume header.
addvol: lda bdev
        sta drive
        lda #2
        sta bblk
        lda #0
        sta bblk+1
        jsr readblk
        bne avzero
        lda BLKBUF+4            ; storage type $F: a volume header
        and #$F0
        cmp #$F0
        bne avzero
        lda BLKBUF+4
        and #$0F
        jsr addbyte
        ldx #0
avcopy: lda BLKBUF+5,x
        jsr addbyte
        inx
        cpx #15
        bne avcopy
        rts
avzero: ldx #16
avz:    lda #0
        jsr addbyte
        dex
        bne avz
        rts

; senddevs: Block devices with bn entries from DEVTAB.
senddevs: lda #1                ; the count, then the entries
        sta tmp
        lda #0
        sta tmp2
        ldx bn
        beq sdhdr
sdmul:  lda tmp
        clc
        adc #ENTRYLEN
        sta tmp
        bcc sdnext
        inc tmp2
sdnext: dex
        bne sdmul
sdhdr:  lda #$C1
        jsr putb
        lda tmp
        jsr putb
        lda tmp2
        jsr putb
        lda #CMDBLKDEVS
        jsr putb
        lda #$C1 ^ CMDBLKDEVS
        eor tmp
        eor tmp2
        jsr putb
        lda #0
        sta peor
        lda bn
        jsr putpe
        lda #<DEVTAB
        sta sdget+1
        lda #>DEVTAB
        sta sdget+2
        lda tmp                 ; the entries' bytes: the length less the count
        sec
        sbc #1
        sta tmp
        bcs sdloop
        dec tmp2
sdloop: lda tmp
        ora tmp2
        beq sdend
sdget:  lda DEVTAB
        jsr putpe
        inc sdget+1
        bne sddec
        inc sdget+2
sddec:  lda tmp
        bne sddec2
        dec tmp2
sddec2: dec tmp
        jmp sdloop
sdend:  lda peor
        jmp putb

; Read blocks: each block read, and with verify read again and compared,
; then sent in a Block data packet. Then Status.
bread:  lda #OPREAD
        jsr bargs
        bcs brdone
brloop: lda index
        cmp count
        beq brdone
        jsr readblk
        bne brfail
        lda bvfy
        beq brsend
        jsr reread
        bne brfail
brsend: jsr sendblk
        jsr pollcancel
        jsr nextblk
        jmp brloop
brfail: sta err
brdone: jmp opdone

; Write blocks: for each block an Acknowledge with its index requests its
; Block data, checked by its CRC, with a NAK for another copy, up to
; ATTEMPTS times; then the block is written, and with verify read back and compared.
bwrite: lda #OPWRITE
        jsr bargs
        bcs bwdone
bwloop: lda index
        cmp count
        beq bwdone
        lda #ATTEMPTS
        sta btry
        lda #ACK
bwask:  ldx index
        ldy #0
        jsr sendack
        jsr getblk
        bcc bwgot
        jsr drain
        dec btry
        beq bwlink
        lda #NAK
        jmp bwask
bwlink: lda #ERRLINK
        bne bwfail              ; always
bwgot:  jsr writeblk
        bne bwfail
        lda bvfy
        beq bwnext
        jsr reread
        bne bwfail
bwnext: jsr nextblk
        jmp bwloop
bwfail: sta err
bwdone: jmp opdone

; bargs: the request's fields, for the operation in A; C set, with
; ERRBLOCK, for a first block past SOS's 16 bits.
bargs:  sta op
        lda #0
        sta err
        sta index
        lda PKTBUF
        sta drive
        lda PKTBUF+1
        sta track
        lda PKTBUF+3
        sta bblk
        lda PKTBUF+4
        sta bblk+1
        lda PKTBUF+6
        sta count
        lda PKTBUF+7
        sta bvfy
        lda PKTBUF+5
        bne bahigh
        clc
        rts
bahigh: lda #ERRBLOCK
        sta err
        sec
        rts

; nextblk: the next block.
nextblk: inc bblk
        bne nbcount
        inc bblk+1
nbcount: inc index
        rts

; readblk, writeblk, readvfy: block bblk of device drive into BLKBUF, from
; it, or into VFYBUF, by D_READ and D_WRITE, made again once when SOS reports
; the disk switched. A is SOS's error, with Z set for none.
readblk: lda #<BLKBUF
        ldx #>BLKBUF
        jsr setread
        jmp dodread
readvfy: lda #<VFYBUF
        ldx #>VFYBUF
        jsr setread
dodread: .byte $00, SOSDREAD
        .word dreadlst
        cmp #SOSDISKSW
        bne drdone
        .byte $00, SOSDREAD
        .word dreadlst
drdone: cmp #0
        rts
setread: sta dreadlst+2
        stx dreadlst+3
        lda drive
        sta dreadlst+1
        lda bblk
        sta dreadlst+6
        lda bblk+1
        sta dreadlst+7
        rts
writeblk: lda drive
        sta dwritlst+1
        lda bblk
        sta dwritlst+6
        lda bblk+1
        sta dwritlst+7
        .byte $00, SOSDWRITE
        .word dwritlst
        cmp #SOSDISKSW
        bne dwdone
        .byte $00, SOSDWRITE
        .word dwritlst
dwdone: cmp #0
        rts

; reread: the block read again into VFYBUF and compared with BLKBUF; A is
; SOS's error or ERRVERIFY, with Z set when they match.
reread: jsr readvfy
        bne rrdone
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
        lda #0
rrdone: rts
rrbad:  lda #ERRVERIFY
        rts

; sendblk: Block data with bblk and BLKBUF's 512 bytes and their CRC.
sendblk: ldx #0
sbhdr:  lda blkhdr,x
        jsr putb
        inx
        cpx #5
        bne sbhdr
        lda #0
        sta peor
        sta crclo
        sta crchi
        lda bblk
        jsr putpe
        lda bblk+1
        jsr putpe
        lda #0
        jsr putpe
        ldy #0
sblo:   lda BLKBUF,y
        jsr crcupd
        lda BLKBUF,y
        jsr putpe
        iny
        bne sblo
sbhi:   lda BLKBUF+256,y
        jsr crcupd
        lda BLKBUF+256,y
        jsr putpe
        iny
        bne sbhi
        lda crclo
        jsr putpe
        lda crchi
        jsr putpe
        lda peor
        jmp putb

blkhdr: .byte $C1, <517, >517, CMDBLKDATA, $C1 ^ <517 ^ >517 ^ CMDBLKDATA

; getblk: the host's Block data for bblk into BLKBUF. C is set on a
; timeout, a packet that isn't Block data, another block or a failed CRC.
getblk: jsr getenvt
        bcs gbfail
        cmp #CMDCANCEL
        bne gbcmd
        jmp cancel
gbcmd:  cmp #CMDBLKDATA
        bne gbfail
        jsr getbtq
        bcs gbfail
        cmp bblk
        bne gbfail
        jsr getbtq
        bcs gbfail
        cmp bblk+1
        bne gbfail
        jsr getbtq
        bcs gbfail
        cmp #0
        bne gbfail
        ldy #0
gblo:   jsr getbtq
        bcs gbfail
        sta BLKBUF,y
        iny
        bne gblo
gbhi:   jsr getbtq
        bcs gbfail
        sta BLKBUF+256,y
        iny
        bne gbhi
        jsr getbtq
        bcs gbfail
        sta tmp
        jsr getbtq
        bcs gbfail
        sta tmp2
        jsr getbtq              ; the payload's EOR; the CRC checks the block
        bcs gbfail
        lda #0
        sta crclo
        sta crchi
        ldy #0
gbclo:  lda BLKBUF,y
        jsr crcupd
        iny
        bne gbclo
gbchi:  lda BLKBUF+256,y
        jsr crcupd
        iny
        bne gbchi
        lda crclo
        cmp tmp
        bne gbfail
        lda crchi
        cmp tmp2
        bne gbfail
        clc
        rts
gbfail: sec
        rts

; pollcancel: a Cancel the host sent during a read. The host sends nothing
; else then, so any byte is taken as Cancel.
pollcancel: lda rxpos
        cmp rxn
        bcc pcany
        jsr fill
        lda rxn
        beq pcdone
pcany:  jmp cancel
pcdone: rts

; ---------------------------------------------------------------------------
; Packets
; ---------------------------------------------------------------------------

; sendready: Ready with the version, SOSFLAG, no drives, five zero bytes in
; the ROM identification's place, and the CRC selfcrc computed.
sendready: ldx #0
srloop: lda readypkt,x
        jsr putb
        inx
        cpx #readyend-readypkt
        bne srloop
        rts

readypkt: .byte $C1, $0B, $00, CMDREADY, $C1 ^ $0B ^ $00 ^ CMDREADY
          .byte <PROGVER, >PROGVER, SOSFLAG, 0
          .byte 0, 0, 0, 0, 0
readycrc: .byte 0, 0                ; selfcrc sets it, low byte first
readychk: .byte 0                   ; and the payload's EOR
readyend:

; sendack: Acknowledge with the kind in A, then X and Y.
sendack: sta tmp
        stx tmp2
        sty tmp3
        ldx #0
saloop: lda ackhdr,x
        jsr putb
        inx
        cpx #5
        bne saloop
        lda tmp
        jsr putb
        lda tmp2
        jsr putb
        lda tmp3
        jsr putb
        lda tmp
        eor tmp2
        eor tmp3
        jmp putb

ackhdr: .byte $C1, $03, $00, CMDACK, $C1 ^ $03 ^ $00 ^ CMDACK

; sendstatus: Status with the operation, the device, its unit, the blocks
; done and the error.
sendstatus: ldx #0
ssloop: lda stathdr,x
        jsr putb
        inx
        cpx #5
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

; getenvt: an envelope, waiting tmolen seconds for its first byte. A and
; envcmd are the command and lenlo and lenhi the payload's length, with C
; clear; C is set on a timeout or a bad check.
getenvt: jsr getbt
        bcs gefail
geskip: cmp #$C1
        beq gestart
        jsr getbtq
        bcs gefail
        jmp geskip
gestart: sta tmp
        jsr getbtq
        bcs gefail
        sta lenlo
        eor tmp
        sta tmp
        jsr getbtq
        bcs gefail
        sta lenhi
        eor tmp
        sta tmp
        jsr getbtq
        bcs gefail
        sta envcmd
        eor tmp
        sta tmp
        jsr getbtq
        bcs gefail
        cmp tmp
        bne gefail
        lda envcmd
        clc
        rts
gefail: sec
        rts

; getpayload: a request's payload, at most 255 bytes, into PKTBUF, and its
; EOR checked. C is set on a timeout, a bad EOR or a longer payload.
getpayload: lda lenhi
        bne gpfail
        lda lenlo
        beq gpdone
        ldy #0
        sty peor
gploop: jsr getbtq
        bcs gpfail
        sta PKTBUF,y
        eor peor
        sta peor
        iny
        cpy lenlo
        bne gploop
        jsr getbtq
        bcs gpfail
        cmp peor
        bne gpfail
gpdone: clc
        rts
gpfail: sec
        rts

; drain: arriving bytes dropped, until a second passes with none.
drain:  jsr getbtq
        bcc drain
        rts

; putpe: putb, adding A into peor.
putpe:  pha
        eor peor
        sta peor
        pla
        jmp putb

; crcupd: CRC-16/XMODEM of A into crclo and crchi, by the table. X and Y
; are saved and restored.
crcupd: stx savex
        eor crchi
        tax
        lda crclo
        eor CRCHI,x
        sta crchi
        lda CRCLO,x
        sta crclo
        ldx savex
        rts

; ---------------------------------------------------------------------------
; .RS232
; ---------------------------------------------------------------------------

; putb: A into TXBUF, written once TXMAX are there. X and Y are saved and restored.
putb:   stx savex
        ldx txn
        sta TXBUF,x
        inc txn
        ldx savex
        pha
        lda txn
        cmp #TXMAX
        bcc pbdone
        jsr flush
pbdone: pla
        rts

; flush: TXBUF written to .RS232, which waits for room in the driver's
; buffer.
flush:  lda txn
        beq fldone
        sta writelst+4
        .byte $00, SOSWRITE
        .word writelst
        lda #0
        sta txn
fldone: rts

; fill: RXBUF refilled by READ, which with immediate read on returns what
; .RS232 has received. rxn is the bytes read, and rxpos 0. X and Y are saved and restored.
fill:   lda #0
        sta rxpos
        sta rxn
        .byte $00, SOSREAD
        .word readlst
        bne fidone
        lda readlst+6
        sta rxn
fidone: rts

; getbt: the next byte received, the bytes put written first, waiting
; tmolen seconds; getbtq waits a second. C is set on a timeout. X and Y
; are saved and restored.
getbtq: lda tmolen
        pha
        lda #1
        sta tmolen
        jsr getbt
        pla
        sta tmolen
        lda tmp3                ; getbt's byte, as the PLA changed A
        rts
getbt:  stx savex
        sty savey
        ldx rxpos
        cpx rxn
        bcc gbtake
        jsr flush
        lda tmolen
        sta tmp3
gbsec:  lda #<POLLS
        sta pollo
        lda #>POLLS
        sta polhi
gbpoll: jsr fill
        ldx rxn
        bne gbtake0
        ldx #100                ; half a millisecond
gbwait: dex
        bne gbwait
        lda pollo
        bne gbdec
        dec polhi
gbdec:  dec pollo
        lda pollo
        ora polhi
        bne gbpoll
        dec tmp3
        bne gbsec
        ldx savex
        ldy savey
        sec
        rts
gbtake0: ldx #0
gbtake: lda RXBUF,x
        sta tmp3
        inx
        stx rxpos
        ldx savex
        ldy savey
        clc
        rts

; openrs: .RS232 opened and set up: 8 data bits, no parity, no protocol, no
; hardware handshake, immediate read, newline mode off, and the rate in the
; byte after progend. C is set when SOS has no .RS232 or returns an error
; for the setup.
openrs: .byte $00, SOSOPEN
        .word rsopen
        bne orfail
        lda rsref
        sta readlst+1
        sta writelst+1
        .byte $00, SOSDEVNUM
        .word devnumlst
        bne orfail
        lda rsdev
        sta dstatlst+1
        sta dctllst+1
        sta dstat3+1
        sta dnewlst+1
        lda #RSCONTROL
        sta dstatlst+2
        .byte $00, SOSDSTATUS
        .word dstatlst
        bne orfail
        lda progend
        and #$0F
        sta dcb+RSBAUD
        lda #RS8N1
        sta dcb+RSFORMAT
        lda #0
        sta dcb+RSPROTOCOL
        sta dcb+RSHANDSHK
        lda #RSON
        sta dcb+RSIMREAD
        jsr rsconfig
        bne orfail
        .byte $00, SOSDCTL
        .word dnewlst
        bne orfail
        clc
        rts
orfail: sec
        rts

; rsconfig: the control parameters in dcb set, which resets the driver.
; A is SOS's error, with Z set for none.
rsconfig: lda #RSPARMS
        sta dcb
        .byte $00, SOSDCTL
        .word dctllst
        rts

; rsstatus3: the driver's buffer sizes and counts into rsbufs; C is set
; when SOS returns an error.
rsstatus3: .byte $00, SOSDSTATUS
        .word dstat3
        cmp #1
        rts

; ---------------------------------------------------------------------------
; Console
; ---------------------------------------------------------------------------

; opencons: .CONSOLE opened for the messages; with no console, conref
; stays 0 and nothing is written.
opencons: .byte $00, SOSOPEN
        .word conopen
        beq ocdone
        lda #0
        sta conref
ocdone: lda conref
        sta conwrite+1
        rts

; showmsg: the message at msgs + X, ending in a zero byte, on a line by
; itself.
showmsg:    lda #0
        sta linen
syloop: lda msgs,x
        beq syend
        jsr lineput
        inx
        bne syloop
syend:  jmp lineend

; lineput: A, without bit 7, into LINEBUF at linen, which advances, up to
; 79 characters. X and Y are saved and restored.
lineput: stx linex
        and #$7F
        ldx linen
        cpx #79
        bcs lpdone
        sta LINEBUF,x
        inc linen
lpdone: ldx linex
        rts

; linedec: A, 0 to 99, as two digits into LINEBUF.
linedec: ldy #'0'
ldtens: cmp #10
        bcc ldones
        sbc #10
        iny
        jmp ldtens
ldones: pha
        tya
        jsr lineput
        pla
        clc
        adc #'0'
        jmp lineput

; lineend: LINEBUF's linen bytes written to the console with a return and
; a line feed.
lineend: ldx linen
        lda #CR
        sta LINEBUF,x
        inx
        lda #LF
        sta LINEBUF,x
        inx
        stx conwrite+4
        lda conref
        beq ledone
        .byte $00, SOSWRITE
        .word conwrite
ledone: rts

; ---------------------------------------------------------------------------
; Tables and timing
; ---------------------------------------------------------------------------

; mktables: the CRC-16/XMODEM table.
mktables: ldx #0
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

; selfcrc: the CRC-16/XMODEM of the program as loaded, SOSORG up to
; progend, into Ready, for the host to compare with the image it assembles,
; and Ready's EOR. The loop's load instruction advances the address in its
; operand, so the operand's two bytes are counted as assembled, SOSORG, not
; as they are when the loop reads them.
selfcrc: lda #0
        sta crclo
        sta crchi
        lda #<SOSORG
        sta scget+1
        lda #>SOSORG
        sta scget+2
scget:  lda SOSORG
        tay
        lda scget+1
        ldx scget+2
        cmp #<(scget+1)
        bne scnot1
        cpx #>(scget+1)
        bne scnot1
        ldy #<SOSORG
scnot1: cmp #<(scget+2)
        bne scnot2
        cpx #>(scget+2)
        bne scnot2
        ldy #>SOSORG
scnot2: tya
        jsr crcupd
        inc scget+1
        bne sccheck
        inc scget+2
sccheck: lda scget+1
        cmp #<progend
        bne scget
        lda scget+2
        cmp #>progend
        bne scget
        lda crclo
        sta readycrc
        lda crchi
        sta readycrc+1
        lda #<PROGVER ^ >PROGVER ^ SOSFLAG
        eor readycrc
        eor readycrc+1
        sta readychk
        rts

; delayms: about A milliseconds at the Apple ///'s 2 MHz.
delayms: tay
dmouter: ldx #200
dminner: dex
        bne dminner
        ldx #200
dminner2: dex
        bne dminner2
        dey
        bne dmouter
        rts

; ---------------------------------------------------------------------------
; SOS parameter lists
; ---------------------------------------------------------------------------

rsname:  .byte 6
         .text ".RS232"
conname: .byte 8
         .text ".CONSOLE"
rsopen:  .byte 4
         .word rsname
rsref:   .byte 0
         .word 0
         .byte 0
conopen: .byte 4
         .word conname
conref:  .byte 0
         .word 0
         .byte 0
devnumlst: .byte 2
         .word rsname
rsdev:   .byte 0
readlst: .byte 4, 0
         .word RXBUF, RXMAX, 0
writelst: .byte 3, 0
         .word TXBUF, 0
conwrite: .byte 3, 0
         .word LINEBUF, 0
closelst: .byte 1, 0
dstatlst: .byte 3, 0, RSCONTROL
         .word dcb
dctllst: .byte 3, 0, RSCONTROL
         .word dcb
dstat3:  .byte 3, 0, RSBUFFERS
         .word rsbufs
dnewlst: .byte 3, 0, RSNEWLINE
         .word newline
newline: .byte 0, CR               ; newline mode off
infolst: .byte 4, 0
         .word devname, devinfo
         .byte 7                   ; slot through total_blocks
dreadlst: .byte 5, 0
         .word BLKBUF, 512, 0, 0
dwritlst: .byte 4, 0
         .word BLKBUF, 512, 0
dcb:     .byte RSPARMS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
rsbufs:  .byte 0, 0, 0, 0, 0, 0, 0, 0
devname: .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
devinfo: .byte 0, 0, 0, 0, 0, 0, 0

; Messages, each ending in a zero byte.
msgs:
msgtitle: .text "CassoLink for the Apple ///"
          .byte 0
msgwait:  .text "Waiting for Casso Explorer on .RS232"
          .byte 0
msgnors:  .text "No .RS232 driver. Add it to SOS.DRIVER with the System Configuration Program."
          .byte 0

; The end of the program, where selfcrc stops. The CassoLink SOS disk puts
; the 6551 control byte for the link's speed here.
progend:

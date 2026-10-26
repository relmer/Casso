; Casso's CassoLink: the shim.
;
; Resident at $3B00, below the program's first table at $4000. It sets the
; 6551 to its speed, sends Ready with no program loaded, and takes the
; reply. Load program brings the program as Sector data packets, one page
; each, the page number in the track field: each page is stored from $0800 up
; and checked against its CRC-16/XMODEM, and the shim sends a bare ACK or NAK
; byte for it. After the last page the shim jumps to the program.
;
; The 6551 control byte for the starting speed is at $3B02 and the speed's
; text, six characters, at $3B06. The host writes both for the link's speed;
; as assembled they are 9,600 baud. At $3B0C is a Wi-Fi modem's dial
; command, ending in a zero byte: when it is not empty, the shim draws the
; target it dials, sends the command and a carriage return, and waits for
; the end of the modem's reply line before its first Ready. The program sets
; bit 7 of the command's first byte before Load program, as the modem is
; connected then.
;
; The 6551's slot is at $3B38: 2 for the //c's port 2, as assembled, 1 for
; its port 1, or a Super Serial Card's slot. The host writes it, and the
; shim sets its own reads and writes of the 6551 from it.
;
; The shim draws its state on the text page: a title, then waiting for the
; PC with the interface below it, the program's loading as a percentage, and
; the version it starts. Each page's percentage is drawn before its ACK,
; since the host sends the next page only after the ACK and the 6551 has
; room for one byte.
;
; NMOS 6502 code throughout. The program finds the 6551 through slotx, which
; the shim sets.
;
; The W5100 build, for an Uthernet II, has the same header and address.
; In the dial command's place are the card's address, subnet mask, gateway
; and MAC address, and the TCP port, high byte first; the speed is not used.
; The shim sets the chip from them, assigns all 8 KB of the chip's receive
; memory and 4 KB of its transmit memory to socket 0, and listens on the
; port; Casso Explorer connects to it. Bytes to send are put in the chip's
; transmit memory and sent together, before each wait for a byte, rather
; than one segment per byte. Back from the program's Load program with the
; PC connected, the connection stays open. When the PC disconnects, the shim
; listens again until the PC connects.

        ifndef SHIMVER
SHIMVER = 1
        endif

; Ready's shim version: a W5100 build sets bit 7, so the host sends the
; program built for the chip whatever the link it connected through.
        ifdef W5100
READYVER = SHIMVER + $80
        else
READYVER = SHIMVER
        endif

ACIADATA = $C088                ; the 6551, + slot * 16
ACIASTAT = $C089
ACIACMD  = $C08A
ACIACTL  = $C08B
RDRF     = $08                  ; status: a byte has arrived
TDRE     = $10                  ; status: the transmitter is free

        ifdef W5100
WZMR     = $C084                ; the W5100's mode register, + slot * 16
WZHI     = $C085                ; its pointer, high byte
WZLO     = $C086                ; and low byte
WZDATA   = $C087                ; the byte at the pointer, which then advances
SKREG    = $04                  ; socket 0's registers' page
SKMR     = $00                  ; its mode
SKCR     = $01                  ; its command
SKIR     = $02                  ; its interrupts
SKSR     = $03                  ; its state
SKPORT   = $04                  ; its port
SKTXWR   = $24                  ; where the next byte to send goes
SKRXRSR  = $26                  ; bytes received
SKRXRD   = $28                  ; the next byte to read
TXMEM    = $40                  ; socket 0's transmit memory's page, for 4 KB
RXMEM    = $60                  ; the receive memory's
CMOPEN   = $01
CMLISTEN = $02
CMCLOSE  = $10
CMSEND   = $20
CMRECV   = $40
STLISTEN = $14
STSYNRCV = $16
STESTAB  = $17
IRSENDOK = $10
        endif

CMDREADY = $D2
CMDLOAD  = $CF
CMDDATA  = $CA
ACK      = $06
NAK      = $15

PROGRAM  = $0800
ZPSAVE   = $3A00                ; the machine's zero page, as the shim found it,
                                ; on the page below the shim, which runs to $3FFF
PROGVERLO = $0803               ; the program's version, low byte

ROW0     = $0400                ; the title
ROW2     = $0500                ; the state
ROW4     = $0600                ; the interface, below a blank row

slotx    = $06                  ; the 6551's index, slot * 16, for the program
crclo    = $07                  ; the CRC being computed
crchi    = $08
ptr      = $09                  ; and $0A: where the page goes
pages    = $0B                  ; pages in the program
page     = $0C                  ; the page being received
gotlo    = $0D                  ; the CRC it came with
gothi    = $0E
envchk   = $0F
envcmd   = $10
prodlo   = $11                  ; the percentage's numerator
prodhi   = $12
quot     = $13                  ; the percentage
num      = $14                  ; putdec: the value left
digit    = $15
lead     = $16                  ; putdec: nonzero once a digit is drawn
tmolo    = $17                  ; getbto: polls left, and $18
tmohi    = $18
        ifdef W5100
rxrd     = $19                  ; and $1A: the next byte to read, as Sn_RX_RD counts
rxn      = $1B                  ; and $1C: bytes received and not yet read
txwr     = $1D                  ; and $1E: where the next byte goes, as Sn_TX_WR counts
txn      = $1F                  ; bytes put since the last SEND
savx     = $20                  ; getbto and putb: X and Y as they came
savy     = $21
        endif

        .org $3B00

        sei
        lda #$1E                ; $3B02: 8 data bits, 1 stop bit, 9,600 baud
        jmp start
speedtxt: .text "9600  "        ; $3B06: the speed, six characters
dialtxt: .ds 40                  ; $3B0C: the modem's dial command, or empty
zpsaved: .byte 0                 ; $3B34: nonzero once the machine's state is saved
oldreset: .byte 0, 0, 0          ; $3B35: the reset vector and its check byte
slot:   .byte 2                  ; $3B38: the 6551's slot

; The first start saves the machine's zero page at ZPSAVE and its reset
; vector, for the program to put back when CassoLink ends. A bootstrap or a
; boot disk has zpsaved at 0; the program's Load program, back to the shim,
; finds it set and saves nothing.
start:  pha                     ; the control byte, for the 6551 once it is found
        cld
        lda zpsaved
        bne zpdone
        ldx #0
zpsave: lda $00,x
        sta ZPSAVE,x
        inx
        bne zpsave
        ldx #2
rvsave: lda $03F2,x
        sta oldreset,x
        dex
        bpl rvsave
        inc zpsaved
zpdone:
        lda slot
        asl a
        asl a
        asl a
        asl a
        sta slotx
        ifdef W5100
        pla                     ; the 6551 control byte, not used
        jsr home
        jsr wzstart
        else
        ora #<ACIADATA          ; each read and write of the 6551 is absolute,
        sta gtdata+1            ; set here for the slot
        sta pbdata+1
        ora #1
        sta gtstat+1
        sta pbstat+1
        ldx slotx
        pla
        sta ACIACTL,x
        lda #$0B                ; DTR on, no parity, no interrupts
        sta ACIACMD,x
        jsr home

        lda dialtxt             ; bit 7 of its first byte is set once the modem
        beq ready               ; is connected, as back from Load program
        bmi ready
        ldx #msgdial-msgs       ; the target, as the command has it after ATDT
        jsr drawstate           ; and its spaces, in capitals and to 40 columns
        ldx #4
dlskip: lda dialtxt,x
        cmp #$20
        bne dltarget
        inx
        bne dlskip
dltarget: lda dialtxt,x
        beq dlsend
        cmp #$61
        bcc dlcaps
        and #$DF
dlcaps: ora #$80
        sta ROW2,y
        inx
        iny
        cpy #40
        bne dltarget
dlsend: ldx #0
dial:   lda dialtxt,x
        beq dialed
        jsr putb
        inx
        bne dial
dialed: lda #$0D
        jsr putb
modemreply: jsr getb               ; the modem's reply line, such as CONNECT,
        cmp #$0A               ; ends in a line feed; Ready waits for it
        bne modemreply
        endif

ready:  ldx #msgwait-msgs
        jsr drawstate
        ifdef W5100
        lda #0                  ; the address, a byte at a time, then the port
        sta page                ; free until the program's pages come
wdip:   ldx page
        lda dialtxt,x
        jsr putdec
        inc page
        lda page
        cmp #4
        beq wdport
        lda #$AE                ; a period
        sta ROW2,y
        iny
        bne wdip
wdport: lda #$BA                ; a colon
        sta ROW2,y
        iny
        jsr putport
        jsr wzwait
        else
        ldx #0                  ; the interface, as the universal disk lists it:
        lda $FBB3               ; a card or a //c's port, by cable or through
        eor #$06                ; a Wi-Fi modem, as SSC, SLOT 2, 115200. A //c
        ora $FBC0               ; has $06 at $FBB3 and $00 at $FBC0
        bne rdmodem
        ldx #2
rdmodem: lda dialtxt
        beq rdname
        inx
rdname: lda ifnames,x
        tax
        ldy #0
rdtext: lda msgs,x
        beq rdslot
        ora #$80
        sta ROW4,y
        iny
        inx
        bne rdtext
rdslot: lda slot
        ora #$B0
        sta ROW4,y
        iny
        lda #$AC                ; a comma, and the row is blank, so a column
        sta ROW4,y              ; skipped is a space
        iny
        iny
        ldx #0
spdtext: lda speedtxt,x
        ora #$80
        sta ROW4,y
        iny
        inx
        cpx #6
        bne spdtext
        endif
        ldx #0
sendrdy: lda readypkt,x
        jsr putb
        inx
        cpx #readyend-readypkt
        bne sendrdy

        jsr getenv
        bcs ready
        cmp #CMDLOAD
        bne ready
        jsr getb                ; the page count
        sta pages
        jsr getb                ; its EOR, the same byte
        cmp pages
        bne ready

loadpg: jsr getenv
        bcs ready
        cmp #CMDDATA
        bne ready
        jsr getbto              ; the page number
        bcs badpg
        sta page
        clc
        adc #>PROGRAM
        sta ptr+1
        jsr getbto              ; the sector field, not used
        bcs badpg
        ldy #0
        sty ptr
        sty crclo
        sty crchi
data:   jsr getbto              ; stored as it arrives: at 115,200 baud a byte
        bcs badpg               ; comes every 89 cycles, too few for the CRC
        sta (ptr),y
        iny
        bne data
        jsr getbto
        bcs badpg
        sta gotlo
        jsr getbto
        bcs badpg
        sta gothi
        jsr getbto              ; the payload's EOR; the CRC checks the page
        bcs badpg
pagecrc: lda (ptr),y              ; the CRC, once the page is in
        jsr crcbyte
        iny
        bne pagecrc

        lda crclo
        cmp gotlo
        bne badpg
        lda crchi
        cmp gothi
        bne badpg
        jsr showpct
        lda #ACK
        jsr putb
        ldx page
        inx
        cpx pages
        bne loadpg
        ldx #msgstart-msgs
        jsr drawstate
        lda PROGVERLO
        jsr putdec
        ifdef W5100
        jsr flush               ; the last ACK sent, and the bytes read given
        jsr rxdone              ; back: the program starts from the chip's
        endif
        jmp PROGRAM             ; own pointers

; A page cut short, by a byte lost on the line, ends in a timeout rather than
; a wait for good; the NAK has the host send it again, and the shim takes it
; from its envelope.
badpg:  lda #NAK                ; the host sends the page again
        jsr putb
        jmp loadpg


; home: the text page cleared and the title drawn.

home:   lda #$A0
        ldy #0
hmclear: sta $0400,y
        sta $0500,y
        sta $0600,y
        sta $0700,y
        iny
        bne hmclear
        ldx #0
hmtitle: lda title,x
        beq hmdone
        ora #$80
        sta ROW0,y
        iny
        inx
        bne hmtitle
hmdone: rts


; drawstate: the state's rows cleared and the message at msgs+X drawn from
; column 0. Y is the column after it.

drawstate:   ldy #39
        lda #$A0
shclear: sta ROW2,y
        sta ROW4,y
        dey
        bpl shclear
        iny
shtext: lda msgs,x
        beq shdone
        ora #$80
        sta ROW2,y
        iny
        inx
        bne shtext
shdone: rts


; showpct: the loading message and (page + 1) * 100 / pages, then a percent
; sign.

showpct: ldx #msgload-msgs
        jsr drawstate
        lda #0
        sta prodlo
        sta prodhi
        sta quot
        ldx page
        inx
spmul:  clc
        lda prodlo
        adc #100
        sta prodlo
        bcc spmul2
        inc prodhi
spmul2: dex
        bne spmul
spdiv:  lda prodlo
        sec
        sbc pages
        tax
        lda prodhi
        sbc #0
        bcc spdone
        sta prodhi
        stx prodlo
        inc quot
        bne spdiv
spdone: lda quot
        jsr putdec
        lda #$A5                ; a percent sign
        sta ROW2,y
        rts


; putdec: A in decimal, without leading zeros, on the state row from Y.

putdec: sta num
        lda #0
        sta lead
        ldx #2
pdloop: lda #0
        sta digit
pdsub:  lda num
        cmp pdtab,x
        bcc pdout
        sbc pdtab,x
        sta num
        inc digit
        bne pdsub
pdout:  lda digit
        ora lead
        bne pdput
        cpx #0
        bne pdnext
pdput:  lda digit
        ora #$B0
        sta ROW2,y
        sta lead
        iny
pdnext: dex
        bpl pdloop
        rts

pdtab:  .byte 1, 10, 100


; getenv: an envelope, skipping bytes until a start byte. A is the command,
; and the carry is set when the envelope's check fails.

getenv: jsr getb
        cmp #$C1
        bne getenv
        ldx #3
genext: sta envchk
        jsr getb                ; length low, length high, command
        sta envcmd
        eor envchk
        dex
        bne genext
        sta envchk
        jsr getb                ; check
        cmp envchk
        clc
        beq geok
        sec
geok:   lda envcmd
        rts


; crcbyte: CRC-16/XMODEM of A into crclo and crchi. X is overwritten.

crcbyte: eor crchi
        sta crchi
        ldx #8
crcbit: asl crclo
        rol crchi
        bcc crcnext
        lda crchi
        eor #$10
        sta crchi
        lda crclo
        eor #$21
        sta crclo
crcnext: dex
        bne crcbit
        rts


; getb and putb: one byte in or out through the 6551. A is the byte.

getb:   jsr getbto
        bcs getb
        rts

        ifdef W5100
; getbto: getb with a time limit of about a second, 16,384 polls of the
; chip; the bytes put are sent first. The carry is set when no byte comes.
; With the PC gone, the shim waits for it again. X and Y are saved and
; restored.
getbto: stx savx
        sty savy
        lda rxn
        ora rxn+1
        bne gtread
        jsr flush
        lda #0
        sta tmolo
        lda #$C0
        sta tmohi
gtpoll: ldy #SKRXRSR
        jsr wzsock
        lda WZDATA,x
        sta rxn+1
        lda WZDATA,x
        sta rxn
        ora rxn+1
        bne gtread
        inc tmolo
        bne gtpoll
        inc tmohi
        bne gtpoll
        jsr wzstat
        cmp #STESTAB
        beq gtnone
        ldx #$FF
        txs
        jmp ready
gtnone: ldx savx
        ldy savy
        sec
        rts
gtread: lda rxrd+1              ; at $6000 + (Sn_RX_RD & $1FFF)
        and #$1F
        ora #RXMEM
        ldy rxrd
        jsr wzptr
        lda WZDATA,x
        pha
        inc rxrd
        bne gtnext
        inc rxrd+1
gtnext: lda rxn
        bne gtcount
        dec rxn+1
gtcount: dec rxn
        lda rxn
        ora rxn+1
        bne gtgot
        jsr rxdone
gtgot:  pla
        ldx savx
        ldy savy
        clc
        rts

; putb: A into the chip's transmit memory, sent with the next wait for a
; byte, or once 128 bytes are waiting. X and Y are saved and restored.
putb:   stx savx
        sty savy
        pha
        lda txwr+1              ; at $4000 + (Sn_TX_WR & $0FFF)
        and #$0F
        ora #TXMEM
        ldy txwr
        jsr wzptr
        pla
        jsr wzput
        inc txwr
        bne pbcount
        inc txwr+1
pbcount: inc txn
        bpl pbdone
        jsr flush
pbdone: ldx savx
        ldy savy
        rts

; flush: the bytes put, sent, once the chip has sent them all; with no PC
; they are dropped.
flush:  lda txn
        beq fldone
        jsr wzstat
        cmp #STESTAB
        bne fldrop
        ldy #SKTXWR
        jsr wzsock
        lda txwr+1
        jsr wzput
        lda txwr
        jsr wzput
        lda #CMSEND
        jsr wzcmd
flwait: ldy #SKIR
        jsr wzsock
        lda WZDATA,x
        and #IRSENDOK
        bne flsent
        jsr wzstat
        cmp #STESTAB
        beq flwait
        bne fldrop
flsent: ldy #SKIR
        jsr wzsock
        lda #IRSENDOK
        jsr wzput
        lda #0
        sta txn
fldone: rts
fldrop: jmp wzsync

; rxdone: the bytes read given back to the chip, Sn_RX_RD and RECV.
rxdone: ldy #SKRXRD
        jsr wzsock
        lda rxrd+1
        jsr wzput
        lda rxrd
        jsr wzput
        lda #CMRECV
        jmp wzcmd

; wzstart: the chip reset and set from the header, and socket 0 listening;
; or, back from the program with the PC connected, left as it is.
wzstart: lda slotx
        ora #<WZDATA            ; each write of the data is absolute, as an
        sta wzput+1             ; indexed store reads it first, which would
        ldx slotx               ; advance the pointer
        lda #$03                ; indirect, with the pointer advancing
        sta WZMR,x
        jsr wzstat
        cmp #STESTAB
        beq wzsync
        lda #$80
        sta WZMR,x
wzreset: lda WZMR,x
        bmi wzreset
        lda #$03
        sta WZMR,x
        ldy #$01                ; the gateway, mask, MAC address and address
        lda #$00
        jsr wzptr
        ldx #0
wzcfg:  ldy cfgtab,x
        lda dialtxt,y
        jsr wzput
        inx
        cpx #cfgend-cfgtab
        bne wzcfg
        ldy #$1A                ; RMSR: all 8 KB of receive memory to socket 0;
        lda #$00                ; TMSR: 4 KB of transmit memory to socket 0
        jsr wzptr               ; and 4 KB to socket 1, which a universal
        lda #$03                ; disk sends its announcements on
        jsr wzput
        lda #$0A
        jsr wzput

; wzlisten: socket 0 closed, opened for TCP on the header's port, and
; listening.
wzlisten: lda #CMCLOSE
        jsr wzcmd
        ldy #SKMR
        jsr wzsock
        lda #$01                ; TCP
        jsr wzput
        ldy #SKPORT
        jsr wzsock
        lda dialtxt+18
        jsr wzput
        lda dialtxt+19
        jsr wzput
        lda #CMOPEN
        jsr wzcmd
        lda #CMLISTEN
        jsr wzcmd

; wzsync: where the chip's pointers are, with nothing read or put.
wzsync: ldy #SKTXWR
        jsr wzsock
        lda WZDATA,x
        sta txwr+1
        lda WZDATA,x
        sta txwr
        ldy #SKRXRD
        jsr wzsock
        lda WZDATA,x
        sta rxrd+1
        lda WZDATA,x
        sta rxrd
        lda #0
        sta rxn
        sta rxn+1
        sta txn
        rts

; wzwait: until the PC connects. A socket that stops listening, as when the
; PC disconnects, listens again after about half a second.
wzwait: jsr wzstat
        cmp #STESTAB
        beq wwdone
        cmp #STLISTEN
        beq wzwait
        cmp #STSYNRCV
        beq wzwait
        lda #0
        sta tmolo
        sta tmohi
wwpause: inc tmolo
        bne wwpause
        inc tmohi
        bne wwpause
        jsr wzlisten
        jmp wzwait
wwdone: rts

; wzcmd: the command in A to socket 0, and the wait until the chip takes it.
wzcmd:  pha
        ldy #SKCR
        jsr wzsock
        pla
        jsr wzput
wcwait: ldy #SKCR
        jsr wzsock
        lda WZDATA,x
        bne wcwait
        rts

; wzstat: socket 0's state in A.
wzstat: ldy #SKSR
        jsr wzsock
        lda WZDATA,x
        rts

; wzsock: the pointer at socket 0's register Y. X is slotx.
wzsock: lda #SKREG

; wzptr: the pointer at A (high) and Y (low). X is slotx.
wzptr:  ldx slotx
        sta WZHI,x
        tya
        sta WZLO,x
        rts

; wzput: A at the pointer, which then advances; wzstart sets the address.
wzput:  sta WZDATA
        rts

; putport: the header's port in decimal, without leading zeros, on the
; state row from Y.
putport: lda dialtxt+18
        sta prodhi
        lda dialtxt+19
        sta prodlo
        lda #0
        sta lead
        ldx #4
ppdigit: lda #0
        sta digit
ppsub:  lda prodlo
        sec
        sbc pptablo,x
        sta quot
        lda prodhi
        sbc pptabhi,x
        bcc ppout
        sta prodhi
        lda quot
        sta prodlo
        inc digit
        bne ppsub
ppout:  lda digit
        ora lead
        bne ppput
        cpx #0
        bne ppnext
ppput:  lda digit
        ora #$B0
        sta ROW2,y
        sta lead
        iny
ppnext: dex
        bpl ppdigit
        rts

pptablo: .byte <1, <10, <100, <1000, <10000
pptabhi: .byte >1, >10, >100, >1000, >10000

; The header's network, from the dial command's place, in the order of the
; chip's registers from $0001: GAR, SUBR, SHAR and SIPR.
cfgtab: .byte 8, 9, 10, 11, 4, 5, 6, 7, 12, 13, 14, 15, 16, 17, 0, 1, 2, 3
cfgend:

        else
; getbto: getb with a time limit of about a second: 65,536 polls of 16
; cycles. The carry is set when no byte comes.
getbto: lda #0
        sta tmolo
        sta tmohi
gtwait:
gtstat: lda ACIASTAT
        and #RDRF
        bne gtgot
        inc tmolo
        bne gtwait
        inc tmohi
        bne gtwait
        sec
        rts
gtgot:
gtdata: lda ACIADATA
        clc
        rts

putb:   pha
putwait:
pbstat: lda ACIASTAT
        and #TDRE
        beq putwait
        pla
pbdata: sta ACIADATA
        rts
        endif


title:  .text "CASSOLINK"
        .byte 0
msgs:
        ifdef W5100
msgwait: .text "WAITING FOR PC AT "      ; and the address and port, to 39 columns
        .byte 0
        else
msgwait: .text "WAITING FOR THE PC ON:"
        .byte 0
msgssc: .text "SSC, SLOT "
        .byte 0
msgsscm: .text "WI-FI MODEM, SLOT "
        .byte 0
msgport: .text "SERIAL PORT "
        .byte 0
msgportm: .text "WI-FI MODEM, PORT "
        .byte 0
        endif
msgload: .text "LOADING PROGRAM "
        .byte 0
msgstart: .text "STARTING VERSION "
        .byte 0
        ifndef W5100
msgdial: .text "DIALING "
        .byte 0

; The interface's name by card or port, then cable or modem. After the
; text, as the assembler sizes a forward reference in .byte wrongly.
ifnames: .byte msgssc-msgs, msgsscm-msgs, msgport-msgs, msgportm-msgs
        endif

; Ready with no program: version 0, the shim's version, no drives.
readypkt: .byte $C1, $04, $00, CMDREADY, $C1 ^ $04 ^ $00 ^ CMDREADY
          .byte $00, $00, READYVER, $00
          .byte $00 ^ $00 ^ READYVER ^ $00
readyend:

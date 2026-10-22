; Casso's CassoLink: the shim.
;
; Resident at $3D00, below the program's first table at $4000. It sets the
; 6551 to its speed, sends Ready with no program loaded, and takes the
; reply. Load program brings the program as Sector data packets, one page
; each, the page number in the track field: each page is stored from $0800 up
; and checked against its CRC-16/XMODEM, and the shim sends a bare ACK or NAK
; byte for it. After the last page the shim jumps to the program.
;
; The 6551 control byte for the starting speed is at $3D02 and the speed's
; text, six characters, at $3D06. The host writes both for the link's speed;
; as assembled they are 9,600 baud. At $3D0C is a Wi-Fi modem's dial
; command, ending in a zero byte: when it is not empty, the shim sends it
; and a carriage return, and waits for the end of the modem's reply line
; before its first Ready.
;
; The 6551's slot is at $3D38: 2 for the //c's port 2, as assembled, 1 for
; its port 1, or a Super Serial Card's slot. The host writes it, and the
; shim sets its own reads and writes of the 6551 from it.
;
; The shim draws its state on the text page: a title, then waiting for the
; PC, the program's loading as a percentage, and the version it starts. Each
; page's percentage is drawn before its ACK, since the host sends the next
; page only after the ACK and the 6551 has room for one byte.
;
; NMOS 6502 code throughout. The program finds the 6551 through slotx, which
; the shim sets.

        ifndef SHIMVER
SHIMVER = 1
        endif

ACIADATA = $C088                ; the 6551, + slot * 16
ACIASTAT = $C089
ACIACMD  = $C08A
ACIACTL  = $C08B
RDRF     = $08                  ; status: a byte has arrived
TDRE     = $10                  ; status: the transmitter is free

CMDREADY = $D2
CMDLOAD  = $CF
CMDDATA  = $CA
ACK      = $06
NAK      = $15

PROGRAM  = $0800
        ifdef W5100
ZPSAVE   = $3A00                ; the machine's zero page, as the shim found it,
        else
ZPSAVE   = $3C00                ; the machine's zero page, as the shim found it,
        endif
                                ; on the page below the shim, which runs to $3FFF
PROGVERLO = $0803               ; the program's version, low byte

ROW0     = $0400                ; the title
ROW2     = $0500                ; the state

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
        .org $3B00
        else
        .org $3D00
        endif

        sei
        lda #$1E                ; $3D02: 8 data bits, 1 stop bit, 9,600 baud
        jmp start
speedtxt: .text "9600  "        ; $3D06: the speed, six characters
dialtxt: .ds 40                  ; $3D0C: the modem's dial command, or empty
zpsaved: .byte 0                 ; $3D34: nonzero once the machine's state is saved
oldreset: .byte 0, 0, 0          ; $3D35: the reset vector and its check byte
slot:   .byte 2                  ; $3D38: the 6551's slot

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

        lda dialtxt
        beq ready
        ldx #msgdial-msgs
        jsr drawstate
        ldx #0
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

ready:  ldx #msgwait-msgs
        jsr drawstate
        ldx #msgslot-msgs       ; a card's slot, or on a //c, its port
        lda $FBB3               ; a //c has $06 here and $00 at $FBC0
        eor #$06
        ora $FBC0
        bne rdport
        ldx #msgport-msgs
rdport: jsr shtext
        iny                     ; the row is blank, so a column skipped is a space
        lda slot
        ora #$B0
        sta ROW2,y
        iny
        iny
        ldx #msgat-msgs
        jsr shtext
        iny
        ldx #0
spdtext: lda speedtxt,x
        ora #$80
        sta ROW2,y
        iny
        inx
        cpx #6
        bne spdtext
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
        jmp PROGRAM

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


; drawstate: the state row cleared and the message at msgs+X drawn from column 0.
; Y is the column after it.

drawstate:   ldy #39
        lda #$A0
shclear: sta ROW2,y
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


title:  .text "CASSOLINK"
        .byte 0
msgs:
msgwait: .text "WAITING FOR THE PC "
        .byte 0
msgport: .text "ON PORT"
        .byte 0
msgslot: .text "IN SLOT"
        .byte 0
msgat:  .text "AT"
        .byte 0
msgload: .text "LOADING PROGRAM "
        .byte 0
msgstart: .text "STARTING VERSION "
        .byte 0
msgdial: .text "DIALING THE PC"
        .byte 0

; Ready with no program: version 0, the shim's version, no drives.
readypkt: .byte $C1, $04, $00, CMDREADY, $C1 ^ $04 ^ $00 ^ CMDREADY
          .byte $00, $00, SHIMVER, $00
          .byte $00 ^ $00 ^ SHIMVER ^ $00
readyend:

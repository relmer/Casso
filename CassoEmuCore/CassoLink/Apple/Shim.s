; Casso's CassoLink: the shim.
;
; Resident at $3D00, below the program's first table at $4000. It sets the
; //c's port 2 to its speed, sends Ready with no program loaded, and takes the
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

ACIADATA = $C0A8                ; port 2's 6551
ACIASTAT = $C0A9
ACIACMD  = $C0AA
ACIACTL  = $C0AB
RDRF     = $08                  ; status: a byte has arrived
TDRE     = $10                  ; status: the transmitter is free

CMDREADY = $D2
CMDLOAD  = $CF
CMDDATA  = $CA
ACK      = $06
NAK      = $15

PROGRAM  = $0800
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

        .org $3D00

        sei
        lda #$1E                ; $3D02: 8 data bits, 1 stop bit, 9,600 baud
        jmp start
speedtxt: .text "9600  "        ; $3D06: the speed, six characters
dialtxt: .ds 40                  ; $3D0C: the modem's dial command, or empty

start:  sta ACIACTL
        cld
        lda #$20
        sta slotx
        lda #$0B                ; DTR on, no parity, no interrupts
        sta ACIACMD
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
        jsr getb                ; the page number
        sta page
        adc #>PROGRAM-1         ; the carry is set from the compare
        sta ptr+1
        jsr getb                ; the sector field, not used
        ldy #0
        sty ptr
        sty crclo
        sty crchi
data:   jsr getb                ; stored as it arrives: at 115,200 baud a byte
        sta (ptr),y             ; comes every 89 cycles, too few for the CRC
        iny
        bne data
        jsr getb
        sta gotlo
        jsr getb
        sta gothi
        jsr getb                ; the payload's EOR; the CRC checks the page
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

getb:   lda ACIASTAT
        and #RDRF
        beq getb
        lda ACIADATA
        rts

putb:   pha
putwait: lda ACIASTAT
        and #TDRE
        beq putwait
        pla
        sta ACIADATA
        rts


title:  .text "CASSOLINK"
        .byte 0
msgs:
msgwait: .text "WAITING FOR THE PC ON PORT 2 AT "
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

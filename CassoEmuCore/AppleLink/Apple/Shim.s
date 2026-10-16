; Casso's Apple II link: the shim.
;
; Resident at $0300, in the 208 bytes below the page-3 vectors at $03D0. It
; sets the //c's port 2 to 9,600 baud, sends Ready with no program loaded,
; and takes the reply. Load program brings the program as Sector data
; packets, one page each, the page number in the track field: each page is
; stored from $0800 up and checked against its CRC-16/XMODEM, and the shim
; sends a bare ACK or NAK byte for it. After the last page the shim jumps to
; the program.
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

        .org $0300

        sei
        cld
        lda #$20
        sta slotx
        lda #$0B                ; DTR on, no parity, no interrupts
        sta ACIACMD
        lda #$1E                ; 8 data bits, 1 stop bit, 9,600 baud
        sta ACIACTL

ready:  ldx #0
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
data:   jsr getb
        sta (ptr),y
        jsr crcbyte
        iny
        bne data
        jsr getb
        sta gotlo
        jsr getb
        sta gothi
        jsr getb                ; the payload's EOR; the CRC checks the page

        lda #NAK
        ldx crclo
        cpx gotlo
        bne verdict
        ldx crchi
        cpx gothi
        bne verdict
        lda #ACK
verdict: jsr putb
        cmp #ACK
        bne loadpg              ; NAK: the host sends the page again
        ldx page
        inx
        cpx pages
        bne loadpg
        jmp PROGRAM


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


; Ready with no program: version 0, the shim's version, no drives.
readypkt: .byte $C1, $04, $00, CMDREADY, $C1 ^ $04 ^ $00 ^ CMDREADY
          .byte $00, $00, SHIMVER, $00
          .byte $00 ^ $00 ^ SHIMVER ^ $00
readyend:

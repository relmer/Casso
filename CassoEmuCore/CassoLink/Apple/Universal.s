; Casso's CassoLink: the universal boot disk's first stage.
;
; Loaded at $2000 with both shims after it: the 6551 shim at $2C00, as built
; for $3D00, and the W5100 shim at $2F00, as built for $3B00, each with its
; header written by the host. The stage finds every interface the machine
; has: a //c's ports 1 and 2; a Super Serial Card in any slot, by its
; firmware's identification bytes; and an Uthernet II in any other slot but
; a Disk II's, by its W5100 taking and returning its address pointer.
;
; Each 6551 is set to the shim's speed. The W5100 is given an address by
; DHCP, or the header's when no DHCP reply comes, listens on the header's
; port, and broadcasts an announcement to UDP port 6510 about every two
; seconds: CASSOLINK, a version byte, its address, its port and its MAC
; address. Casso Explorer lists the Apple of each announcement.
;
; The stage then waits for the PC on all of them at once. An envelope's start
; byte on a 6551, or a connection to the W5100, chooses that interface: its
; shim is copied to its address with the slot in its header, and started.
; The machine's zero page and reset vector are saved first, as a shim saves
; them, and the chosen shim is given them with its zpsaved byte set. A shim
; with a modem's dial command is started at once on the last 6551 found, a
; //c's port 2 or the card in the highest slot, since no connection from the
; PC is possible before the modem dials it.
;
; NMOS 6502 code throughout.

ACIMG    = $2C00                ; the 6551 shim, as built for $3D00
W5IMG    = $2F00                ; the W5100 shim, as built for $3B00
ACSHIM   = $3D00
W5SHIM   = $3B00
ACZPSAVE = $3C00                ; each shim's copy of the machine's zero page
W5ZPSAVE = $3A00
ZPCOPY   = $3400                ; the stage's, until a shim is chosen

HCTL     = $02                  ; a shim's header: the 6551 control byte,
HSPEED   = $06                  ; the speed's text, six characters,
HDIAL    = $0C                  ; the dial command, or the W5100's network:
NADDR    = HDIAL                ; address,
NMASK    = HDIAL + 4            ; subnet mask,
NGW      = HDIAL + 8            ; gateway,
NMAC     = HDIAL + 12           ; MAC address,
NPORT    = HDIAL + 18           ; and port, high byte first;
HZPSAVED = $34                  ; nonzero once the machine's state is saved,
HRESET   = $35                  ; the reset vector and its check byte,
HSLOT    = $38                  ; and the slot

ACIADATA = $C088                ; the 6551, + slot * 16
ACIASTAT = $C089
ACIACMD  = $C08A
ACIACTL  = $C08B
RDRF     = $08

WZMR     = $C084                ; the W5100, + slot * 16
WZHI     = $C085
WZLO     = $C086
WZDATA   = $C087
SKMR     = $00                  ; a socket's registers
SKCR     = $01
SKIR     = $02
SKSR     = $03
SKPORT   = $04
SKDIPR   = $0C
SKTXWR   = $24
SKRXRSR  = $26
SKRXRD   = $28
CMOPEN   = $01
CMLISTEN = $02
CMCLOSE  = $10
CMSEND   = $20
CMRECV   = $40
STLISTEN = $14
STSYNRCV = $16
STESTAB  = $17
IRSENT   = $18                  ; SEND_OK or TIMEOUT
ANNPORT  = 6510                 ; where the announcements go,
ANNFROM  = 6511                 ; and the port they come from: in Casso, a
                                ; port the guest opens is opened on the PC
ANNWAIT  = $50                  ; wait loops between them, in 256s
DHWAIT   = $A0                  ; polls for a DHCP reply, in 256s

ptr      = $E0                  ; and $E1
slotx    = $E2                  ; the W5100's slot * 16
nacia    = $E3                  ; 6551s found
w5slot   = $E4                  ; the W5100's slot, or 0
scslot   = $E5                  ; the slot being looked at
count    = $E6
tmolo    = $E7
tmohi    = $E8
offlo    = $E9                  ; where the next byte is, as the socket counts
offhi    = $EA
remlo    = $EB                  ; a datagram's bytes not yet read
remhi    = $EC
optcode  = $ED
optlen   = $EE
msgtype  = $EF                  ; a DHCP reply's type
tries    = $F0
annlo    = $F1                  ; wait loops since the last announcement
annhi    = $F2
rowlo    = $F3                  ; the row being drawn, and $F4
rowhi    = $F4
sel      = $F5                  ; the chosen 6551, by its index
skpage   = $F6                  ; the socket's registers' page
txbase   = $F7                  ; its transmit memory's page
savx     = $F8
savy     = $F9
num      = $FA
digit    = $FB
lead     = $FC
dhtype   = $FD                  ; the DHCP message being sent
good     = $FE                  ; nonzero for a DHCP reply to this Apple
column   = $FF                  ; where the W5100's row goes on

        .org $2000

        sei
        cld
        ldx #0
svzp:   lda $00,x
        sta ZPCOPY,x
        inx
        bne svzp
        ldx #2
svrst:  lda $03F2,x
        sta ACIMG+HRESET,x
        sta W5IMG+HRESET,x
        dex
        bpl svrst
        lda #1
        sta ACIMG+HZPSAVED
        sta W5IMG+HZPSAVED
        lda #0
        sta nacia
        sta w5slot
        jsr home

        lda $FBB3               ; a //c has $06 here and $00 at $FBC0
        eor #$06
        ora $FBC0
        bne scan
        lda #1
        jsr addacia
        lda #2
        jsr addacia
        jmp found

scan:   lda #1
        sta scslot
scloop: jsr isssc
        bcc scnot
        lda scslot
        jsr addacia
        jmp scnext
scnot:  jsr isdisk
        bcs scnext
        lda w5slot
        bne scnext
        jsr isw5100
        bcc scnext
        lda scslot
        sta w5slot
scnext: inc scslot
        lda scslot
        cmp #8
        bne scloop
        lda $CFFF               ; no card's expansion ROM left selected

found:  lda nacia
        ora w5slot
        bne foundany
        ldx #msgnone-msgs
        jsr drawstate
hang:   jmp hang

foundany: lda ACIMG+HDIAL       ; a modem's dial command: the last 6551, now
        beq showall
        ldx nacia
        beq showall
        dex
        stx sel
        jmp acgo

showall: ldx #msgwait-msgs
        jsr drawstate
        ldx #0
shacia: cpx nacia
        beq shw5
        stx sel
        txa
        jsr rowat
        ldx #msgslot-msgs       ; a card's slot, or on a //c, its port
        lda $FBB3
        eor #$06
        ora $FBC0
        bne shname
        ldx #msgport-msgs
shname: ldy #0
        jsr puttext
        ldx sel
        lda acslots,x
        jsr putdec
        ldx #msgat-msgs
        jsr puttext
        ldx #0
shspd:  lda ACIMG+HSPEED,x
        ora #$80
        sta (rowlo),y
        iny
        inx
        cpx #6
        bne shspd
        ldx sel
        inx
        bne shacia

shw5:   lda w5slot
        bne w5show
        jmp wait
w5show: lda nacia
        jsr rowat
        ldx #msgslot-msgs
        ldy #0
        jsr puttext
        lda w5slot
        jsr putdec
        sty column
        ldx #msgdhcp-msgs
        jsr puttext
        jsr w5up
        lda nacia
        jsr rowat
        ldy column
        ldx #msgat-msgs
        jsr puttext
        ldx #0
w5ip:   lda W5IMG+NADDR,x
        stx sel
        jsr putdec
        ldx sel
        inx
        cpx #4
        beq w5port
        lda #$AE                ; a period
        sta (rowlo),y
        iny
        bne w5ip
w5port: lda #$BA                ; a colon
        sta (rowlo),y
        iny
        jsr putport
        lda #$A0                ; the rest of the row, blank
w5blank: cpy #40
        beq wait
        sta (rowlo),y
        iny
        bne w5blank

; wait: until an envelope's start byte arrives on a 6551, or the PC connects
; to the W5100. A W5100 socket that stops listening listens again.
wait:   ldx #0
wtacia: cpx nacia
        beq wtw5
        lda acslots,x
        asl a
        asl a
        asl a
        asl a
        tay
        lda ACIASTAT,y
        and #RDRF
        beq wtnext
        lda ACIADATA,y
        cmp #$C1
        bne wtnext
        stx sel
        jmp acgo
wtnext: inx
        bne wtacia
wtw5:   lda w5slot
        beq wait
        lda #$04
        sta skpage
        ldy #SKSR
        jsr wzsock
        jsr wzget
        cmp #STESTAB
        beq w5go
        cmp #STLISTEN
        beq wtann
        cmp #STSYNRCV
        beq wtann
        jsr w5listen
wtann:  inc annlo
        bne wait
        inc annhi
        lda annhi
        cmp #ANNWAIT
        bne wait
        lda #0
        sta annhi
        jsr announce
        jmp wait

; acgo: the 6551 shim, for the 6551 at sel, given the zero page and started.
acgo:   ldx sel
        lda acslots,x
        sta ACIMG+HSLOT
        ldx #0
acgocopy: lda ZPCOPY,x
        sta ACZPSAVE,x
        lda ACIMG,x
        sta ACSHIM,x
        lda ACIMG+$100,x
        sta ACSHIM+$100,x
        lda ACIMG+$200,x
        sta ACSHIM+$200,x
        inx
        bne acgocopy
        jmp ACSHIM

; w5go: the W5100 shim, with the address in use, given the zero page and
; started. The PC is connected already, and the shim goes on with that
; connection.
w5go:   lda w5slot
        sta W5IMG+HSLOT
        ldx #0
w5gocopy: lda ZPCOPY,x
        sta W5ZPSAVE,x
        lda W5IMG,x
        sta W5SHIM,x
        lda W5IMG+$100,x
        sta W5SHIM+$100,x
        lda W5IMG+$200,x
        sta W5SHIM+$200,x
        lda W5IMG+$300,x
        sta W5SHIM+$300,x
        lda W5IMG+$400,x
        sta W5SHIM+$400,x
        inx
        bne w5gocopy
        jmp W5SHIM


; addacia: the 6551 in slot A listed, and set to the shim's speed with DTR
; on. A byte already waiting in it is read and dropped.
addacia: ldx nacia
        sta acslots,x
        inc nacia
        asl a
        asl a
        asl a
        asl a
        tay
        lda ACIMG+HCTL
        sta ACIACTL,y
        lda #$0B
        sta ACIACMD,y
        lda ACIADATA,y
        rts


; isssc, isdisk and isw5100: the carry set when scslot has a Super Serial
; Card, a Disk II, or a W5100.
isssc:  jsr romptr
        ldy #$05
        lda (ptr),y
        cmp #$38
        bne no
        ldy #$07
        lda (ptr),y
        cmp #$18
        bne no
        ldy #$0B
        lda (ptr),y
        cmp #$01
        bne no
        ldy #$0C
        lda (ptr),y
        cmp #$31
        bne no
yes:    sec
        rts
no:     clc
        rts

isdisk: jsr romptr
        ldy #$01
        lda (ptr),y
        cmp #$20
        bne no
        ldy #$03
        lda (ptr),y
        cmp #$00
        bne no
        ldy #$05
        lda (ptr),y
        cmp #$03
        bne no
        beq yes

isw5100: lda scslot
        asl a
        asl a
        asl a
        asl a
        tax
        lda WZMR,x              ; read first: a W5100's mode register has
        and #$64                ; bits 6, 5 and 2 clear, and a card that is
        bne no                  ; not one is not written to
        lda #$03                ; indirect, with the pointer advancing
        sta WZMR,x
        lda #$55
        sta WZHI,x
        lda #$AA
        sta WZLO,x
        lda WZHI,x
        cmp #$55
        bne no
        lda WZLO,x
        cmp #$AA
        bne no
        lda #$2A
        sta WZHI,x
        lda #$55
        sta WZLO,x
        lda WZHI,x
        cmp #$2A
        bne no
        lda WZLO,x
        cmp #$55
        bne no
        beq yes

romptr: lda #0
        sta ptr
        lda scslot
        ora #$C0
        sta ptr+1
        rts


; macsalt: the header's MAC address made this Apple's: RNDL and RNDH, which
; the monitor counts up while it waits for a key, and a 16-bit sum of
; $4000-$BFFF, which differs between machines and power-ons, added into its
; last three bytes. Copies of one disk on two Apples then have two
; addresses. The first three bytes, with the locally administered bit, are
; left as they are.
macsalt: lda #0
        sta tmolo
        sta tmohi
        sta ptr
        lda #$40
        sta ptr+1
        ldy #0
mssum:  lda (ptr),y
        clc
        adc tmolo
        sta tmolo
        bcc msnext
        inc tmohi
msnext: iny
        bne mssum
        inc ptr+1
        lda ptr+1
        cmp #$C0
        bne mssum
        clc
        lda W5IMG+NMAC+3
        adc $4E                 ; RNDL
        sta W5IMG+NMAC+3
        lda W5IMG+NMAC+4
        adc $4F                 ; RNDH
        sta W5IMG+NMAC+4
        lda W5IMG+NMAC+5
        adc tmolo
        sta W5IMG+NMAC+5
        lda W5IMG+NMAC+4
        eor tmohi
        sta W5IMG+NMAC+4
        rts


; w5up: the W5100 reset and set up: the header's MAC address, salted, 8 KB
; of receive memory and 4 KB of transmit memory to socket 0 and 4 KB of
; transmit memory to socket 1, an address from DHCP, socket 0 listening on
; the header's port and socket 1 open for the announcements.
w5up:   jsr macsalt
        lda w5slot
        asl a
        asl a
        asl a
        asl a
        sta slotx
        ora #<WZDATA            ; each write of the data is absolute, as an
        sta wzput+1             ; indexed store reads it first, which would
        ldx slotx               ; advance the pointer
        lda #$80
        sta WZMR,x
w5reset: lda WZMR,x
        bmi w5reset
        lda #$03
        sta WZMR,x
        lda #$00
        ldy #$09                ; SHAR
        jsr wzptr
        ldx #0
w5mac:  lda W5IMG+NMAC,x
        jsr wzput
        inx
        cpx #6
        bne w5mac
        lda #$00
        ldy #$1A                ; RMSR and TMSR
        jsr wzptr
        lda #$03
        jsr wzput
        lda #$0A
        jsr wzput
        jsr dhcp
        lda #$00
        ldy #$01                ; GAR, SUBR, SHAR and SIPR
        jsr wzptr
        ldx #0
w5cfg:  ldy cfgtab,x
        lda W5IMG+HDIAL,y
        jsr wzput
        inx
        cpx #cfgend-cfgtab
        bne w5cfg
        jsr w5listen

        lda #$05                ; socket 1: UDP, to port 6510 everywhere
        sta skpage
        lda #CMCLOSE
        jsr wzcmd
        ldy #SKMR
        jsr wzsock
        lda #$02
        jsr wzput
        ldy #SKPORT
        jsr wzsock
        lda #>ANNFROM
        jsr wzput
        lda #<ANNFROM
        jsr wzput
        ldy #SKDIPR
        jsr wzsock
        jsr broadcast
        lda #>ANNPORT
        jsr wzput
        lda #<ANNPORT
        jsr wzput
        lda #CMOPEN
        jmp wzcmd

; w5listen: socket 0 closed, opened for TCP on the header's port, and
; listening.
w5listen: lda #$04
        sta skpage
        lda #CMCLOSE
        jsr wzcmd
        ldy #SKMR
        jsr wzsock
        lda #$01
        jsr wzput
        ldy #SKPORT
        jsr wzsock
        lda W5IMG+NPORT
        jsr wzput
        lda W5IMG+NPORT+1
        jsr wzput
        lda #CMOPEN
        jsr wzcmd
        lda #CMLISTEN
        jmp wzcmd

; broadcast: 255.255.255.255 at the pointer.
broadcast: lda #$FF
        jsr wzput
        jsr wzput
        jsr wzput
        jmp wzput

; announce: CASSOLINK, the version, the address, the port and the MAC
; address, from socket 1.
announce: lda #$05
        sta skpage
        lda #$50
        sta txbase
        jsr txbegin
        ldx #0
antag:  lda anntag,x
        jsr txbyte
        inx
        cpx #annend-anntag
        bne antag
        ldx #0
anaddr: lda W5IMG+NADDR,x
        jsr txbyte
        inx
        cpx #4
        bne anaddr
        lda W5IMG+NPORT
        jsr txbyte
        lda W5IMG+NPORT+1
        jsr txbyte
        ldx #0
anmac:  lda W5IMG+NMAC,x
        jsr txbyte
        inx
        cpx #6
        bne anmac
        jmp txsend


; dhcp: DISCOVER, then for an OFFER, REQUEST, and for an ACK, the address,
; mask and gateway into the W5100 shim's header; three times at most. With
; no ACK the header's own address is used. Socket 0 sends from port 68 to
; port 67 everywhere, and is closed after.
dhcp:   ldx #3
dhdef:  lda W5IMG+NMASK,x       ; the mask and gateway, for a reply with none
        sta smask,x
        lda W5IMG+NGW,x
        sta sgw,x
        dex
        bpl dhdef
        lda #$04
        sta skpage
        lda #$40
        sta txbase
        lda #CMCLOSE
        jsr wzcmd
        ldy #SKMR
        jsr wzsock
        lda #$02                ; UDP
        jsr wzput
        ldy #SKPORT
        jsr wzsock
        lda #0
        jsr wzput
        lda #68
        jsr wzput
        ldy #SKDIPR
        jsr wzsock
        jsr broadcast
        lda #0
        jsr wzput
        lda #67
        jsr wzput
        lda #CMOPEN
        jsr wzcmd
        lda #3
        sta tries
dhtry:  lda #1                  ; DISCOVER
        sta dhtype
        jsr dhsend
        jsr dhrecv
        bcs dhagain
        lda msgtype
        cmp #2                  ; OFFER
        bne dhagain
        lda #3                  ; REQUEST
        sta dhtype
        jsr dhsend
        jsr dhrecv
        bcs dhagain
        lda msgtype
        cmp #5                  ; ACK
        bne dhagain
        ldx #3
dhkeep: lda yip,x
        sta W5IMG+NADDR,x
        lda smask,x
        sta W5IMG+NMASK,x
        lda sgw,x
        sta W5IMG+NGW,x
        dex
        bpl dhkeep
        bmi dhclose
dhagain: dec tries
        bne dhtry
dhclose: lda #CMCLOSE
        jmp wzcmd

; dhsend: a DHCP message of type dhtype, broadcast. The transaction ID is
; the MAC address's last four bytes, and a REQUEST has options 50 and 54,
; the offered address and the server that offered it. Padded to 300 bytes or
; more.
dhsend: jsr txbegin
        ldx #0
dsfix:  lda dhhead,x            ; op, htype, hlen, hops
        jsr txbyte
        inx
        cpx #4
        bne dsfix
        ldx #0
dsxid:  lda W5IMG+NMAC+2,x
        jsr txbyte
        inx
        cpx #4
        bne dsxid
        lda #0                  ; secs
        jsr txbyte
        jsr txbyte
        lda #$80                ; flags: reply by broadcast
        jsr txbyte
        lda #1+16               ; the flags' low byte and the four addresses
        jsr zeros
        ldx #0
dsmac:  lda W5IMG+NMAC,x
        jsr txbyte
        inx
        cpx #6
        bne dsmac
        lda #10+64+128          ; the rest of chaddr, sname and file
        jsr zeros
        ldx #0
dsopt:  lda dhopts,x            ; the magic cookie, then option 53
        jsr txbyte
        inx
        cpx #dhoptend-dhopts
        bne dsopt
        lda dhtype
        jsr txbyte
        cmp #3
        bne dsparm
        lda #50                 ; option 50, Requested IP Address
        jsr txbyte
        lda #4
        jsr txbyte
        ldx #0
dsreq:  lda yip,x
        jsr txbyte
        inx
        cpx #4
        bne dsreq
        lda #54                 ; option 54, Server Identifier
        jsr txbyte
        lda #4
        jsr txbyte
        ldx #0
dssrv:  lda ssrv,x
        jsr txbyte
        inx
        cpx #4
        bne dssrv
dsparm: ldx #0
dspl:   lda dhparm,x            ; option 55, mask and router, then the end
        jsr txbyte
        inx
        cpx #dhparmend-dhparm
        bne dspl
        lda #52
        jsr zeros
        jmp txsend

; zeros: A zero bytes into the transmit memory.
zeros:  sta count
zrloop: lda #0
        jsr txbyte
        dec count
        bne zrloop
        rts

; dhrecv: the next DHCP reply to this Apple, within about three seconds: a
; reply whose transaction ID is this Apple's. Its type in msgtype, its
; address in yip and its mask, router and server, when it has them. The
; carry is set when none comes; other datagrams are dropped.
dhrecv: lda #0
        sta tmolo
        sta tmohi
drwait: ldy #SKRXRSR
        jsr wzsock
        jsr wzget
        sta remhi
        jsr wzget
        ora remhi
        bne drgot
        inc tmolo
        bne drwait
        inc tmohi
        lda tmohi
        cmp #DHWAIT
        bne drwait
        sec
        rts
drgot:  ldy #SKRXRD
        jsr wzsock
        jsr wzget
        sta offhi
        jsr wzget
        sta offlo
        lda #0
        sta good
        sta msgtype
        ldx #6                  ; the sender's address and port
        jsr rxskip
        jsr rxbyte              ; the datagram's size
        sta remhi
        jsr rxbyte
        sta remlo
        jsr rxdata              ; op: a reply
        cmp #2
        bne drdone
        ldx #3
        jsr rxdskip
        ldx #0
drxid:  jsr rxdata
        cmp W5IMG+NMAC+2,x
        bne drdone
        inx
        cpx #4
        bne drxid
        ldx #8                  ; secs, flags and ciaddr
        jsr rxdskip
        ldx #0
dryip:  jsr rxdata
        sta yip,x
        inx
        cpx #4
        bne dryip
        ldx #4+4+16+64          ; siaddr, giaddr, chaddr and sname
        jsr rxdskip
        ldx #128+4              ; file and the magic cookie
        jsr rxdskip
        inc good
dropt:  lda remlo
        ora remhi
        beq drdone
        jsr rxdata
        sta optcode
        cmp #255
        beq drdone
        cmp #0
        beq dropt
        jsr rxdata
        sta optlen
        ldx #0
drodata: cpx optlen
        beq dropt
        jsr rxdata
        ldy optcode
        cpy #53
        bne drmask
        sta msgtype
drmask: cpy #1
        bne drgw
        cpx #4
        bcs drnext
        sta smask,x
drgw:   cpy #3
        bne drsrv
        cpx #4
        bcs drnext
        sta sgw,x
drsrv:  cpy #54
        bne drnext
        cpx #4
        bcs drnext
        sta ssrv,x
drnext: inx
        bne drodata

; drdone: the datagram given back to the chip, whatever of it was not read
; skipped.
drdone: clc
        lda offlo
        adc remlo
        sta offlo
        lda offhi
        adc remhi
        sta offhi
        ldy #SKRXRD
        jsr wzsock
        lda offhi
        jsr wzput
        lda offlo
        jsr wzput
        lda #CMRECV
        jsr wzcmd
        lda good
        bne drgood
        jmp drwait
drgood: clc
        rts

; rxskip and rxdskip: X bytes read and dropped, the second counting them
; against the datagram's size. X is 0 after.
rxskip: jsr rxbyte
        dex
        bne rxskip
        rts

rxdskip: jsr rxdata
        dex
        bne rxdskip
        rts

; rxdata: rxbyte, counted against the datagram's size.
rxdata: jsr rxbyte
        pha
        lda remlo
        bne rxdcount
        dec remhi
rxdcount: dec remlo
        pla
        rts

; rxbyte: the byte at offlo and offhi in socket 0's receive memory, at
; $6000 + (offset & $1FFF), and the offset advanced. X and Y are restored.
rxbyte: stx savx
        sty savy
        lda offhi
        and #$1F
        ora #$60
        ldy offlo
        jsr wzptr
        jsr wzget
        inc offlo
        bne rxbdone
        inc offhi
rxbdone: ldx savx
        ldy savy
        rts

; txbegin: offlo and offhi set to the socket's Sn_TX_WR.
txbegin: ldy #SKTXWR
        jsr wzsock
        jsr wzget
        sta offhi
        jsr wzget
        sta offlo
        rts

; txbyte: A into the socket's transmit memory at txbase * 256 +
; (offset & $0FFF), and the offset advanced. X and Y are restored.
txbyte: stx savx
        sty savy
        pha
        lda offhi
        and #$0F
        ora txbase
        ldy offlo
        jsr wzptr
        pla
        pha
        jsr wzput
        inc offlo
        bne txbdone
        inc offhi
txbdone: pla
        ldx savx
        ldy savy
        rts

; txsend: Sn_TX_WR at the offset, SEND, and the wait until the chip has sent
; it or given up.
txsend: ldy #SKTXWR
        jsr wzsock
        lda offhi
        jsr wzput
        lda offlo
        jsr wzput
        lda #CMSEND
        jsr wzcmd
tswait: ldy #SKIR
        jsr wzsock
        jsr wzget
        and #IRSENT
        beq tswait
        ldy #SKIR
        jsr wzsock
        lda #IRSENT
        jmp wzput

; wzcmd: the command in A to the socket, and the wait until the chip takes
; it.
wzcmd:  pha
        ldy #SKCR
        jsr wzsock
        pla
        jsr wzput
wcwait: ldy #SKCR
        jsr wzsock
        jsr wzget
        bne wcwait
        rts

; wzsock: the pointer at the socket's register Y.
wzsock: lda skpage

; wzptr: the pointer at A (high) and Y (low). X is slotx.
wzptr:  ldx slotx
        sta WZHI,x
        tya
        sta WZLO,x
        rts

; wzget: the byte at the pointer, which then advances. X is slotx.
wzget:  ldx slotx
        lda WZDATA,x
        rts

; wzput: A at the pointer, which then advances; w5up sets the address.
wzput:  sta WZDATA
        rts


; home: the text page cleared and the title drawn.
home:   lda #$A0
        ldy #0
hmclear: sta $0400,y
        sta $0500,y
        sta $0600,y
        sta $0700,y
        iny
        bne hmclear
        lda #<$0400
        sta rowlo
        lda #>$0400
        sta rowhi
        ldx #title-msgs
        jmp puttext

; drawstate: the message at msgs+X on the third row.
drawstate: lda #<$0500
        sta rowlo
        lda #>$0500
        sta rowhi
        ldy #0

; puttext: the message at msgs+X on the row, from column Y. Y is the column
; after it.
puttext: lda msgs,x
        beq ptdone
        ora #$80
        sta (rowlo),y
        iny
        inx
        bne puttext
ptdone: rts

; rowat: the row for interface A, the fifth row and down.
rowat:  tax
        lda rowtablo,x
        sta rowlo
        lda rowtabhi,x
        sta rowhi
        rts

; putdec: A in decimal, without leading zeros, on the row from Y.
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
        sta (rowlo),y
        sta lead
        iny
pdnext: dex
        bpl pdloop
        rts

pdtab:  .byte 1, 10, 100

; putport: the W5100's port in decimal, without leading zeros, on the row
; from Y.
putport: lda W5IMG+NPORT
        sta tmohi
        lda W5IMG+NPORT+1
        sta tmolo
        lda #0
        sta lead
        ldx #4
ppdigit: lda #0
        sta digit
ppsub:  lda tmolo
        sec
        sbc pptablo,x
        sta num
        lda tmohi
        sbc pptabhi,x
        bcc ppout
        sta tmohi
        lda num
        sta tmolo
        inc digit
        bne ppsub
ppout:  lda digit
        ora lead
        bne ppput
        cpx #0
        bne ppnext
ppput:  lda digit
        ora #$B0
        sta (rowlo),y
        sta lead
        iny
ppnext: dex
        bpl ppdigit
        rts

pptablo: .byte <1, <10, <100, <1000, <10000
pptabhi: .byte >1, >10, >100, >1000, >10000

rowtablo: .byte <$0600, <$0680, <$0700, <$0780, <$0428, <$04A8, <$0528, <$05A8
rowtabhi: .byte >$0600, >$0680, >$0700, >$0780, >$0428, >$04A8, >$0528, >$05A8

; The W5100 shim's network, in the order of the chip's registers from
; $0001: GAR, SUBR, SHAR and SIPR.
cfgtab: .byte 8, 9, 10, 11, 4, 5, 6, 7, 12, 13, 14, 15, 16, 17, 0, 1, 2, 3
cfgend:

dhhead: .byte 1, 1, 6, 0
dhopts: .byte $63, $82, $53, $63, 53, 1
dhoptend:
dhparm: .byte 55, 2, 1, 3, 255
dhparmend:

anntag: .text "CASSOLINK"
        .byte 1
annend:

msgs:
title:  .text "CASSOLINK"
        .byte 0
msgwait: .text "WAITING FOR THE PC ON:"
        .byte 0
msgnone: .text "NO SERIAL PORT OR CARD FOUND"
        .byte 0
msgport: .text "PORT "
        .byte 0
msgslot: .text "SLOT "
        .byte 0
msgat:  .text " AT "
        .byte 0
msgdhcp: .text " GETTING AN ADDRESS"
        .byte 0

acslots: .ds 8                  ; each 6551's slot, or a //c's port
yip:    .ds 4                   ; the address offered
smask:  .ds 4                   ; its mask
sgw:    .ds 4                   ; its router
ssrv:   .ds 4                   ; the server that offered it

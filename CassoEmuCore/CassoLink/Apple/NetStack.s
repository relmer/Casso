; Casso's CassoLink: the network shim's stack, assembled after Shim.s in its
; network build, at $B000.
;
; An original Uthernet has a CS8900A, and a LANceGS a LAN91C96, each with its
; 16 registers at $C080 + slot * 16. Neither chip has a network stack, so the
; one here has ARP replies, IPv4 and UDP, DHCP, and the announcement Casso
; Explorer finds the Apple by. The shim's header has the card's slot and
; netchip, and from $B00C the address, subnet mask, gateway, MAC address and
; UDP port, high byte first. An address of 0.0.0.0 is taken from a DHCP
; server.
;
; One frame is received at a time, into RXFRM, and one sent, from TXFRM. The
; link's datagrams go to the address, port and MAC address of the last one
; received, so no ARP request is sent, and a datagram to a PC behind a
; router goes to the router's MAC address. A datagram's payload is the link's bytes
; as a serial line has them; a lost datagram is a run of lost bytes, which
; the link's CRCs and timeouts recover from.
;
; No zero page is used, so the program's is left alone: the frames are
; addressed by self-modified absolute operands. Each write to a card's data
; register is absolute too, since an indexed store reads the register first,
; which would advance the chip's pointer.

RXFRM    = $9600                ; the frame received, to $9BFF
TXFRM    = $9C00                ; the frame to send, to $A1FF
RXDATA   = RXFRM + 42           ; a UDP datagram's payload
TXDATA   = TXFRM + 42
DHOPTS   = RXDATA + 240         ; a DHCP reply's options
LINKMAX  = $0400                ; the most bytes a datagram of the link has
NCCS     = 1                    ; netchip: a CS8900A
NCLAN    = 2                    ; a LAN91C96
NSLINK   = 1                    ; nspoll: a datagram of the link
NSDHCP   = 2                    ; a datagram to the DHCP client's port
NSROUNDS = 12000                ; nwaitk's rounds in about a second at 1 MHz
ANNVER   = 2                    ; the announcement's version: UDP

; The CS8900A, + slot * 16. A 16-bit register is two bytes, low byte first.
CSRT0    = $C080                ; receive and transmit data
CSRT1    = $C081
CSTXCMD  = $C084                ; TxCMD
CSTXLEN  = $C086                ; TxLength
CSPPTR   = $C08A                ; the PacketPage pointer
CSPDATA  = $C08C                ; the PacketPage data

; The LAN91C96, + slot * 16, in bank 2 unless a comment has another.
LNMMU    = $C080                ; the MMU command, and bit 0 set while busy
LNPNR    = $C082                ; the packet number
LNARR    = $C083                ; the allocation result, bit 7 set on a failure
LNRXFIFO = $C085                ; the receive FIFO, bit 7 set when empty
LNPTR    = $C086                ; the pointer
LNDATA   = $C088                ; the data at the pointer, which then advances
LNIST    = $C08C                ; the interrupts, ALLOC_INT in bit 3
LNBANK   = $C08E                ; the bank select

; nsinit: the card set up for the header's MAC address, receiving frames to
; it and broadcast frames.
nsinit: lda slot
        asl a
        asl a
        asl a
        asl a
        sta nsx
        ora #<CSRT0
        sta cswr0+1
        ora #<CSRT1
        sta cswr1+1
        lda nsx
        ora #<LNDATA
        sta lnwr+1
        lda netchip
        cmp #NCLAN
        bne nsics
        jmp lninit
nsics:  jmp csinit

; nsrx: a frame into RXFRM, its length in rxflen, with the carry clear; the
; carry is set when no frame has come.
nsrx:   lda netchip
        cmp #NCLAN
        bne nsrxcs
        jmp lnrx
nsrxcs: jmp csrx

; nstx: TXFRM's first txflen bytes sent as a frame.
nstx:   lda netchip
        cmp #NCLAN
        bne nstxcs
        jmp lntx
nstxcs: jmp cstx

; nsend: the X (low) and Y (high) bytes at TXDATA sent as a datagram to the
; PC, from the header's port.
nsend:  stx plen
        sty plen+1
        ldx #5
nsdmac: lda pmac,x
        sta dmac,x
        dex
        bpl nsdmac
        ldx #3
nsdip:  lda pip,x
        sta dip,x
        lda dialtxt,x
        sta sip,x
        dex
        bpl nsdip
        lda pport
        sta dport
        lda pport+1
        sta dport+1
        lda dialtxt+18
        sta sport
        lda dialtxt+19
        sta sport+1
        ; falls into udpsend

; udpsend: the plen bytes at TXDATA sent as a datagram from sip and sport
; to dmac, dip and dport.
udpsend: ldx #5
usmac:  lda dmac,x
        sta TXFRM,x
        lda dialtxt+12,x
        sta TXFRM+6,x
        dex
        bpl usmac
        ldx #11
ushdr:  lda iphdr,x
        sta TXFRM+12,x
        dex
        bpl ushdr
        ldx #3
usip:   lda sip,x
        sta TXFRM+26,x
        lda dip,x
        sta TXFRM+30,x
        dex
        bpl usip
        lda sport
        sta TXFRM+34
        lda sport+1
        sta TXFRM+35
        lda dport
        sta TXFRM+36
        lda dport+1
        sta TXFRM+37
        lda #0
        sta TXFRM+24
        sta TXFRM+25
        sta TXFRM+40            ; no UDP checksum
        sta TXFRM+41
        lda plen                ; the UDP length: the payload and 8
        clc
        adc #8
        sta TXFRM+39
        lda plen+1
        adc #0
        sta TXFRM+38
        lda TXFRM+39            ; the IP length: 20 more
        clc
        adc #20
        sta TXFRM+17
        lda TXFRM+38
        adc #0
        sta TXFRM+16
        lda TXFRM+17            ; the frame's: 14 more
        clc
        adc #14
        sta txflen
        lda TXFRM+16
        adc #0
        sta txflen+1
        jsr ipsum
        jmp nstx

; The frame's type, then the IP header's version and length, service, total
; length, identification, fragment field, time to live and protocol.
iphdr:  .byte $08, $00, $45, $00, 0, 0, 0, 0, 0, 0, $40, $11

; ipsum: TXFRM's IP header checksum, the ones' complement of the ones'
; complement sum of its 10 words.
ipsum:  lda #0
        sta sumlo
        sta sumhi
        tax
isword: lda sumlo
        clc
        adc TXFRM+15,x
        sta sumlo
        lda sumhi
        adc TXFRM+14,x
        sta sumhi
        bcc isnext
        inc sumlo               ; the carry added back in
        bne isnext
        inc sumhi
isnext: inx
        inx
        cpx #20
        bne isword
        lda sumhi
        eor #$FF
        sta TXFRM+24
        lda sumlo
        eor #$FF
        sta TXFRM+25
        rts

; bcast: dmac and dip set to the broadcast addresses.
bcast:  lda #$FF
        ldx #5
bcmac:  sta dmac,x
        dex
        bpl bcmac
        ldx #3
bcip:   sta dip,x
        dex
        bpl bcip
        rts

; nrecv: a datagram of the link within A seconds, its payload at RXDATA and
; its length in rxlen, with the carry clear; the carry is set on a timeout.
; With A at 0 the card is checked once. Meanwhile an ARP reply is sent for
; each ARP request for the address, and other frames are dropped.
nrecv:  ldx #NSLINK
        ; falls into nwaitk

; nwaitk: nrecv for a datagram of the kind in X, NSLINK or NSDHCP.
nwaitk: stx nswant
        sta nsecs
nwsec:  lda #<NSROUNDS
        sta nrlo
        lda #>NSROUNDS
        sta nrhi
nwpoll: jsr nspoll
        cmp nswant
        beq nwgot
        lda nsecs
        beq nwnone
        lda nrlo
        bne nwdec
        dec nrhi
nwdec:  dec nrlo
        lda nrlo
        ora nrhi
        bne nwpoll
        dec nsecs
        bne nwsec
nwnone: sec
        rts
nwgot:  clc
        rts

; nspoll: a frame received and handled. A is NSLINK for a datagram of the
; link, whose sender is then the PC, NSDHCP for one to port 68, and 0 for
; none or any other frame.
nspoll: jsr nsrx
        bcc npframe
npnone: lda #0
        rts
npframe: lda RXFRM+12
        cmp #$08
        bne npnone
        lda RXFRM+13
        beq npip
        cmp #$06
        bne npnone
        jmp nparp
npip:   lda RXFRM+14            ; IPv4 with no options
        cmp #$45
        bne npnone
        lda RXFRM+23            ; UDP
        cmp #$11
        bne npnone
        lda RXFRM+20            ; not a fragment: no more-fragments flag and
        and #$3F                ; no offset
        ora RXFRM+21
        bne npnone
        lda RXFRM+38            ; the payload's length, the UDP length less 8
        sta rxlen+1
        lda RXFRM+39
        sec
        sbc #8
        sta rxlen
        bcs nplen
        dec rxlen+1
        bmi npnone              ; a UDP length under 8
nplen:  lda rxflen              ; and within the frame
        sec
        sbc #42
        tax
        lda rxflen+1
        sbc #0
        bcc npnone
        tay
        txa
        cmp rxlen
        tya
        sbc rxlen+1
        bcc npnone
        lda RXFRM+36            ; to the DHCP client's port
        bne nplink
        lda RXFRM+37
        cmp #68
        bne nplink
        lda #NSDHCP
        rts
nplink: lda RXFRM+36            ; to the header's port and address
        cmp dialtxt+18
        bne npnot
        lda RXFRM+37
        cmp dialtxt+19
        bne npnot
        ldx #3
npdst:  lda RXFRM+30,x
        cmp dialtxt,x
        bne npnot
        dex
        bpl npdst
        ldx #5
nppmac: lda RXFRM+6,x
        sta pmac,x
        dex
        bpl nppmac
        ldx #3
nppip:  lda RXFRM+26,x
        sta pip,x
        dex
        bpl nppip
        lda RXFRM+34
        sta pport
        lda RXFRM+35
        sta pport+1
        lda #1
        sta peered
        lda #NSLINK
        rts
npnot:  lda #0
        rts

; nparp: an ARP reply to a request for the header's address, sent from
; TXFRM's first 42 bytes, below the datagram being put together at TXDATA.
nparp:  lda RXFRM+20
        bne naskip
        lda RXFRM+21
        cmp #1
        bne naskip
        ldx #3
naip:   lda RXFRM+38,x
        cmp dialtxt,x
        bne naskip
        dex
        bpl naip
        ldx #5
namac:  lda RXFRM+22,x          ; to the sender, from the header's MAC address
        sta TXFRM,x
        sta TXFRM+32,x
        lda dialtxt+12,x
        sta TXFRM+6,x
        sta TXFRM+22,x
        dex
        bpl namac
        ldx #9
nahdr:  lda arphdr,x
        sta TXFRM+12,x
        dex
        bpl nahdr
        ldx #3
naips:  lda dialtxt,x
        sta TXFRM+28,x
        lda RXFRM+28,x
        sta TXFRM+38,x
        dex
        bpl naips
        lda #42
        sta txflen
        lda #0
        sta txflen+1
        jsr nstx
naskip: lda #0
        rts

; An ARP reply's type, hardware and protocol, their lengths, and operation.
arphdr: .byte $08, $06, $00, $01, $08, $00, $06, $04, $00, $02

; nswait: until a datagram of the link comes, the announcement broadcast
; about once a second. Back from the program's Load program, the PC is known
; and nothing is waited for.
nswait: lda peered
        bne nswdone
nswann: jsr announce
        lda #1
        jsr nrecv
        bcs nswann
nswdone: rts

; announce: CASSOLINK, ANNVER, the address, the port and the MAC address,
; broadcast from UDP port 6511 to 6510.
announce: jsr bcast
        ldx #3
anip:   lda dialtxt,x
        sta sip,x
        sta TXDATA+10,x
        dex
        bpl anip
        lda #>6510
        sta dport
        lda #<6510
        sta dport+1
        lda #>6511
        sta sport
        lda #<6511
        sta sport+1
        ldx #8
antext: lda title,x
        sta TXDATA,x
        dex
        bpl antext
        lda #ANNVER
        sta TXDATA+9
        lda dialtxt+18
        sta TXDATA+14
        lda dialtxt+19
        sta TXDATA+15
        ldx #5
anmac:  lda dialtxt+12,x
        sta TXDATA+16,x
        dex
        bpl anmac
        lda #22
        sta plen
        lda #0
        sta plen+1
        jmp udpsend

; nsboot: the card set up, and for an address of 0.0.0.0 an address from a
; DHCP server, for the universal boot disk, which waits on the card through
; nrecv. The carry is set when no address comes.
nsboot: lda #1
        sta booted
        jsr nsinit
        lda dialtxt
        ora dialtxt+1
        ora dialtxt+2
        ora dialtxt+3
        clc
        bne nbdone
        jmp dhcp
nbdone: rts

; dhcp: an address from a DHCP server, into the header: a DISCOVER
; broadcast, a REQUEST for the OFFER's address, and the ACK's
; subnet mask. A message with no reply in 2 seconds, or a NAK, starts again
; from the DISCOVER, 4 times at most; the carry is set when no address
; comes.
dhcp:   lda #4
        sta dhtries
dhround: lda #1                 ; DISCOVER
        jsr dhsend
        jsr dhreply
        bcs dhagain
        cmp #2                  ; an OFFER
        bne dhagain
        ldx #3
dhoffer: lda RXDATA+16,x        ; its address
        sta offer,x
        dex
        bpl dhoffer
        lda #54                 ; its server
        jsr dhopt
        bcs dhagain
        ldx #0
dhsrv:  lda DHOPTS,y
        sta server,x
        iny
        inx
        cpx #4
        bne dhsrv
        lda #3                  ; REQUEST
        jsr dhsend
        jsr dhreply
        bcs dhagain
        cmp #5                  ; an ACK
        bne dhagain
        ldx #3
dhaddr: lda offer,x
        sta dialtxt,x
        dex
        bpl dhaddr
        lda #1                  ; the subnet mask
        jsr dhopt
        bcs dhdone
        ldx #0
dhmask: lda DHOPTS,y
        sta dialtxt+4,x
        iny
        inx
        cpx #4
        bne dhmask
dhdone: clc
        rts
dhagain: dec dhtries
        bne dhround
        sec
        rts

; dhreply: the next reply to this client within 2 seconds, its message type
; in A with the carry clear; the carry is set on a timeout.
dhreply: lda #2
        ldx #NSDHCP
        jsr nwaitk
        bcs drnone
        lda RXDATA              ; a reply
        cmp #2
        bne dhreply
        ldx #3
drxid:  lda RXDATA+4,x          ; to this transaction
        cmp dialtxt+14,x
        bne dhreply
        dex
        bpl drxid
        lda #53                 ; its message type
        jsr dhopt
        bcs dhreply
        lda DHOPTS,y
        clc
drnone: rts

; dhopt: the DHCP option A in the reply, its value at DHOPTS + Y with the
; carry clear; the carry is set when the reply has no such option.
dhopt:  sta dhcode
        ldy #0
doscan: lda DHOPTS,y
        cmp #255
        beq donone
        cmp #0
        bne docode
        iny
        bne doscan
        beq donone
docode: cmp dhcode
        beq dofound
        iny
        tya
        sec
        adc DHOPTS,y            ; past the length and the value
        bcs donone
        tay
        jmp doscan
dofound: iny
        iny
        clc
        rts
donone: sec
        rts

; dhsend: a DHCP message of the type in A broadcast from port 68 to 67, with
; the transaction id the MAC address's last 4 bytes. A REQUEST has the
; offered address and its server.
dhsend: sta dhtype
        lda #0                  ; 300 bytes, the least a BOOTP message has
        tax
dszero: sta TXDATA,x
        sta TXDATA+44,x
        inx
        bne dszero
        lda #1
        sta TXDATA              ; a request
        sta TXDATA+1            ; for Ethernet
        lda #6
        sta TXDATA+2            ; and its 6-byte addresses
        lda #$80
        sta TXDATA+10           ; the reply broadcast, since the address is 0.0.0.0
        ldx #3
dsxid:  lda dialtxt+14,x
        sta TXDATA+4,x
        lda dhmagic,x
        sta TXDATA+236,x
        dex
        bpl dsxid
        ldx #5
dsmac:  lda dialtxt+12,x
        sta TXDATA+28,x
        dex
        bpl dsmac
        lda #53
        sta TXDATA+240
        lda #1
        sta TXDATA+241
        lda dhtype
        sta TXDATA+242
        cmp #3
        beq dsreq
        lda #255
        sta TXDATA+243
        bne dsgo                ; always
dsreq:  lda #50
        sta TXDATA+243
        lda #54
        sta TXDATA+249
        lda #4
        sta TXDATA+244
        sta TXDATA+250
        ldx #3
dsaddr: lda offer,x
        sta TXDATA+245,x
        lda server,x
        sta TXDATA+251,x
        dex
        bpl dsaddr
        lda #255
        sta TXDATA+255
dsgo:   jsr bcast
        lda #0
        ldx #3
dssip:  sta sip,x
        dex
        bpl dssip
        sta sport
        sta dport
        lda #68
        sta sport+1
        lda #67
        sta dport+1
        lda #<300
        sta plen
        lda #>300
        sta plen+1
        jmp udpsend

dhmagic: .byte $63, $82, $53, $63

; ---------------------------------------------------------------------------
; The CS8900A, in its 8-bit I/O mode. A frame's status and length are read
; high byte first; its data and every other register low byte first.
; ---------------------------------------------------------------------------

; csread: the PacketPage register at A (low) and Y (high), into A (low) and
; Y (high). X is the card's index.
csread: ldx nsx
        sta CSPPTR,x
        tya
        sta CSPPTR+1,x
        lda CSPDATA+1,x
        tay
        lda CSPDATA,x
        rts

; cswrite: csval into the PacketPage register at A (low) and Y (high).
cswrite: ldx nsx
        sta CSPPTR,x
        tya
        sta CSPPTR+1,x
        lda csval
        sta CSPDATA,x
        lda csval+1
        sta CSPDATA+1,x
        rts

; csinit: the chip reset, its individual address set, and its receiver and
; transmitter on for frames to that address and broadcast frames.
csinit: lda #$55                ; SelfCTL: RESET
        ldx #$00
        ldy #$14
        jsr csset
        lda #0                  ; SelfST's INITD, for at most 65,536 checks
        sta nsclo
        sta nschi
csinit2: lda #$36
        ldy #$01
        jsr csread
        and #$80
        bne csia
        inc nsclo
        bne csinit2
        inc nschi
        bne csinit2
csia:   lda dialtxt+12          ; the individual address, at $0158
        ldx dialtxt+13
        ldy #$58
        jsr csset
        lda dialtxt+14
        ldx dialtxt+15
        ldy #$5A
        jsr csset
        lda dialtxt+16
        ldx dialtxt+17
        ldy #$5C
        jsr csset
        lda #$05                ; RxCTL: RxOK, individual and broadcast
        ldx #$0D
        ldy #$04
        jsr csset
        lda #$D3                ; LineCTL: SerRxON and SerTxON
        ldx #$00
        ldy #$12
        ; falls into csset

; csset: A (low) and X (high) into the PacketPage register at $0100 + Y.
csset:  sta csval
        stx csval+1
        tya
        ldy #$01
        jmp cswrite

; csrx: nsrx for the CS8900A. A frame longer than 1,518 bytes or shorter
; than 14 is skipped.
csrx:   lda #$24                ; RxEvent's RxOK
        ldy #$01
        jsr csread
        tya
        and #$01
        bne csrxok
        sec
        rts
csrxok: lda CSRT1,x             ; the status, not used
        lda CSRT0,x
        lda CSRT1,x             ; the length
        sta rxflen+1
        lda CSRT0,x
        sta rxflen
        jsr frsize
        bcs csskip
        lda #>RXFRM
        sta csst0+2
        sta csst1+2
        lda rxflen              ; its words
        ldy rxflen+1
        jsr frwords
        ldy #0
csword: lda CSRT0,x
csst0:  sta RXFRM,y
        iny
        lda CSRT1,x
csst1:  sta RXFRM,y
        iny
        bne csrxn
        inc csst0+2
        inc csst1+2
csrxn:  jsr frcount
        bne csword
        clc
        rts
csskip: lda #$43                ; RxCFG: Skip_1
        ldx #$00
        ldy #$02
        jsr csset
        sec
        rts

; cstx: nstx for the CS8900A, once it has room for the frame. A frame with
; no room within 65,536 checks is dropped.
cstx:   ldx nsx
        lda #$C0                ; TxCMD: sent once all of it is in
        sta CSTXCMD,x
        lda #$00
        sta CSTXCMD+1,x
        lda txflen
        sta CSTXLEN,x
        lda txflen+1
        sta CSTXLEN+1,x
        lda #0
        sta nsclo
        sta nschi
cstxw:  lda #$38                ; BusST
        ldy #$01
        jsr csread
        and #$80                ; TxBidErr: a length it can't send
        bne cstxno
        tya
        and #$01                ; Rdy4TxNOW
        bne cstxgo
        inc nsclo
        bne cstxw
        inc nschi
        bne cstxw
cstxno: rts
cstxgo: lda #>TXFRM
        sta csld0+2
        sta csld1+2
        lda txflen
        ldy txflen+1
        jsr frwords
        ldy #0
csld0:  lda TXFRM,y
cswr0:  sta CSRT0
        iny
csld1:  lda TXFRM,y
cswr1:  sta CSRT1
        iny
        bne cstxn
        inc csld0+2
        inc csld1+2
cstxn:  jsr frcount
        bne csld0
        rts

; frsize: the carry set for a frame length in rxflen over 1,518 or under 14.
frsize: lda rxflen+1
        cmp #$06
        bcs fsdone
        cmp #$05
        beq fslong
        cmp #0
        bne fsok
        lda rxflen
        cmp #14
        bcc fsbad
fsok:   clc
        rts
fslong: lda rxflen
        cmp #$EF
fsdone: rts
fsbad:  sec
        rts

; frwords: nscnt set to the words of the length in A (low) and Y (high),
; (length + 1) / 2.
frwords: clc
        adc #1
        sta nscnt
        tya
        adc #0
        lsr a
        sta nscnt+1
        ror nscnt
        rts

; frcount: nscnt less 1, with Z set at 0.
frcount: lda nscnt
        bne fcdec
        dec nscnt+1
fcdec:  dec nscnt
        lda nscnt
        ora nscnt+1
        rts

; ---------------------------------------------------------------------------
; The LAN91C96, a byte at a time, in bank 2 once set up. A packet in its
; memory is a status word, a byte count of the whole packet, the frame, and
; a control byte, with the frame's last byte ahead of it when the frame's
; length is odd.
; ---------------------------------------------------------------------------

; lninit: the MMU reset, frames sent with their pages released, the
; individual address set, and the receiver and transmitter on, the frames
; sent padded and the frames received without their CRC.
lninit: ldx nsx
        lda #2
        sta LNBANK,x
        lda #$40                ; the MMU reset
        sta LNMMU,x
        lda #1
        sta LNBANK,x
        lda #$09                ; bank 1's CTR: AUTO RELEASE
        sta $C08D,x
        lda dialtxt+12          ; bank 1's individual address
        sta $C084,x
        lda dialtxt+13
        sta $C085,x
        lda dialtxt+14
        sta $C086,x
        lda dialtxt+15
        sta $C087,x
        lda dialtxt+16
        sta $C088,x
        lda dialtxt+17
        sta $C089,x
        lda #0
        sta LNBANK,x
        lda #$81                ; bank 0's TCR: TXENA and PAD_EN
        sta $C080,x
        lda #$00
        sta $C081,x
        sta $C084,x             ; bank 0's RCR: RXEN and STRIP_CRC
        lda #$03
        sta $C085,x
        lda #2
        sta LNBANK,x
        rts

; lnrx: nsrx for the LAN91C96. A frame longer than 1,518 bytes or shorter
; than 14 is released unread.
lnrx:   ldx nsx
        lda LNRXFIFO,x
        bpl lnrxok
        sec
        rts
lnrxok: lda #$00                ; the pointer: receive area, auto-increment, read
        sta LNPTR,x
        lda #$E0
        sta LNPTR+1,x
        lda LNDATA,x            ; the status word, ODDFRM in bit 12
        lda LNDATA,x
        sta lnstat
        lda LNDATA,x            ; the byte count
        sec
        sbc #6
        sta rxflen
        lda LNDATA,x
        sbc #0
        sta rxflen+1
        bcc lnrel
        lda lnstat
        and #$10
        beq lnsize
        inc rxflen
        bne lnsize
        inc rxflen+1
lnsize: jsr frsize
        bcs lnrel
        lda #>RXFRM
        sta lnst+2
        lda rxflen
        sta nscnt
        lda rxflen+1
        sta nscnt+1
        ldy #0
lnbyte: lda LNDATA,x
lnst:   sta RXFRM,y
        iny
        bne lnrxn
        inc lnst+2
lnrxn:  jsr frcount
        bne lnbyte
        jsr lnrel
        clc
        rts
lnrel:  jsr lnidle
        lda #$80                ; the frame removed, and its pages released
        sta LNMMU,x
        sec
        rts

; lntx: nstx for the LAN91C96, in pages allocated for it. A frame with no
; pages within 65,536 checks is dropped.
lntx:   jsr lnidle
        lda txflen+1            ; (length + 6) / 256 pages, less 1
        ldy txflen
        cpy #$FA
        bcc lnpages
        adc #0
lnpages: ora #$20               ; allocate
        sta LNMMU,x
        lda #0
        sta nsclo
        sta nschi
lnalloc: lda LNIST,x
        and #$08                ; ALLOC_INT
        bne lnalloc2
        inc nsclo
        bne lnalloc
        inc nschi
        bne lnalloc
        rts
lnalloc2: lda LNARR,x
        bmi lntxno
        sta LNPNR,x
        lda #$00                ; the pointer: transmit area, auto-increment
        sta LNPTR,x
        lda #$40
        sta LNPTR+1,x
        lda #0                  ; the status word
        jsr lnput
        jsr lnput
        lda txflen              ; the byte count, even
        clc
        adc #6
        pha
        lda txflen+1
        adc #0
        tay
        pla
        and #$FE
        jsr lnput
        tya
        jsr lnput
        lda #>TXFRM
        sta lnld+2
        lda txflen
        sta nscnt
        lda txflen+1
        sta nscnt+1
        ldy #0
lnld:   lda TXFRM,y
        jsr lnput
        iny
        bne lntxn
        inc lnld+2
lntxn:  jsr frcount
        bne lnld
        lda txflen              ; the control byte: ODD, after the last byte
        lsr a
        lda #$20
        bcs lnctl
        lda #0                  ; or after a byte not sent
        jsr lnput
lnctl:  jsr lnput
        ldx nsx
        lda #$C0                ; the packet queued to be sent
        sta LNMMU,x
lntxno: rts

; lnput: A written to the data register, which advances the pointer.
lnput:
lnwr:   sta LNDATA
        rts

; lnidle: until the MMU's command before is done. X is the card's index.
lnidle: ldx nsx
lnwait: lda LNMMU,x
        lsr a
        bcs lnwait
        rts

; ---------------------------------------------------------------------------
; The stack's variables.
; ---------------------------------------------------------------------------

nsx:    .byte 0                 ; the card's index, slot * 16
nswant: .byte 0                 ; nwaitk: the kind waited for
nsecs:  .byte 0                 ; and its seconds left
nrlo:   .byte 0                 ; and that second's rounds left
nrhi:   .byte 0
nsclo:  .byte 0                 ; a chip's checks left
nschi:  .byte 0
nscnt:  .word 0                 ; a frame's words or bytes left
plen:   .word 0                 ; udpsend: the payload's length
dmac:   .ds 6                   ; and where it goes
dip:    .ds 4
dport:  .ds 2
sport:  .ds 2                   ; and where it comes from
sip:    .ds 4
pmac:   .ds 6                   ; the PC: the last datagram of the link's sender
pip:    .ds 4
pport:  .ds 2
peered: .byte 0                 ; nonzero once a datagram of the link has come
rxflen: .word 0                 ; the frame received's length
txflen: .word 0                 ; and the frame to send's
sumlo:  .byte 0                 ; ipsum: the sum
sumhi:  .byte 0
csval:  .word 0                 ; cswrite: the value
lnstat: .byte 0                 ; lnrx: the status word's high byte
dhcode: .byte 0                 ; dhopt: the option looked for
dhtype: .byte 0                 ; dhsend: the message type
dhtries: .byte 0                ; dhcp: the rounds left
offer:  .ds 4                   ; the address offered
server: .ds 4                   ; by this DHCP server
netend:

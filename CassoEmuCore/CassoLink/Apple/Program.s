; Casso's CassoLink: the program.
;
; Loaded at $0800 by the shim. It sends Ready, with its version, the shim's
; and its drive map, and the host's reply is the next operation:
;
;   Idle            nothing to do; Ready again after 100 ms
;   Set speed       a 6551 control byte; ACK, then the new speed
;   Read sectors    drive, track, count, sectors: Sector data for each
;   Write sectors   drive, track, count, sectors: for each, an Acknowledge
;                   with its number, ACK or NAK for another copy, and the
;                   host's Sector data, written and read back
;   Format track    drive, track, volume, and after the percentages the gap
;                   after each sector, in self-sync bytes (0 for 10)
;   Write track     drive, track, volume and the gap: an Acknowledge for each
;                   of the 16 sectors as for Write sectors, then the track
;                   formatted with their data in one revolution, read back
;                   and compared
;   Time drive      drive, track, revolutions: $A5 the moment each pass of
;                   sector 0's address field after the first is read, so the
;                   PC times the drive's revolution
;   Cancel          during an operation: it stops, and Ready follows
;   Label           the user's operation, as text: drawn, and Ready again
;   Log             the PC's hour and minute, then text: a history line,
;                   drawn at the top of the history, and Ready again
;   Load program    back to the shim, which sends Ready for the program
;   Exit            CassoLink ends, and control returns to BASIC or the monitor
;
; Read sectors, Write sectors, Format track and Write track end with two
; percentages, the part of the user's whole operation the request covers, so
; the status row has the progress of the whole operation.
;
; Every operation ends with a Status packet: operation, drive, track,
; progress and an error code (0, or one of the ERR values below). A sector is
; the number in its address field, the physical sector; the host maps DOS 3.3
; and ProDOS numbering onto it.
;
; The status screen on the text page has the connection; the PC, connected
; or how long since its last reply; the drive, the track, the operation and
; the progress; the last error, until an operation succeeds; the last
; Label; and the history, the host's Log lines with the newest at the top.
; Ready is sent again after each second with no reply.
;
; The disk routines are Casso's own, from Apple's published description of
; the Disk II and the 6-and-2 encoding Casso's NibblizationLayer writes: the
; arm is stepped by its phases, sectors are found by their address fields,
; and data fields are read and written in 6-and-2. Writes go through one
; cycle-counted loop, the player, which writes a prepared stream of nibbles
; 32 cycles apart and each self-sync byte 40 cycles after the one before it.
;
; NMOS 6502 code throughout.
;
; The W5100 build, for an Uthernet II, has its own byte routines and its
; own ends to Set speed, Load program and Exit; the rest is the same code.
; The shim has socket 0 connected to the PC: bytes to send go in the chip's
; transmit memory and are sent before each wait for a byte, or once 128 are
; waiting. When the PC disconnects, the socket listens again, and the PC's
; silence is reported as on a serial line. Set speed is acknowledged and
; changes nothing, and Exit disconnects the socket.

        ifndef PROGVER
PROGVER = 1
        endif
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

; The 6551, through slotx: slot * 16, $20 for the //c's port 2.
ACIADATA = $C088
ACIASTAT = $C089
ACIACMD  = $C08A
ACIACTL  = $C08B
RDRF     = $08
TDRE     = $10

        ifdef W5100
; The W5100, through slotx: slot * 16, $30 for slot 3.
WZHI     = $C085                ; the pointer, high byte
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
CMDISCON = $08
CMCLOSE  = $10
CMSEND   = $20
CMRECV   = $40
STLISTEN = $14
STSYNRCV = $16
STESTAB  = $17
IRSENDOK = $10
        endif

; The Disk II, through diskx: slot * 16, $60 for slot 6.
PHASEOFF = $C080                ; + phase * 2
PHASEON  = $C081
MOTOROFF = $C088
MOTORON  = $C089
DRIVE1   = $C08A
DRIVE2   = $C08B
Q6L      = $C08C
Q6H      = $C08D
Q7L      = $C08E
Q7H      = $C08F

; Commands.
CMDREADY  = $D2
CMDIDLE   = $C9
CMDDATA   = $CA
CMDNIBS   = $CC                 ; a sector's data field nibbles, for Write sectors
CMDACK    = $CB
CMDSPEED  = $D7
CMDREAD   = $D5
CMDWRITE  = $D6
CMDFORMAT = $C8
CMDWTRACK = $DB
CMDSTATUS = $D4
CMDCANCEL = $D1
CMDLOAD   = $CF
CMDLABEL  = $DC
CMDLOG    = $DD
CMDTIME   = $DE
CMDEXIT   = $DF
        ifdef W5100
SHIM      = $3B00
        else
SHIM      = $3D00
        endif
SHIMSPEED = SHIM + $02            ; the shim's 6551 control byte
SHIMTEXT  = SHIM + $06            ; its speed's text, six characters
SHIMDIAL  = SHIM + $0C            ; a Wi-Fi modem's dial command
SHIMPORT  = SHIM + $1E            ; a W5100 shim's TCP port, high byte first
SHIMSAVED = SHIM + $34            ; nonzero once the shim saved the machine's state
SHIMRESET = SHIM + $35            ; the reset vector the shim found, and its check
        ifdef W5100
ZPSAVE    = $3A00                 ; the zero page the shim found, below the shim
        else
ZPSAVE    = $3C00
        endif
ACK       = $06
NAK       = $15

; Operations, as the Status packet and the screen report them.
OPIDLE   = 0
OPREAD   = 1
OPWRITE  = 2
OPFORMAT = 3
OPSPEED  = 4

; Errors.
ERRNONE    = $00
ERRPROTECT = $10                ; the disk is write-protected
ERRNOTFND  = $20                ; no address field for the sector
ERRDATA    = $30                ; the data field did not decode
ERRVERIFY  = $40                ; the sector read back differently
ERRLINK    = $50                ; the host's data did not arrive whole
ERRDRIVE   = $60                ; no such drive

; Buffers, page-aligned so indexed loads never cross a page.
INVTAB   = $4000                ; nibble -> 6-bit value
CRCLO    = $4100                ; CRC-16/XMODEM table
CRCHI    = $4200
SECBUF   = $4300                ; a sector to write
WRAUX    = $4700                ; write: the 86 two-bit groups
WRPRI    = $4800                ; write: the 256 six-bit values
PKTBUF   = $4900                ; a command's payload
WSTREAM  = $4A00                ; a sector's write stream, two pages
FSTREAM  = $5000                ; a track's format stream, 25 pages
RAWBUF   = $5000                ; read: a track's raw sectors, two pages each, to $6FFF
WSLOTS   = $7000                ; write sectors: each sector's stream, two pages each, to $8FFF,
                                ; on ENCBUF's pages, as Write sectors encodes nothing
AUXT0    = $4C00                ; a 6-bit group's two bits for the first third
AUXT1    = $4C40                ; for the second
AUXT2    = $4C80                ; for the third
AUXA0    = $4D00                ; sendsector: a sector's bits, 86 per third
AUXA1    = AUXA0 + 86
AUXA2    = AUXA1 + 86
PENDING  = $4980                ; read: a flag per physical sector not yet read
ENCBUF   = $7000                ; write track: each sector's 86 groups on one
                                ; page and 256 values on the next, to $8FFF
ENCT0    = $9000                ; encode: a byte's low two bits, swapped, for
ENCT1    = $9100                ; the first third of a sector, shifted for the
ENCT2    = $9200                ; second, and for the third
FPAGES   = 26                   ; the format stream, whatever its gaps
WSTART   = 512 - 355            ; the sector stream and its end mark end at its second page's end
NIBSTART = WSTART + 8           ; its 343 data field nibbles, after 5 syncs and the prologue
SYNCEND  = NIBSTART - 3         ; where the prologue starts
NIBEND   = NIBSTART + 343 - 256 ; where the nibbles end on the second page
PEND     = $01                  ; the end of a write stream

; Zero page.
slotx    = $06                  ; as the shim set it
plo      = $07                  ; the whole operation's percentage at the
phi      = $08                  ; request's start, and at its end
fdata    = $09                  ; buildformat: nonzero for ENCBUF's sectors
misses   = $0A                  ; revolutions in a row with no address field
revs     = $0B                  ; Time drive: passes left to report
fgap     = $0C                  ; the gap after each sector, in self-sync bytes
idles    = $0D                  ; Idle replies since the last operation
diskx    = $E0
drive    = $E1                  ; 1 or 2
track    = $E2
sector   = $E3
volume   = $E4
count    = $E5                  ; sectors in the request
index    = $E6                  ; the one being worked on
op       = $E7
err      = $E8
crclo    = $E9
crchi    = $EA
ptr      = $EB                  ; and $EC
tmp      = $ED
tmp2     = $EE
tmo      = $EF                  ; $EF-$F1: the receive timeout
avol     = $F2                  ; an address field: volume, track, sector,
atrk     = $F3                  ; checksum
asec     = $F4
achk     = $F5
target   = $F6                  ; the half-track the arm is going to
pagecnt  = $F7
tries    = $F8
peor     = $F9                  ; the EOR of a payload
savex    = $FA
envcmd   = $FB
lenlo    = $FC
lenhi    = $FD
curhalf  = $FE                  ; and $FF: each drive's half-track, $FF unknown
        ifdef W5100
rxrd     = $24                  ; and $25: the next byte to read, as Sn_RX_RD counts
rxn      = $26                  ; and $27: bytes received and not yet read
txwr     = $28                  ; and $29: where the next byte goes, as Sn_TX_WR counts
txn      = $2A                  ; bytes put since the last SEND
wsavy    = $2B                  ; getbt and putb: Y as it came
wbyte    = $2C                  ; putb: the byte; putword: a difference
wrel     = $2D                  ; getbt: each second's rounds of 256 polls
wnum     = $2E                  ; and $2F: putword's value
wdig     = $30                  ; putword: the digit
wlead    = $31                  ; putword: nonzero once a digit is drawn
        endif

        .org $0800

        jmp entry
        .word PROGVER           ; $0803: the version the shim draws

entry:  sei
        cld
        ldx #$FF
        txs
        lda slotx
        bne slotok
        lda #$20
        sta slotx
slotok:
        ifdef W5100
        ora #<WZDATA            ; each write of the data is absolute, as an
        sta wzput+1             ; indexed store reads it first, which would
        jsr wzsync              ; advance the pointer
        else
        clc
        adc #<ACIADATA
        sta pbsta+1
        sta sdsta+1             ; sendsector's own store and status read,
        clc                     ; absolute for the same reason as putb's
        adc #1
        sta sdwait+1
        endif
        lda #$60
        sta diskx
        lda #$FF
        sta curhalf
        sta curhalf+1
        lda #OPIDLE
        sta op
        lda #1
        sta drive
        lda #0
        sta track
        sta sector
        sta plo
        sta phi
        sta index
        sta count
        sta idles
        sta pcflag
        sta silent
        sta spinning
        sta fdata
        lda #3
        sta tmolen
        lda #<exitlink          ; Control-Reset ends CassoLink cleanly
        sta $03F2
        lda #>exitlink
        sta $03F3
        eor #$A5
        sta $03F4
        jsr mktables
        jsr probe
        jsr readid
        jsr drawscreen

main:   lda $C000               ; Escape on the Apple ends CassoLink
        cmp #$9B
        bne mnokey
        sta $C010
        jmp exitlink
mnokey: lda #OPIDLE
        sta op
        jsr showop
        jsr sendready
        lda #1                  ; a second for the reply
        sta tmolen
        jsr getenvt
        lda #3                  ; and three for each byte of an operation
        sta tmolen
        bcc mreply
        jsr motoroff            ; with no PC, the drive stops
        jsr nopc
        jmp main
mreply: jsr getpayload
        bcs main
        jsr pcok
        lda envcmd
        cmp #CMDIDLE
        bne notidle
        jsr clrlabel            ; the user's operation is over
        lda idles               ; the motor runs on for IDLEOFF Idles, about
        cmp #IDLEOFF            ; two seconds, so the user's next operation
        bcc mnidle              ; does not wait for it to come up to speed
        jsr motoroff
        jmp mnwait
mnidle: inc idles
mnwait: lda #100
        jsr delayms
        jmp main
notidle: cmp #CMDSPEED
        bne notspeed
        jmp dospeed
notspeed: cmp #CMDREAD
        bne notread
        jmp doread
notread: cmp #CMDWRITE
        bne notwrite
        jmp dowrite
notwrite: cmp #CMDFORMAT
        bne notformat
        jmp doformat
notformat: cmp #CMDWTRACK
        bne notwtrack
        jmp dowtrack
notwtrack: cmp #CMDLABEL
        bne notlabel
        jmp dolabel
notlabel: cmp #CMDLOG
        bne notlog
        jmp dolog
notlog: cmp #CMDTIME
        bne nottime
        jmp dotime
nottime: cmp #CMDLOAD
        bne notload
        jmp doload
notload: cmp #CMDEXIT
        bne tomain
        jmp exitlink
tomain: jmp main

; exitlink: CassoLink ends, by Control-Reset, Escape or the PC's Exit, and
; control returns to BASIC. The drive is in read mode with its motor off and
; the 6551's DTR off, so the PC's DSR drops; the zero page and reset
; vector the shim saved are put back; BASIC's program is emptied as NEW
; does, since the program was loaded over it; input and output are the
; keyboard and screen; and DOS's warm start follows, or the monitor when
; there is no DOS.
exitlink: sei
        cld
        ldx #$FF
        txs
        ldx diskx
        lda Q7L,x
        lda Q6L,x
        jsr motoroff
        ifdef W5100
        lda #CMDISCON           ; the PC's connection ended, then the socket
        jsr wzcmd               ; closed, after at most a few seconds
        lda #0
        sta tmo
        sta tmo+1
xlwait: jsr wzstat
        beq xlclose
        inc tmo
        bne xlwait
        inc tmo+1
        bne xlwait
xlclose: lda #CMCLOSE
        jsr wzcmd
        else
        ldx slotx
        lda #$0A                ; DTR off, as the shim set it otherwise
        sta ACIACMD,x
        endif
        lda SHIMSAVED          ; 1 once the shim saved them; an older shim
        cmp #1                  ; has code there, which is left alone
        bne lvbasic
        ldx #0
lvzp:   lda ZPSAVE,x
        sta $00,x
        inx
        bne lvzp
        lda SHIMRESET
        sta $03F2
        lda SHIMRESET+1
        sta $03F3
        lda SHIMRESET+2
        sta $03F4
        lda #0
        sta SHIMSAVED
lvbasic: lda #0                 ; text output plain, whatever was saved:
        sta $F3                 ; FLASHBIT off and INVFLG normal, as a zero
        lda #$FF                ; INVFLG prints every character as @
        sta $32
        lda #0
        sta $0800
        sta $0801
        sta $0802
        lda #$01                ; TXTTAB, BASIC's program, at $0801
        sta $67
        lda #$08
        sta $68
        sta $6A
        sta $6C
        sta $6E
        lda #$03                ; VARTAB, ARYTAB and STREND at $0803: no
        sta $69                 ; program and no variables
        sta $6B
        sta $6D
        jsr $FE89               ; SETKBD: input from the keyboard
        jsr $FE93               ; SETVID: output to the screen
        jsr $FB2F               ; INIT: the text screen, the whole window
        jsr $FC58               ; HOME
        lda $03D0               ; DOS's warm start, a JMP into DOS
        cmp #$4C
        bne lvmon
        lda $03D2
        cmp #$9D
        bcc lvmon
        jmp $03D0
lvmon:  jmp $FF69               ; the monitor

; Time drive: sector 0's address field found on the track, then $A5 sent as
; each later pass of it is read, one a revolution; the time between the
; PC's timestamps on them is the drive's revolution.
dotime: lda PKTBUF+2
        sta revs
        lda #0
        sta PKTBUF+2            ; no sector count for begin
        lda #OPREAD
        jsr begin
        bcs tmdone
        lda #0
        sta sector
        jsr findsector
        bcs tmdone
tmloop: jsr findsector
        bcs tmdone
        lda #$A5
        jsr putb
        ifdef W5100
        jsr flush               ; sent at once, as the PC times its arrival
        endif
        dec revs
        bne tmloop
tmdone: jmp opdone

; getgap: fgap from the request's byte after its percentages, or 10 when the
; request has none or it is outside 4 to 40.
getgap: lda #10
        sta fgap
        lda lenlo
        cmp #6
        bcc ggdone
        lda PKTBUF+5
        cmp #4
        bcc ggdone
        cmp #41
        bcs ggdone
        sta fgap
ggdone: rts

; doload: back to the shim, at the speed the 6551 is at now, which is the
; speed the host sends the program at; and with no dial command, as a modem
; link is connected already. A W5100's connection is left open, with what
; was read given back and what was put sent.
        ifdef W5100
doload: jsr flush
        jsr rxdone
        jmp SHIM
        else
doload: ldx slotx
        lda ACIACTL,x
        sta SHIMSPEED
        and #$0F
        ldx #0
dlfind: cmp speeds,x
        beq dlfound
        inx
        inx
        inx
        cpx #speedend-speeds
        bne dlfind
dlfound: lda speeds+1,x
        sta ptr
        lda speeds+2,x
        sta ptr+1
        ldy #0
dltext: lda (ptr),y
        beq dlpad
        sta SHIMTEXT,y
        iny
        cpy #6
        bne dltext
        beq dlgo
dlpad:  lda #' '
        sta SHIMTEXT,y
        iny
        cpy #6
        bne dlpad
dlgo:   lda #0
        sta SHIMDIAL
        jmp SHIM
        endif
; cancel: the operation stops where it is, and the loop starts again.
cancel: jsr motoroff
        ldx #$FF
        txs
        jmp main


; ---------------------------------------------------------------------------
; Operations
; ---------------------------------------------------------------------------

; Set speed: ACK at the old speed, wait until it is sent, then switch.
dospeed: lda #OPSPEED
        sta op
        jsr showop
        lda #ACK
        ldx #0
        ldy #0
        jsr sendack
        ifdef W5100
        jmp main                ; a W5100 has no speed to set
        else
        ldx slotx
spdwait: lda ACIASTAT,x
        and #TDRE
        beq spdwait
        lda #12                 ; the last byte's bits on the line, at as
        jsr delayms             ; slow as 1,200 baud
        lda PKTBUF
        sta ACIACTL,x
        jsr showconn
        jmp main
        endif

; Read sectors: the requested sectors read into TRKBUF in the order they pass
; under the head, then each one's data in the order of the request, then the status.
; Sending a sector takes longer than the gap to the next, so reading and
; sending in turn would wait most of a revolution for every sector.
doread: lda #OPREAD
        jsr begin
        bcs opdone
        jsr readtrack
        bcs opdone
rdnext: jsr showprog
        ldy index
        lda PKTBUF+3,y
        sta sector
        jsr rawslot
        jsr sendsector
        jsr pollcancel
        inc index
        lda index
        cmp count
        bne rdnext
        jsr showprog
; The motor runs on after an operation, so the next track's read does not wait
; for it to spin up again; Idle, with nothing more to do, stops it.
opdone: jsr showerr
        jsr sendstatus
        jmp main

; Write sectors: for each, an Acknowledge with its number, then the host's
; Sector nibbles, checked by their CRC, with a NAK sent for another copy
; until they arrive whole. The host encodes each sector, so its nibbles go
; straight into its stream in WSLOTS, and only the fixed parts around them
; are added here; then all are written in one pass, each as its address
; field passes, and the track read back once and compared. Writing each
; sector and reading it back before asking for the next takes more than a
; revolution a sector, and encoding each here took more time than sending
; its nibbles.
dowrite: lda #OPWRITE
        jsr begin
        bcs wrdone
        jsr protected
        bcs wrdone
wrnext: jsr showprog
        ldy index
        lda PKTBUF+3,y
        and #15
        sta sector
        lda #8
        sta tries
        lda #ACK
wrask:  ldx sector
        ldy track
        jsr sendack
        jsr getnibs
        bcc wrgot
        jsr drain
        dec tries
        beq wrlink
        lda #NAK
        jmp wrask
wrlink: lda #ERRLINK
        sta err
wrdone: jmp opdone
wrgot:  jsr frameslot
        inc index
        lda index
        cmp count
        bne wrnext
        jsr writepass
        bcs wrdone
        jsr verifysecs
        jsr showprog
        jmp opdone

; Format track: 16 sectors of zeros, numbered 0 to 15 in order around the
; track, under the given volume; then the track read back, so a format that
; did not take ends with the first sector that does not read.
doformat: lda PKTBUF+2
        sta volume
        jsr getgap
        lda #0
        sta PKTBUF+2            ; no sector count for begin
        lda #OPFORMAT
        jsr begin
        bcs fmtdone
        jsr protected
        bcs fmtdone
        jsr buildformat
        lda #>FSTREAM
        ldy #0
        jsr play
        jsr readall
        bcs fmtdone
        lda #1
        sta count
        sta index
        jsr showprog
fmtdone: jmp opdone

; Write track: the 16 sectors, each requested by an Acknowledge, as for
; Write sectors, and encoded into ENCBUF as it arrives; then the track formatted with them,
; which writes it in one revolution, and read back and compared. Writing
; sector by sector waits for each sector to come round twice, once to write
; and once to read back.
dowtrack: lda PKTBUF+2
        sta volume
        jsr getgap
        lda #0
        sta PKTBUF+2            ; no sector count for begin
        lda #OPWRITE
        jsr begin
        bcs wtdone
        jsr protected
        bcs wtdone
        lda #16
        sta count
wtnext: jsr showprog
        lda index
        sta sector
        lda #8
        sta tries
        lda #ACK
wtask:  ldx sector
        ldy track
        jsr sendack
        jsr getsector
        bcc wtgot
        jsr drain
        dec tries
        beq wtlink
        lda #NAK
        jmp wtask
wtlink: lda #ERRLINK
        sta err
wtdone: jmp opdone
wtgot:  jsr encpage
        jsr encodeto
        inc index
        lda index
        cmp count
        bne wtnext
        lda #1
        sta fdata
        jsr buildformat
        lda #0
        sta fdata
        lda #>FSTREAM
        ldy #0
        jsr play
        jsr verifytrack
        jsr showprog
        jmp opdone

; encpage: the page of `sector`'s groups in ENCBUF in A; its values are on
; the page after it.
encpage: lda sector
        asl a
        clc
        adc #>ENCBUF
        rts

; readall: all 16 sectors of the track read into RAWBUF. The carry is set,
; with err and sector, at the first that does not read.
readall: lda #16
        sta count
        ldx #15
ralist: txa
        sta PKTBUF+3,x
        dex
        bpl ralist
        jmp readtrack

; verifytrack: the track read back, and each sector's 6-and-2 values compared
; with those in ENCBUF. The carry is set, with err and sector, at the first
; that differs.
verifytrack: jsr readall
        bcs vtfail
        lda #0
        sta sector
vtsec:  jsr cmpslot
        bcs vtfail
        inc sector
        lda sector
        cmp #16
        bne vtsec
        dec sector              ; the status ends on the last sector, 15
        clc
        rts
vtfail: sec
        rts

; verifysecs: Write sectors' sectors read back, and each one's 6-and-2
; values compared with those in ENCBUF. The carry is set, with err and
; sector, at the first that does not read or differs.
verifysecs: jsr readtrack
        bcs vsdone
        lda #0
        sta left
vsnext: ldx left
        lda PKTBUF+3,x
        and #15
        sta sector
        jsr cmpnibs
        bcs vsdone
        inc left
        lda left
        cmp count
        bne vsnext
        clc
vsdone: rts

; cmpslot: `sector`'s raw values in RAWBUF compared with its encoding in
; ENCBUF. The carry is set, with err, when they differ.
cmpslot: jsr rawslot
        jsr encpage
        sta csaux+2
        clc
        adc #1
        sta cspri+2
        ldy #85
cscmpa: lda (dst),y
csaux:  cmp ENCBUF,y
        bne csbad
        dey
        bpl cscmpa
        ldy #0
cscmpp: lda (dstp),y
cspri:  cmp ENCBUF+256,y
        bne csbad
        iny
        bne cscmpp
        clc
        rts
csbad:  lda #ERRVERIFY
        sta err
        sec
        rts
; begin: the operation in A; drive and track from the payload, the sector
; count after them. Draw them, start the motor and seek. The carry is set
; with err when the drive is not 1 or 2.
begin:  sta op
        lda #ERRNONE
        sta err
        lda #0
        sta index
        sta idles
        lda PKTBUF
        sta drive
        lda PKTBUF+1
        sta track
        lda PKTBUF+2
        sta count
        ldy count               ; the percentages after the sectors
        lda PKTBUF+3,y
        sta plo
        lda PKTBUF+4,y
        sta phi
        jsr showop
        jsr showdrive
        jsr showtrack
        lda drive
        cmp #1
        beq drvok
        cmp #2
        beq drvok
        lda #ERRDRIVE
        sta err
        sec
        rts
drvok:  jsr motoron
        jsr seek
        clc
        rts


; ---------------------------------------------------------------------------
; Packets
; ---------------------------------------------------------------------------

; sendready: Ready with this program's version, the shim's and the drives:
; bit 0 for drive 1, bit 1 for drive 2.
sendready: ldx #0
srloop: lda readypkt,x
        jsr putb
        inx
        cpx #readyend-readypkt
        bne srloop
        rts

readypkt: .byte $C1, $09, $00, CMDREADY, $C1 ^ $09 ^ $00 ^ CMDREADY
          .byte <PROGVER, >PROGVER, READYVER
readymap: .byte $03                ; probe sets it
readyrom: .byte 0, 0, 0, 0, 0       ; $FBB3, $FBC0, $FBBF, $FB1E, and 1 on a IIgs
readychk: .byte 0
readyend:

; readid: the ROM's identification bytes into Ready, as Apple's technical
; notes list them, and Ready's EOR. A IIgs and an enhanced //e have the same
; bytes; the IIgs's ID call at $FE1F clears the carry, where on a //e that address
; is an RTS. Earlier machines have other code there, so it is called only
; on an enhanced //e's bytes.
readid: lda $FBB3
        sta readyrom
        lda $FBC0
        sta readyrom+1
        lda $FBBF
        sta readyrom+2
        lda $FB1E
        sta readyrom+3
        lda #0
        sta readyrom+4
        lda readyrom
        cmp #$06
        bne ridsum
        lda readyrom+1
        cmp #$E0
        bne ridsum
        sec
        jsr $FE1F
        bcs ridsum
        lda #1
        sta readyrom+4
ridsum: lda #<PROGVER ^ >PROGVER ^ READYVER
        eor readymap
        ldx #4
ridnext: eor readyrom,x
        dex
        bpl ridnext
        sta readychk
        rts

; probe: drive 2 into readymap. A drive at the port reads nibbles from a
; disk, and noise with none, so its data latch changes many times a
; revolution. With no drive at the //c's port the latch is $80 throughout,
; as measured on a //c. Drive 1 is always there: the //c's own drive, or the
; first on a card.
probe:  ldx diskx
        lda DRIVE2,x
        lda MOTORON,x
        lda Q6L,x
        lda Q7L,x
        lda #50                 ; the drive's electronics, powered with the motor
        jsr delayms
        lda #0
        sta tmp                 ; 64 * 256 reads, about a quarter second
        sta pagecnt             ; the reads that differ from the one before
        lda #64
        sta tmp2
        lda #$01
        sta readymap
        lda Q6L,x
        sta acky                ; sendack's byte, free here: the read before
prloop: lda Q6L,x
        cmp acky
        beq prnext
        sta acky
        inc pagecnt
        lda pagecnt
        cmp #16
        bcs prfound
prnext: dec tmp
        bne prloop
        dec tmp2
        bne prloop
        beq prdone
prfound: lda #$03
        sta readymap
prdone: lda MOTOROFF,x
        lda DRIVE1,x
        rts
; sendack: Acknowledge with the kind in A, then X and Y.
sendack: sta tmp
        stx tmp2
        sty acky
        ldx #0
saloop: lda ackhdr,x
        jsr putb
        inx
        cpx #ackend-ackhdr
        bne saloop
        lda tmp
        jsr putb
        lda tmp2
        jsr putb
        lda acky
        jsr putb
        lda tmp
        eor tmp2
        eor acky
        jmp putb

ackhdr: .byte $C1, $03, $00, CMDACK, $C1 ^ $03 ^ $00 ^ CMDACK
ackend:

; sendstatus: Status with the operation, drive, track, progress and error.
sendstatus: ldx #0
ssloop: lda stathdr,x
        jsr putb
        inx
        cpx #statend-stathdr
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
statend:

; sendsector: Sector data with the track, the sector, the raw sector at (dst)
; and (dstp) decoded, and its CRC. The 86 groups are first turned into three
; arrays of ready two-bit values, one per third of the sector; each byte is
; then one shift and one ORA, sent, and added to the EOR and the CRC, all
; inline, in less time than the 6551 takes to send a byte at 115,200 baud.
sendsector: ldy #85
ssaux:  lda (dst),y
        tax
        lda AUXT0,x
        sta AUXA0,y
        lda AUXT1,x
        sta AUXA1,y
        lda AUXT2,x
        sta AUXA2,y
        dey
        bpl ssaux
        ldy #0
        sty crclo
        sty crchi
        ldx #0
sdloop: lda datahdr,x
        jsr putb
        inx
        cpx #dataend-datahdr
        bne sdloop
        lda #0
        sta peor
        lda track
        jsr putpe
        lda sector
        jsr putpe
        ldy #0
        lda #<AUXA0
        sta sdora+1
        lda #>AUXA0
        sta sdora+2
sdpass: ldx #0
sddata: lda (dstp),y             ; the byte: the value << 2 | its two bits
        asl a
        asl a
sdora:  ora AUXA0,x             ; the third's array, set before each pass
        sta sbyte
        ifdef W5100
        jsr putb
        else
sdwait: lda ACIASTAT            ; set for the port at startup
        and #TDRE
        beq sdwait
        lda sbyte
sdsta:  sta ACIADATA            ; set for the port at startup
        endif
        eor peor
        sta peor
        lda sbyte
        eor crchi
        stx tmp2
        tax
        lda crclo
        eor CRCHI,x
        sta crchi
        lda CRCLO,x
        sta crclo
        ldx tmp2
        iny
        beq sddone
        inx
        cpx #86
        bne sddata
        lda sdora+1             ; the next third's array, 86 bytes on
        clc
        adc #86
        sta sdora+1
        bcc sdpass
        inc sdora+2
        jmp sdpass
sddone:
        lda crclo
        jsr putpe
        lda crchi
        jsr putpe
        lda peor
        jmp putb

datahdr: .byte $C1, $04, $01, CMDDATA, $C1 ^ $04 ^ $01 ^ CMDDATA
dataend:

; getsector: the host's Sector data into SECBUF. The carry is set when it
; times out, is not Sector data, is for another sector or fails its CRC.
getsector: jsr getenvt
        bcs gsfail
        cmp #CMDCANCEL
        bne gsdatacmd
        jmp cancel
gsdatacmd: cmp #CMDDATA
        bne gsfail
        jsr getbtq              ; track
        bcs gsfail
        jsr getbtq              ; sector
        bcs gsfail
        cmp sector
        bne gsfail
        ldy #0
gsdata: jsr getbtq
        bcs gsfail
        sta SECBUF,y
        iny
        bne gsdata
        jsr getbtq
        bcs gsfail
        sta tmp
        jsr getbtq
        bcs gsfail
        sta tmp2
        jsr getbtq              ; the payload's EOR; the CRC checks the data
        bcs gsfail
        ldy #0
        sty crclo
        sty crchi
gscrc:  lda SECBUF,y
        jsr crcupd
        iny
        bne gscrc
        lda crclo
        cmp tmp
        bne gsfail
        lda crchi
        cmp tmp2
        bne gsfail
        clc
        rts
gsfail: sec
        rts

; getenvt: an envelope, with a timeout. A and envcmd are the command and
; lenlo and lenhi the payload length, with the carry clear; the carry is set
; on a timeout or a bad check. The host sends a packet in one piece, so after
; its first byte the rest come at once: a byte other than the start is the
; rest of a packet whose start was lost on the line, and the line going quiet
; after it ends the wait at once rather than after the whole timeout.
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

; drain: the bytes still arriving dropped, until a tenth of a second passes
; with none. A damaged packet is found partway through, with the rest of it
; still on the line; a NAK sent then has the host send the packet again
; behind that rest, which is then read as the new packet's start, and so on
; for every try.
drain:  jsr getbtq
        bcc drain
        rts

; getpayload: a command's payload, at most 255 bytes, into PKTBUF, and its
; EOR. The carry is set on a timeout, a bad EOR or a longer payload.
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

; pollcancel: a Cancel the host sent during a read. The host sends nothing
; else then, and while the program is busy with the drive the 6551 has room
; for only the packet's first byte, so any byte is taken as Cancel. A W5100
; takes the one byte the same way.
pollcancel:
        ifdef W5100
        lda rxn
        ora rxn+1
        bne pcany
        ldy #SKRXRSR
        jsr wzsock
        lda WZDATA,x
        ora WZDATA,x
        beq pcdone
pcany:  jsr getbtq
        jmp cancel
        else
        ldx slotx
        lda ACIASTAT,x
        and #RDRF
        beq pcdone
        lda ACIADATA,x
        jmp cancel
        endif
pcdone: rts

; putpe: putb, adding A into peor.
putpe:  pha
        eor peor
        sta peor
        pla
        jmp putb

; crcupd: CRC-16/XMODEM of A into crclo and crchi, by the table. Y is saved and restored.
crcupd: sty savex
        eor crchi
        tay
        lda crclo
        eor CRCHI,y
        sta crchi
        lda CRCLO,y
        sta crclo
        ldy savex
        rts

        ifdef W5100
; getbt: a byte from the W5100, waiting tmolen seconds; the bytes put are
; sent first. The carry is set on a timeout, after which a socket that has
; stopped listening, as when the PC disconnects, listens again. X and Y are saved
; and restored. getbtq waits about a tenth of a second, for a byte inside a
; packet. A poll of the chip takes about 58 cycles, so a second is 71
; rounds of 256 polls.
getbt:  stx savex
        sty wsavy
        lda tmolen
        sta tmo+2
        lda #$B9
        bne gbset               ; always
getbtq: stx savex
        sty wsavy
        lda #1
        sta tmo+2
        lda #$F7                ; 9 rounds
gbset:  sta wrel
        sta tmo+1
        lda #0
        sta tmo
        lda rxn
        ora rxn+1
        bne gbread
        jsr flush
gbpoll: ldy #SKRXRSR
        jsr wzsock
        lda WZDATA,x
        sta rxn+1
        lda WZDATA,x
        sta rxn
        ora rxn+1
        bne gbread
        inc tmo
        bne gbpoll
        inc tmo+1
        bne gbpoll
        lda wrel
        sta tmo+1
        dec tmo+2
        bne gbpoll
        jsr wzstat
        cmp #STESTAB
        beq gbnone
        cmp #STLISTEN
        beq gbnone
        cmp #STSYNRCV
        beq gbnone
        jsr wzlisten
gbnone: ldx savex
        ldy wsavy
        sec
        rts
gbread: lda rxrd+1              ; at $6000 + (Sn_RX_RD & $1FFF)
        and #$1F
        ora #RXMEM
        ldy rxrd
        jsr wzptr
        lda WZDATA,x
        sta wbyte
        inc rxrd
        bne gbnext
        inc rxrd+1
gbnext: lda rxn
        bne gbcount
        dec rxn+1
gbcount: dec rxn
        lda rxn
        ora rxn+1
        bne gbgot
        jsr rxdone
gbgot:  lda wbyte
        ldx savex
        ldy wsavy
        clc
        rts

; putb: A into the chip's transmit memory, sent with the next wait for a
; byte, or once 128 bytes are waiting. A, X and Y are saved and restored.
putb:   stx savex
        sty wsavy
        sta wbyte
        lda txwr+1              ; at $4000 + (Sn_TX_WR & $0FFF)
        and #$0F
        ora #TXMEM
        ldy txwr
        jsr wzptr
        lda wbyte
        jsr wzput
        inc txwr
        bne pbcount
        inc txwr+1
pbcount: inc txn
        bpl pbdone
        jsr flush
pbdone: lda wbyte
        ldx savex
        ldy wsavy
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

; wzlisten: socket 0 closed, opened for TCP on the shim's port, and
; listening.
wzlisten: lda #CMCLOSE
        jsr wzcmd
        ldy #SKMR
        jsr wzsock
        lda #$01                ; TCP
        jsr wzput
        ldy #SKPORT
        jsr wzsock
        lda SHIMPORT
        jsr wzput
        lda SHIMPORT+1
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

; wzput: A at the pointer, which then advances; the entry sets the address.
wzput:  sta WZDATA
        rts

        else
; getbt: a byte through the 6551, waiting about three seconds. The carry is
; set on a timeout. X and Y are saved and restored. getbtq waits about a
; tenth of a second, for a byte inside a packet the host sends in one
; piece: a byte lost on the line then costs a tenth of a second before the
; NAK, not three. A USB serial adapter can hold bytes for 16 ms.
getbt:  stx savex
        ldx slotx
        lda #0
        sta tmo
        sta tmo+1
        lda tmolen
        bne gbset               ; always: tmolen is 1 or more
getbtq: stx savex
        ldx slotx
        lda #0
        sta tmo
        lda #$E0                ; 32 of the 256 rounds
        sta tmo+1
        lda #1
gbset:  sta tmo+2
gbwait: lda ACIASTAT,x
        and #RDRF
        bne gbgot
        inc tmo
        bne gbwait
        inc tmo+1
        bne gbwait
        dec tmo+2
        bne gbwait
        ldx savex
        sec
        rts
gbgot:  lda ACIADATA,x
        ldx savex
        clc
        rts

; putb: a byte through the 6551. X and Y are saved and restored. The store
; is absolute, set for the port at startup: an indexed store reads its
; address first, and a read of the 6551's data register clears its receive
; flag.
putb:   stx savex
        ldx slotx
        pha
pbwait: lda ACIASTAT,x
        and #TDRE
        beq pbwait
        pla
pbsta:  sta ACIADATA
        ldx savex
        rts
        endif


; ---------------------------------------------------------------------------
; Tables
; ---------------------------------------------------------------------------

; mktables: INVTAB from the write table, and the CRC table.
mktables: ldy #0
        lda #0
mkinv0: sta INVTAB,y
        iny
        bne mkinv0
        ldx #63
mkinv1: lda wrtab,x
        tay
        txa
        sta INVTAB,y
        dex
        bpl mkinv1

        ldx #0
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

        ldx #63                 ; for each 6-bit group, its two bits for each
mkaux:  txa                     ; third of the sector, swapped as written
        and #3
        tay
        lda swap2,y
        sta AUXT0,x
        txa
        lsr a
        lsr a
        and #3
        tay
        lda swap2,y
        sta AUXT1,x
        txa
        lsr a
        lsr a
        lsr a
        lsr a
        tay
        lda swap2,y
        sta AUXT2,x
        dex
        bpl mkaux

        ldy #0                  ; encode's tables
mkenc:  tya
        and #3
        tax
        lda swap2,x
        sta ENCT0,y
        asl a
        asl a
        sta ENCT1,y
        asl a
        asl a
        sta ENCT2,y
        iny
        bne mkenc
        rts

; The 6-and-2 write table: the 64 values a data nibble can have.
wrtab:  .byte $96, $97, $9A, $9B, $9D, $9E, $9F, $A6
        .byte $A7, $AB, $AC, $AD, $AE, $AF, $B2, $B3
        .byte $B4, $B5, $B6, $B7, $B9, $BA, $BB, $BC
        .byte $BD, $BE, $BF, $CB, $CD, $CE, $CF, $D3
        .byte $D6, $D7, $D9, $DA, $DB, $DC, $DD, $DE
        .byte $DF, $E5, $E6, $E7, $E9, $EA, $EB, $EC
        .byte $ED, $EE, $EF, $F2, $F3, $F4, $F5, $F6
        .byte $F7, $F9, $FA, $FB, $FC, $FD, $FE, $FF

; A byte's two low bits, swapped, in the auxiliary groups' order.
swap2:  .byte $00, $02, $01, $03


; ---------------------------------------------------------------------------
; The drive
; ---------------------------------------------------------------------------

; motoron: select the drive, start its motor, read mode, and wait for it to
; come up to speed, unless that drive's motor is running already.
motoron: ldx diskx
        lda drive
        cmp spinning
        bne monstart
        lda Q6L,x
        lda Q7L,x
        rts
monstart: sta spinning
        lda drive
        cmp #2
        beq mon2
        lda DRIVE1,x
        jmp mon
mon2:   lda DRIVE2,x
mon:    lda MOTORON,x
        lda Q6L,x
        lda Q7L,x
        lda #250
        jmp delayms

motoroff: ldx diskx
        lda MOTOROFF,x
        lda #0
        sta spinning
        rts

; seek: the arm to `track`. Unknown at first, the arm is taken to track 0
; from as far out as it can be, as the boot ROM does. Then the track is
; checked against an address field, and when the arm turns out to be on
; another track, as DOS also allows for, it steps on from there. On a track
; with no address fields, such as an unformatted one, the arm is not moved
; further.
seek:   jsr seekto
        lda #3
        sta vtries
skcheck: jsr rdaddr
        bcs sknext
        lda atrk
        cmp track
        beq skdone
        asl a
        ldx drive
        sta curhalf-1,x
        jsr seekto
sknext: dec vtries
        bne skcheck
skdone: rts

seekto: ldx drive
        lda curhalf-1,x
        cmp #$FF
        bne seekknown
        lda #80
        sta curhalf-1,x
        lda #0
        sta target
        jsr stepto
seekknown: lda track
        asl a
        sta target
        jsr stepto
        rts

; stepto: one half-track at a time to `target`: the next phase on, a pause,
; the last one off; at the end, a settling pause and the phase off.
stepto: ldx drive
        lda curhalf-1,x
        cmp target
        beq stepdone
        bcc stepup
        sec
        sbc #1
        jmp stepmove
stepup: clc
        adc #1
stepmove: ldy curhalf-1,x       ; the phase that is on
        sta curhalf-1,x
        and #3
        asl a
        ora diskx
        tax
        lda PHASEON,x
        lda #12
        jsr delayms
        tya
        and #3
        asl a
        ora diskx
        tax
        lda PHASEOFF,x
        jmp stepto
stepdone: lda #25
        jsr delayms
        lda curhalf-1,x
        and #3
        asl a
        ora diskx
        tax
        lda PHASEOFF,x
        rts

; protected: the carry is set, with err, when the disk is write-protected.
protected: ldx diskx
        lda Q6H,x
        lda Q7L,x
        asl a
        lda Q6L,x
        bcc wpok
        lda #ERRPROTECT
        sta err
        sec
        rts
wpok:   clc
        rts

; findsector: the address field of `sector` on `track`. The carry is set,
; with err, after enough tries to have passed it several times over.
findsector: lda #48
        sta tries
        lda #0
        sta misses
fsloop: jsr rdaddr
        bcc fshit
        jsr missed
        bcs fsnone
        bcc fsnext
fshit:  lda #0
        sta misses
        lda atrk
        cmp track
        bne fsnext
        lda asec
        cmp sector
        bne fsnext
        clc
        rts
fsnext: dec tries
        bne fsloop
fsnone: lda #ERRNOTFND
        sta err
        sec
        rts

; rdaddr: the next address field, its volume, track, sector and checksum in
; avol to achk. The carry is set when none turns up in about a revolution, or
; its epilogue or its checksum is wrong.
rdaddr: ldx diskx
        lda #0
        sta tmp
        lda #32
        sta tmp2
ra1:    dec tmp
        bne ra1w
        dec tmp2
        beq rafail
ra1w:   lda Q6L,x
        bpl ra1w
ra1c:   cmp #$D5
        bne ra1
ra2:    lda Q6L,x
        bpl ra2
        cmp #$AA
        bne ra1c
ra3:    lda Q6L,x
        bpl ra3
        cmp #$96
        bne ra1c
        ldy #0
ra4:    lda Q6L,x
        bpl ra4
        sec
        rol a
        sta tmp
ra5:    lda Q6L,x
        bpl ra5
        and tmp
        sta avol,y
        iny
        cpy #4
        bne ra4
ra6:    lda Q6L,x               ; the epilogue's first nibble, before it passes
        bpl ra6
        cmp #$DE
        bne rafail
        lda avol
        eor atrk
        eor asec
        cmp achk
        bne rafail
        clc
        rts
rafail: sec
        rts

; readraw: the data field after the address field just read, its 86 two-bit
; groups at (dst) and its 256 six-bit values at (dstp), not yet decoded. The
; carry is set when it is not found or its checksum is wrong. Decoding takes
; longer than the gap to the next sector, so a track read stores the raw form.
readraw: ldx diskx
        ldy #32                 ; the data prologue comes within a few nibbles
rd1:    dey
        beq rrbad
rd1w:   lda Q6L,x
        bpl rd1w
rd1c:   cmp #$D5
        bne rd1
rd2:    lda Q6L,x
        bpl rd2
        cmp #$AA
        bne rd1c
rd3:    lda Q6L,x
        bpl rd3
        cmp #$AD
        bne rd1c

        lda #0                  ; the running value
        ldy #0
rdaux:  sty tmp
rdauxw: ldy Q6L,x
        bpl rdauxw
        eor INVTAB,y
        ldy tmp
        sta (dst),y
        iny
        cpy #86
        bne rdaux
        ldy #0
rdpri:  sty tmp
rdpriw: ldy Q6L,x
        bpl rdpriw
        eor INVTAB,y
        ldy tmp
        sta (dstp),y
        iny
        bne rdpri
rdchk:  ldy Q6L,x
        bpl rdchk
        eor INVTAB,y
        bne rrbad
        clc
        rts
rrbad:  sec
        rts


; readtrack: each requested sector of `track` into RAWBUF, raw, two pages per
; physical sector, taken as its address field passes. The carry is set, with
; err, when some are not read after several revolutions.
readtrack: ldx #15
        lda #0
rtclear: sta PENDING,x
        dex
        bpl rtclear
        ldy #0
rtmark: lda PKTBUF+3,y
        and #15
        tax
        lda #1
        sta PENDING,x
        iny
        cpy count
        bne rtmark
        lda count
        sta left
        lda #96                 ; address fields to look at: six revolutions
        sta tries
        lda #0
        sta misses
rtloop: jsr rdaddr
        bcc rthit
        jsr missed
        bcs rtnone
        bcc rtnext
rthit:  lda #0
        sta misses
        lda atrk
        cmp track
        bne rtnext
        ldx asec
        cpx #16
        bcs rtnext
        lda PENDING,x
        beq rtnext
        stx sector
        jsr rawslot
        jsr readraw
        bcs rtnext
        ldx sector
        lda #0
        sta PENDING,x
        dec left
        beq rtdone
rtnext: dec tries
        bne rtloop
rtnone: ldx #0                  ; the first sector not read, for the error
rtfind: lda PENDING,x
        bne rtfound
        inx
        cpx #15
        bne rtfind
rtfound: stx sector
        lda #ERRNOTFND
        sta err
        sec
        rts
rtdone: clc
        rts

; missed: after rdaddr failed. A whole revolution with no address field,
; after which tmp2 is zero, counts in misses; the carry is set at eight
; in a row, which is no disk or an unformatted one, rather than going on
; through all 96 tries. Eight is past a motor coming up to speed.
missed: lda tmp2
        bne mskeep
        inc misses
        lda misses
        cmp #8
        rts
mskeep: clc
        rts

; rawslot: dst and dstp at `sector`'s two pages in RAWBUF, its 86 groups and
; its 256 values.
rawslot: lda sector
        asl a
        clc
        adc #>RAWBUF
        sta dst+1
        clc
        adc #1
        sta dstp+1
        lda #0
        sta dst
        sta dstp
        rts
; writepass: each of Write sectors' sectors written from its stream in
; WSLOTS as its address field passes, so the sectors that follow one another
; around the track are written in one revolution. The streams are built
; first, so writing can start the moment an address field has passed. The
; carry is set, with err and sector, when some are not written after several
; revolutions.
writepass: ldx #15
        lda #0
wpclear: sta PENDING,x
        dex
        bpl wpclear
        ldy #0
wpmark: lda PKTBUF+3,y
        and #15
        tax
        lda #1
        sta PENDING,x
        iny
        cpy count
        bne wpmark
        lda count
        sta left
        lda #96                 ; address fields to look at: six revolutions
        sta tries
        lda #0
        sta misses
wploop: jsr rdaddr
        bcc wphit
        jsr missed
        bcs wpnone
        bcc wpnext
wphit:  lda #0
        sta misses
        lda atrk
        cmp track
        bne wpnext
        ldx asec
        cpx #16
        bcs wpnext
        lda PENDING,x
        beq wpnext
        lda #0
        sta PENDING,x
        txa
        asl a
        clc
        adc #>WSLOTS
        ldy #<WSTART
        jsr play
        dec left
        beq wpdone
wpnext: dec tries
        bne wploop
wpnone: jmp rtnone              ; the first sector not written, for the error
wpdone: clc
        rts

; encode: SECBUF as 6-and-2: WRAUX's 86 two-bit groups, then WRPRI's 256
; six-bit values, as NibblizationLayer lays them out; the last two groups
; have no third byte. encodeto puts them on page A and the page after it.
encode: lda #>WRAUX
encodeto: sta enauxst+2
        clc
        adc #1
        sta enprist+2
        ldx #0
enaux:  ldy SECBUF,x
        lda ENCT0,y
        ldy SECBUF+86,x
        ora ENCT1,y
        cpx #84
        bcs enauxst
        ldy SECBUF+172,x
        ora ENCT2,y
enauxst: sta WRAUX,x
        inx
        cpx #86
        bne enaux
        ldx #0
enpri:  lda SECBUF,x
        lsr a
        lsr a
enprist: sta WRPRI,x
        inx
        bne enpri
        rts
; slotpage: ptr at the first of `sector`'s two pages in WSLOTS.
slotpage: lda sector
        asl a
        clc
        adc #>WSLOTS
        sta ptr+1
        lda #0
        sta ptr
        rts

; getnibs: the host's Sector nibbles into `sector`'s stream in WSLOTS, at
; its data field's place, from NIBSTART on the first page to NIBEND on the
; second. The carry is set when it times out, is not Sector nibbles, is for
; another sector or fails its CRC. The nibbles are stored as they arrive: at
; 115,200 baud a byte comes every 89 cycles, too few for the CRC as well.
getnibs: jsr getenvt
        bcs gnfail
        cmp #CMDCANCEL
        bne gncmd
        jmp cancel
gncmd:  cmp #CMDNIBS
        bne gnfail
        jsr getbtq              ; track
        bcs gnfail
        jsr getbtq              ; sector
        bcs gnfail
        cmp sector
        bne gnfail
        jsr slotpage
        ldy #<NIBSTART
gnfirst: jsr getbtq
        bcs gnfail
        sta (ptr),y
        iny
        bne gnfirst
        inc ptr+1
gnsecond: jsr getbtq
        bcs gnfail
        sta (ptr),y
        iny
        cpy #NIBEND
        bne gnsecond
        jsr getbtq
        bcs gnfail
        sta tmp
        jsr getbtq
        bcs gnfail
        sta tmp2
        jsr getbtq              ; the payload's EOR; the CRC checks the nibbles
        bcs gnfail
        dec ptr+1
        ldy #0
        sty crclo
        sty crchi
        ldy #<NIBSTART
gncrc1: lda (ptr),y
        jsr crcupd
        iny
        bne gncrc1
        inc ptr+1
gncrc2: lda (ptr),y
        jsr crcupd
        iny
        cpy #NIBEND
        bne gncrc2
        lda crclo
        cmp tmp
        bne gnfail
        lda crchi
        cmp tmp2
        bne gnfail
        clc
        rts
gnfail: sec
        rts

; frameslot: the parts of `sector`'s stream around its nibbles: five
; self-sync bytes from WSTART and the data prologue before them, the
; epilogue and the end mark after, ending at the second page's end.
frameslot: jsr slotpage
        lda #0
        ldy #<WSTART
fssync: sta (ptr),y
        iny
        cpy #<SYNCEND
        bne fssync
        lda #$D5
        sta (ptr),y
        iny
        lda #$AA
        sta (ptr),y
        iny
        lda #$AD
        sta (ptr),y
        inc ptr+1
        ldy #NIBEND
        lda #$DE
        sta (ptr),y
        iny
        lda #$AA
        sta (ptr),y
        iny
        lda #$EB
        sta (ptr),y
        iny
        lda #PEND
        sta (ptr),y
        rts

; cmpnibs: `sector`'s raw values in RAWBUF compared with the nibbles of its
; stream in WSLOTS, decoded as the read decodes them. The carry is set, with
; err, when they differ. The read checked the data field's checksum.
cmpnibs: jsr rawslot
        jsr slotpage
        lda #<NIBSTART
        sta ptr
        lda #0
        sta tmp2
        ldy #0
cnaux:  jsr cnnext
        cmp (dst),y
        bne cnbad
        iny
        cpy #86
        bne cnaux
        ldy #0
cnpri:  jsr cnnext
        cmp (dstp),y
        bne cnbad
        iny
        bne cnpri
        clc
        rts
cnbad:  lda #ERRVERIFY
        sta err
        sec
        rts

; cnnext: the nibble at (ptr) as the read decodes it, EOR the value before
; it, in A and tmp2; ptr on. X is overwritten.
cnnext: ldx #0
        lda (ptr,x)
        tax
        lda INVTAB,x
        eor tmp2
        sta tmp2
        inc ptr
        bne cnndone
        inc ptr+1
cnndone: rts

; putdata: a data field into the stream: D5 AA AD, the 342 nibbles of the
; 86 groups on page A and the 256 values on the page after it, each the EOR
; of its value with the one before, the checksum and DE AA EB. The nibbles
; go through stores whose addresses are stepped in place, in less time than
; putstream takes.
putdata: sta pdaux+2
        clc
        adc #1
        sta pdpri+2
        lda #$D5
        jsr putstream
        lda #$AA
        jsr putstream
        lda #$AD
        jsr putstream
        lda ptr
        sta pdst+1
        lda ptr+1
        sta pdst+2
        lda #0
        sta tmp2                ; the value before
        ldx #0
pdaux:  lda WRAUX,x
        tay
        eor tmp2
        sty tmp2
        tay
        lda wrtab,y
pdst:   sta $FFFF
        inc pdst+1
        bne pdanext
        inc pdst+2
pdanext: inx
        cpx #86
        bne pdaux
        lda pdst+1
        sta pdst2+1
        lda pdst+2
        sta pdst2+2
        ldx #0
pdpri:  lda WRPRI,x
        tay
        eor tmp2
        sty tmp2
        tay
        lda wrtab,y
pdst2:  sta $FFFF
        inc pdst2+1
        bne pdpnext
        inc pdst2+2
pdpnext: inx
        bne pdpri
        lda pdst2+1
        sta ptr
        lda pdst2+2
        sta ptr+1
        ldy tmp2
        lda wrtab,y
        jsr putstream
        lda #$DE
        jsr putstream
        lda #$AA
        jsr putstream
        lda #$EB
        jmp putstream
; putstream: A at (ptr), and ptr on. X and Y are saved and restored.
putstream: sty savex
        ldy #0
        sta (ptr),y
        ldy savex
        inc ptr
        bne psdone
        inc ptr+1
psdone: rts

; buildformat: FSTREAM, a whole track of 26 pages: the first gap, then for
; each sector 0 to 15 its address field, 5 self-sync bytes, a data field and
; fgap self-sync bytes. The data fields are zeros, or with fdata set, the
; sectors in ENCBUF. A sector is 368 bytes and its gap, so the first gap is
; 768 - 16 * fgap self-sync bytes and the stream always fills its pages. The
; stream takes longer to write than a revolution, so its end lands in the
; first gap; the host picks fgap for the drive's measured speed so the 16
; sectors take a little less than a revolution. A revolution at 300 rpm is
; about 204,100 cycles at the //c's speed, and the sectors with gaps of 10
; take about 195,600.
buildformat: ldy #0
        lda #0
bfzero: sta SECBUF,y
        iny
        bne bfzero
        jsr encode
        lda #<FSTREAM
        sta ptr
        lda #>FSTREAM
        sta ptr+1
        lda fgap                ; 768 - 16 * fgap: 16 * fgap in tmp2:tmp
        asl a
        asl a
        asl a
        asl a
        sta tmp
        lda fgap
        lsr a
        lsr a
        lsr a
        lsr a
        sta tmp2
        lda #0
        sec
        sbc tmp
        sta gap1lo
        lda #3
        sbc tmp2
        sta gap1hi
bfgap1: lda gap1lo
        ora gap1hi
        beq bfsecs
        lda #0
        jsr putstream
        lda gap1lo
        bne bfg1dec
        dec gap1hi
bfg1dec: dec gap1lo
        jmp bfgap1
bfsecs:
        lda #0
        sta sector
bfsec:  lda #$D5
        jsr putstream
        lda #$AA
        jsr putstream
        lda #$96
        jsr putstream
        lda volume
        jsr put44
        lda track
        jsr put44
        lda sector
        jsr put44
        lda volume
        eor track
        eor sector
        jsr put44
        lda #$DE
        jsr putstream
        lda #$AA
        jsr putstream
        lda #$EB
        jsr putstream
        ldx #5
bfgap2: lda #0
        jsr putstream
        dex
        bne bfgap2
        lda #>WRAUX             ; zeros, or the sector's own values
        ldx fdata
        beq bfdata
        jsr encpage
bfdata: jsr putdata
        ldx fgap
bfgap3: lda #0
        jsr putstream
        dex
        bne bfgap3
        inc sector
        lda sector
        cmp #16
        bne bfsec
        dec sector              ; the status ends on the last sector, 15
        lda #PEND               ; the end mark, after the stream's 26 pages
        jmp putstream

gap1lo: .byte 0                 ; buildformat: the first gap's count
gap1hi: .byte 0

; put44: A as two 4-and-4 nibbles: the odd bits, then the even.
put44:  sta tmp
        lsr a
        ora #$AA
        jsr putstream
        lda tmp
        ora #$AA
        jmp putstream

; The player's cycle counts are exact only while none of its branches crosses a
; page, which costs a cycle more; it starts a page of its own.
        .align 256

; play: the stream from page A, offset Y, up to its end mark, PEND, written
; by the drive. A 0 in the stream is a self-sync byte; every nibble has bit 7
; set, so one bpl finds both a sync and the end.
;
; The stores are indexed, as in Apple's RWTS: an indexed store reads its
; address in the cycle before it writes it, and the first access starts the
; controller's write loop, so the second is taken as data. The controller
; then takes data only every fourth cycle. An absolute store is a cycle out
; of step, and nothing is written (Sather, Understanding the Apple IIe).
;
; Cycles from one store to the next, each a whole number of 4-cycle bit
; cells: data to data 32; data to sync, sync to sync 40; sync to data 32.
; Every path from a store back to plda is 21 cycles, the one that steps the
; page as well, so a page crossing adds no time. Extra time there would put
; zero bits between two nibbles, three or more in a row, which Apple's own
; disks never have and which a weak drive can read as a 1.
play:   sta plda+2
        ldx diskx
        lda Q6H,x
        lda Q7L,x
        lda #$FF
        sta Q7H,x               ; write mode, a self-sync byte loaded
        ora Q6L,x               ; 4
        jsr dly12               ; 12
        nop                     ; 2
        jmp plda                ; 3: 21 from the store
plda:   lda WSTREAM,y           ; 4   (the page is set above)
        bpl pspec               ; 2 / 3
        sta Q6H,x               ; 5: a data byte 11 after plda
        ora Q6L,x               ; 4   at once, or the latch loads twice
        iny                     ; 2
        beq pcross              ; 2 / 3
        nop                     ; 2
        nop                     ; 2
        nop                     ; 2
        nop                     ; 2
        nop                     ; 2
        jmp plda                ; 3: 21 from the store
pspec:  bne pdone               ; 2 / 3   the end mark
        bit $00                 ; 3
        lda #$FF                ; 2
        sta Q6H,x               ; 5: a self-sync byte 19 after plda
        ora Q6L,x               ; 4
        iny                     ; 2
        beq pcross              ; 2 / 3
        nop                     ; 2
        nop                     ; 2
        nop                     ; 2
        nop                     ; 2
        nop                     ; 2
        jmp plda                ; 3: 21 from the store
pcross: inc plda+2              ; 6
        bit $00                 ; 3
        jmp plda                ; 3: 21 from the store, as without a crossing; Two self-sync bytes follow the stream's last nibble before the write is
; turned off. The //c's IWM writes a stored byte only after the byte before
; it, so a write turned off just after the last store loses the end of the
; stream: a sector's epilogue, and so the sector. The first sync is stored
; 40 cycles after the last nibble, the second 40 after it, and the wait after
; them is past both being written.
pdone:  nop                     ; 31 cycles to here from the last store
        lda #$FF                ; 2
        sta Q6H,x               ; 5: 40 after the last store
        ora Q6L,x               ; 4
        jsr dly12               ; 12
        jsr dly12               ; 12
        nop                     ; 2
        bit $00                 ; 3
        lda #$FF                ; 2
        sta Q6H,x               ; 5: 40 after the first
        ora Q6L,x               ; 4
        jsr dly12               ; 96 cycles: both out of the IWM
        jsr dly12
        jsr dly12
        jsr dly12
        jsr dly12
        jsr dly12
        jsr dly12
        jsr dly12
        lda Q7L,x               ; read mode
        lda Q6L,x
        rts
dly12:  rts                     ; jsr and rts: 12 cycles


; ---------------------------------------------------------------------------
; Timing
; ---------------------------------------------------------------------------

; delayms: about A milliseconds at the //c's 1.02 MHz. Y is saved and
; restored, and X is not used.
delayms: sty savex
dlouter: ldy #203
dlinner: dey
        bne dlinner
        sec
        sbc #1
        bne dlouter
        ldy savex
        rts


; ---------------------------------------------------------------------------
; The status screen
; ---------------------------------------------------------------------------

ROW0 = $0400                    ; the title
ROW2 = $0500                    ; the connection
ROW3 = $0580                    ; the PC
ROWST = $0680                   ; row 5: the status, after a blank row
ROWLB = $0700                   ; row 6: the label of the user's operation
ROWER = $0428                   ; row 8: the last error, when there is one
ROWHIST = $0528                 ; row 10: the history's title, its lines below
LOGROWS = 12                    ; the history's lines, rows 11 to 22
ROWESC = $07D0 + 11             ; row 23, centered: how to leave CassoLink
PCGONE = 5                      ; seconds with no reply before the PC is gone
IDLEOFF = 18                    ; Idles, 110 ms apart, before the motor stops
FIELD = 12
FIELDLEN = 20
scr = $13                       ; and $14: the field being drawn
vtries = $15                    ; seek: address fields left to check
acky = $16                      ; sendack: its third byte
pcflag = $17                    ; nonzero once the PC has replied
silent = $18                    ; seconds since its last reply
tmolen = $19                    ; getbt's timeout, in about seconds
left = $1A                      ; readtrack: sectors not yet read
spinning = $1B                  ; the drive whose motor is running, or 0
dst = $1C                       ; and $1D: a raw sector's 86 groups
dstp = $1E                      ; and $1F: its 256 values
sbyte = $20                     ; sendsector: the byte being sent
pct = $21                       ; showstatus: the percentage
pcsum = $22                     ; and $23: percent's running sum

; drawscreen: clear the text page and draw the labels.
drawscreen: lda #$A0
        ldy #0
dsclear: sta $0400,y
        sta $0500,y
        sta $0600,y
        sta $0700,y
        iny
        bne dsclear
        ldx #0
dsline: lda labels+1,x          ; row low, row high, text, 0; a 0 row ends
        beq dsdone
        sta ptr+1
        lda labels,x
        sta ptr
        inx
        inx
        ldy #0
dstext: lda labels,x
        beq dsnext
        ora #$80
        sta (ptr),y
        inx
        iny
        bne dstext
dsnext: inx
        jmp dsline
dsdone: jsr showconn
        jsr showpc
        jmp showstatus

labels: .byte <ROW0, >ROW0
        .text "CASSOLINK"
        .byte 0
        .byte <ROW2, >ROW2
        .text "CONNECTION"
        .byte $3A, 0
        .byte <ROW3, >ROW3
        .text "PC"
        .byte $3A, 0
        .byte <ROWHIST, >ROWHIST
        .text "HISTORY"
        .byte $3A, 0
        .byte <ROWESC, >ROWESC
        .text "PRESS ESC TO EXIT"
        .byte 0
        .byte 0
        .byte 0, 0

; field: scr at the field of the row whose address is in X (low) and A
; (high), and the field cleared. Y is 0.
field:  stx scr
        sta scr+1
        lda scr
        clc
        adc #FIELD
        sta scr
        ldy #FIELDLEN-1
        lda #$A0
fdclear: sta (scr),y
        dey
        bpl fdclear
        iny
        rts

; puttext: the 0-terminated text at (ptr) into the field from Y.
puttext: sty tmp
        ldy #0
pttext: lda (ptr),y
        beq ptdone
        sty tmp2
        ldy tmp
        jsr putch
        sty tmp
        ldy tmp2
        iny
        bne pttext
ptdone: ldy tmp
        rts

; putch: A into the field at Y, and Y on. Nothing is drawn at a Y of 40 or
; more, so a text from a row's start is never drawn on the row below it.
putch:  cpy #40
        bcs pcnext
        ora #$80
        sta (scr),y
pcnext: iny
        rts

; putdec: A in decimal, without leading zeros, into the field from Y.
putdec: ldx #0                  ; hundreds
pdh:    cmp #100
        bcc pdh2
        sbc #100
        inx
        bne pdh
pdh2:   sta tmp2
        txa
        beq pdt
        ora #'0'
        jsr putch
        lda #1                  ; a hundreds digit forces the tens
        sta tmp
        bne pdt1
pdt:    sta tmp
pdt1:   lda tmp2
        ldx #0
pdtl:   cmp #10
        bcc pdt2
        sbc #10
        inx
        bne pdtl
pdt2:   sta tmp2
        txa
        ora tmp
        beq pdu
        txa
        ora #'0'
        jsr putch
pdu:    lda tmp2
        ora #'0'
        jmp putch

; showconn: the port and the speed the 6551 is set to: a //c's port, or a
; card's slot.
showconn: ldx #<ROW2
        lda #>ROW2
        jsr field
        lda #<conntext
        sta ptr
        lda #>conntext
        sta ptr+1
        jsr puttext
        ifdef W5100
        lda slotx               ; the card's slot and the port, as
        lsr a                   ; SLOT 3, PORT 6502
        lsr a
        lsr a
        lsr a
        jsr putdec
        lda #<connport
        sta ptr
        lda #>connport
        sta ptr+1
        jsr puttext
        lda SHIMPORT+1
        sta wnum
        lda SHIMPORT
        sta wnum+1
        jmp putword
        else
        lda #<connslot
        sta ptr
        lda #>connslot
        sta ptr+1
        lda $FBB3               ; a //c has $06 here and $00 at $FBC0
        eor #$06
        ora $FBC0
        bne scslot
        lda #<connport
        sta ptr
        lda #>connport
        sta ptr+1
scslot: jsr puttext
        lda slotx
        lsr a
        lsr a
        lsr a
        lsr a
        jsr putdec
        lda #<conncomma
        sta ptr
        lda #>conncomma
        sta ptr+1
        jsr puttext
        ldx slotx
        lda ACIACTL,x
        and #$0F
        ldx #0
scfind: cmp speeds,x
        beq scfound
        inx
        inx
        inx
        cpx #speedend-speeds
        bne scfind
scfound: lda speeds+1,x
        sta ptr
        lda speeds+2,x
        sta ptr+1
        jmp puttext
        endif

        ifdef W5100
conntext: .text "SLOT "
        .byte 0
connport: .byte $2C
        .text " PORT "
        .byte 0

; putword: wnum in decimal, without leading zeros, into the field from Y.
putword: lda #0
        sta wlead
        ldx #4
pwdigit: lda #0
        sta wdig
pwsub:  lda wnum
        sec
        sbc pwtablo,x
        sta wbyte
        lda wnum+1
        sbc pwtabhi,x
        bcc pwout
        sta wnum+1
        lda wbyte
        sta wnum
        inc wdig
        bne pwsub
pwout:  lda wdig
        ora wlead
        bne pwput
        cpx #0
        bne pwnext
pwput:  lda wdig
        ora #'0'
        sta wlead
        jsr putch
pwnext: dex
        bpl pwdigit
        rts

pwtablo: .byte <1, <10, <100, <1000, <10000
pwtabhi: .byte >1, >10, >100, >1000, >10000
        else
conntext: .byte 0
connport: .text "PORT "
        .byte 0
connslot: .text "SLOT "
        .byte 0
conncomma: .text ", "
        .byte 0
        endif


        ifndef W5100
s9600:   .text "9600"
         .byte 0
s19200:  .text "19200"
         .byte 0
s115200: .text "115200"
         .byte 0
s4800:   .text "4800"
         .byte 0
s2400:   .text "2400"
         .byte 0
s1200:   .text "1200"
         .byte 0
sother:  .text "OTHER"
         .byte 0
; The 6551's rate bits and their text; the last entry matches any other.
; After the text, as the assembler sizes a forward < or > in .byte wrongly.
speeds: .byte $0E, <s9600, >s9600
        .byte $0F, <s19200, >s19200
        .byte $00, <s115200, >s115200
        .byte $0C, <s4800, >s4800
        .byte $0A, <s2400, >s2400
        .byte $08, <s1200, >s1200
speedend:
        .byte $FF, <sother, >sother
        endif

; showstatus: the status row: the operation, and while one runs its drive,
; track and sector and how much of the request is done, as
; STATUS: READING D2, T17, S5 (45%). Set speed has no drive. The other
; labels are the points that update it.
showop:
showdrive:
showtrack:
showprog:
showstatus: jsr percent
        sta pct
        ldx #<ROWST
        lda #>ROWST
        jsr fullrow
        lda #<sttext
        sta ptr
        lda #>sttext
        sta ptr+1
        jsr puttext
        lda op
        asl a
        tax
        lda optext,x
        sta ptr
        lda optext+1,x
        sta ptr+1
        lda op                  ; idle with no PC: waiting for the connection,
        ora pcflag              ; not for a command
        bne stput
        lda #<onopc
        sta ptr
        lda #>onopc
        sta ptr+1
stput:  jsr puttext
        lda op
        beq stdone
        cmp #OPSPEED
        bne strun
stdone: rts
strun:  lda #<stdrive
        sta ptr
        lda #>stdrive
        sta ptr+1
        jsr puttext
        lda drive
        jsr putdec
        lda #<sttrack
        sta ptr
        lda #>sttrack
        sta ptr+1
        jsr puttext
        lda track
        jsr putdec
        lda #<stsector
        sta ptr
        lda #>stsector
        sta ptr+1
        jsr puttext
        lda sector
        jsr putdec
        lda #' '
        jsr putch
        lda #'('
        jsr putch
        lda pct
        jsr putdec
        lda #'%'
        jsr putch
        lda #')'
        jmp putch

sttext:   .text "STATUS: "
          .byte 0
stdrive:  .text " D"
          .byte 0
sttrack:  .byte $2C
          .text " T"
          .byte 0
stsector: .byte $2C
          .text " S"
          .byte 0

optext: .word oidle, oread, owrite, oformat, ospeed

oidle:   .text "WAITING FOR A COMMAND"
         .byte 0
onopc:   .text "WAITING FOR CONNECTION"
         .byte 0
oread:   .text "READING"
         .byte 0
owrite:  .text "WRITING"
         .byte 0
oformat: .text "FORMATTING"
         .byte 0
ospeed:  .text "SETTING SPEED"
         .byte 0

; percent: the whole operation's percentage in A, plo plus index / count of
; the way from plo to phi, or plo with no count. Y is overwritten.
percent: lda #0
        sta pcsum
        sta pcsum+1
        ldx index
        beq pcdiv
pcmul:  lda phi
        sec
        sbc plo
        clc
        adc pcsum
        sta pcsum
        bcc pcmnext
        inc pcsum+1
pcmnext: dex
        bne pcmul
pcdiv:  ldx #0
        lda count
        beq pctdone
pcsub:  lda pcsum
        sec
        sbc count
        tay
        lda pcsum+1
        sbc #0
        bcc pctdone
        sta pcsum+1
        sty pcsum
        inx
        bne pcsub
pctdone: txa
        clc
        adc plo
        rts

; fullrow: scr at the row whose address is in X (low) and A (high), and the
; whole row cleared. Y is 0.
fullrow: stx scr
        sta scr+1
        ldy #39
        lda #$A0
frclear: sta (scr),y
        dey
        bpl frclear
        iny
        rts

; showpc: the PC: waiting for its first reply, connected, or how long since
; its last reply.
showpc: ldx #<ROW3
        lda #>ROW3
        jsr field
        lda silent
        bne spquiet
        lda #<pcwait
        ldx #>pcwait
        ldy pcflag
        beq sptext
        lda #<pcconn
        ldx #>pcconn
sptext: sta ptr
        stx ptr+1
        ldy #0
        jmp puttext
spquiet: lda #<pcnone
        sta ptr
        lda #>pcnone
        sta ptr+1
        ldy #0
        jsr puttext
        lda silent
        jsr putdec
        lda #' '
        jsr putch
        lda #'S'
        jmp putch

; pcok: a reply arrived. The line is drawn again only when it changes, as a
; command's reply follows. A first reply, or one after a silence, is a new
; connection, which starts with no error.
pcok:   lda pcflag
        beq pcnew
        lda silent
        beq pcsame
pcnew:  lda #1
        sta pcflag
        lda #0
        sta silent
        sta err
        jsr showerr
        jmp showpc
pcsame: rts

; nopc: a second with no reply. The host ends its session after a few
; seconds with no packet, so after PCGONE of them the PC is gone: the label
; of its operation is cleared, and the PC is waited for again. Before a
; first reply there is no silence to count.
nopc:   lda pcflag
        beq npdraw
        inc silent
        lda silent
        cmp #PCGONE
        bcc npdraw
        lda #0
        sta pcflag
        sta silent
        jsr clrlabel
npdraw: jmp showpc

pcwait: .text "WAITING"
        .byte 0
pcconn: .text "CONNECTED"
        .byte 0
pcnone: .text "NO REPLY FOR "
        .byte 0

; showerr: the last operation's error, as ERROR: WRITE-PROTECTED DISK IN D2 or
; ERROR: BAD DATA D2, T17, S5, or the row cleared when it succeeded.
showerr: ldx #<ROWER
        lda #>ROWER
        jsr fullrow
        lda err
        beq sedone
        lda #<ertext
        sta ptr
        lda #>ertext
        sta ptr+1
        jsr puttext
        lda err
        lsr a
        lsr a
        lsr a
        cmp #errtextend-errtext
        bcc seknown
        lda #0
seknown: tax
        lda errtext,x
        sta ptr
        lda errtext+1,x
        sta ptr+1
        jsr puttext
        lda err                 ; write protection is the disk's: its drive
        cmp #ERRPROTECT
        bne senotwp
        lda drive
        jmp putdec
senotwp: cmp #ERRNOTFND          ; a sector that cannot be read or verified:
        bcc sedone              ; where it is, as the status row has it
        cmp #ERRLINK
        bcs sedone
        lda #<stdrive
        sta ptr
        lda #>stdrive
        sta ptr+1
        jsr puttext
        lda drive
        jsr putdec
        lda #<sttrack
        sta ptr
        lda #>sttrack
        sta ptr+1
        jsr puttext
        lda track
        jsr putdec
        lda #<stsector
        sta ptr
        lda #>stsector
        sta ptr+1
        jsr puttext
        lda sector
        jmp putdec
sedone: rts
; The error codes are $10 apart, so the code shifted right three times is
; the index of its text's address; the first entry is for any other code.
ertext:  .text "ERROR"
         .byte $3A, $20, 0

errtext: .word eother, eprotect, enotfnd, edata, everify, elink, edrive
errtextend:

eother:   .text "FAILED"
          .byte 0
eprotect: .text "WRITE-PROTECTED DISK IN D"
          .byte 0
enotfnd:  .text "NO SECTOR"
          .byte 0
edata:    .text "BAD DATA"
          .byte 0
everify:  .text "VERIFY FAILED"
          .byte 0
elink:    .text "LINK DATA DAMAGED"
          .byte 0
edrive:   .text "NO SUCH DRIVE"
          .byte 0

; dolabel: the host's text for the user's operation across the label row,
; then Ready again.
dolabel: jsr clrlabel
lbtext: cpy lenlo
        beq lbdone
        cpy #40
        beq lbdone
        lda PKTBUF,y
        ora #$80
        sta ROWLB,y
        iny
        bne lbtext
lbdone: jmp main

; clrlabel: the label row cleared. Y is 0.
clrlabel: ldy #39
        lda #$A0
lbclear: sta ROWLB,y
        dey
        bpl lbclear
        iny
        rts

; dolog: the history moved down a row, its last line dropped, and the PC's
; line at its top: the time as HH:MM, then the text. Then Ready again.
dolog:  ldx #LOGROWS-1
lgscroll: lda logrowlo-1,x
        sta ptr
        lda logrowhi-1,x
        sta ptr+1
        lda logrowlo,x
        sta scr
        lda logrowhi,x
        sta scr+1
        ldy #39
lgcopy: lda (ptr),y
        sta (scr),y
        dey
        bpl lgcopy
        dex
        bne lgscroll
        ldx logrowlo
        lda logrowhi
        jsr fullrow
        lda PKTBUF
        jsr put2
        lda #':'
        jsr putch
        lda PKTBUF+1
        jsr put2
        lda #' '
        jsr putch
        ldx #2
lgtext: cpx lenlo
        beq lgdone
        cpy #40
        beq lgdone
        lda PKTBUF,x
        jsr putch
        inx
        bne lgtext
lgdone: jmp main

; put2: A, 0 to 99, as two digits from Y. X is overwritten.
put2:   ldx #0
p2tens: cmp #10
        bcc p2ones
        sbc #10
        inx
        bne p2tens
p2ones: sta tmp2
        txa
        ora #'0'
        jsr putch
        lda tmp2
        ora #'0'
        jmp putch

; The history's lines, rows 11 to 22 of the text page.
logrowlo: .byte $A8, $28, $A8, $28, $A8, $50, $D0, $50, $D0, $50, $D0, $50
logrowhi: .byte $05, $06, $06, $07, $07, $04, $04, $05, $05, $06, $06, $07
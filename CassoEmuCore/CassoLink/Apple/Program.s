; Casso's CassoLink: the program.
;
; Loaded at $0800 by the shim. It sends Ready, with its version, the shim's,
; its drive map, the ROM's identification and a CRC of the program as loaded,
; and the host's reply is the next operation:
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
;   Exit            CassoLink ends, and control returns to BASIC
;   Session screen  whether a Full Send is active, its tone, and its rows' text,
;                   each ending in a zero: drawn, the tone sounded, and Ready
;                   again; not active clears them and ends the session
;   Read CRCs       drive, track, count, sectors: Sector CRCs, each sector's CRC
;                   and whether it was read, computed here so a read again
;                   crosses the link in a few bytes; then the status
;   Set columns     1 for 80 columns where the machine has them, 0 for 40: the
;                   screen drawn again, and Ready again
;   Wait for key    a Full Send's pause, with the prompt's lines, each ending in a
;                   zero: drawn with the motor off,
;                   Key with its code on Space, Q, 1 or 2, Key with 0 each
;                   second until then; any byte from the host is taken as Cancel
;   Krunch          drive, and the CRC-16 of the Apple Pascal directory the host
;                   checked, low byte first: the disk's files moved toward its
;                   start, all here, so a dropped link doesn't stop it partway;
;                   Krunch progress, with the percentage and the files moved,
;                   once the directory is read and after each part of a file;
;                   a Cancel is taken only between files
;
; F on the standard screen sends Key with its code ahead of the next Ready,
; and the host starts a Full Send. During a Full Send Q does the same, and the
; session ends and returns to the standard screen; Escape is ignored, so that a
; second press cannot end CassoLink.
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
; The status screen on the text page has the connection and the PC, connected
; or how long since its last reply; the last Label; each drive's state and its
; bar, one cell a track, with the track and sector over it; the last error,
; until an operation succeeds; and the history, the host's Log lines with the
; newest at the top. During a krunch a map of the disk's blocks is drawn over
; the history, which is drawn again when the krunch ends.
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
CMDBLANK  = $E0                 ; Write track's sector of zeros: its numbers alone
CMDSCREEN = $E1                 ; a Full Send's screen
CMDCOLS   = $E3                 ; 40 or 80 columns
CMDREADCRC = $E4                ; Read CRCs
CMDCRCS   = $E5                 ; and the CRCs in reply
CMDKEY    = $E2                 ; Wait for key, and the key in reply
CMDFAULT  = $E6                 ; Debug builds: a fault for the next operation
CMDKRUNCH = $E7                 ; Krunch
CMDKPROG  = $E8                 ; and its progress in reply

; Fault's kinds, in Debug builds alone. The first three change a byte the
; Apple sends, counted from the operation's first: flipped, dropped, or sent
; after a stall. The others change a sector: not read, sent with a bit of
; its data flipped and the CRC computed for the change, or given a wrong CRC
; in reply to Read CRCs.
FLTFLIP   = 1
FLTDROP   = 2
FLTSTALL  = 3
FLTUNREAD = 4
FLTDATA   = 5
FLTCRC    = 6

SHIM      = $3B00
SHIMSPEED = SHIM + $02            ; the shim's 6551 control byte
SHIMTEXT  = SHIM + $06            ; its speed's text, six characters
SHIMDIAL  = SHIM + $0C            ; a Wi-Fi modem's dial command
SHIMADDR  = SHIM + $0C            ; a W5100 shim's address
SHIMPORT  = SHIM + $1E            ; a W5100 shim's TCP port, high byte first
SHIMSAVED = SHIM + $34            ; nonzero once the shim saved the machine's state
SHIMRESET = SHIM + $35            ; the reset vector the shim found, and its check
ZPSAVE    = $3A00                 ; the zero page the shim found, below the shim
ACK       = $06
NAK       = $15

; Operations, as the Status packet and the screen report them.
OPIDLE   = 0
OPREAD   = 1
OPWRITE  = 2
OPFORMAT = 3
OPSPEED  = 4
OPDISK   = 5                    ; a Full Send's pause, on the status row alone

QUIETDRIVE = $80                ; the drive's bit for a check the PC reports itself

; Errors.
ERRNONE    = $00
ERRPROTECT = $10                ; the disk is write-protected
ERRNOTFND  = $20                ; no address field for the sector
ERRDATA    = $30                ; the data field did not decode
ERRVERIFY  = $40                ; the sector read back differently
ERRLINK    = $50                ; the host's data did not arrive whole
ERRDRIVE   = $60                ; no such drive
ERRCHANGED = $70                ; Krunch: the directory differs from the host's

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
KFLO     = $4400                ; Krunch: each directory entry's first block,
KFHI     = $4480                ; by its slot, low and high bytes,
KNLO     = $4500                ; and the block after its last
KNHI     = $4580
KHIST    = $9300                ; Krunch: the history's rows while the block map
                                ; is drawn over them, main memory's 320 bytes and
                                ; then auxiliary memory's, to $957F
KCHAR    = $4E10                ; Krunch: each block's character in the block map,
                                ; a letter of its file's name or a space, to $4F27
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
vflag    = $0E                  ; nonzero while a written track is read back
fsmode   = $0F                  ; nonzero during a Full Send
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
        jsr mktables            ; the CRC table, for selfcrc
        jsr selfcrc             ; ahead of the stores below into the program's code
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
        sta vflag
        sta fsmode
        lda #3
        sta tmolen
        lda #<exitlink          ; Control-Reset ends CassoLink cleanly
        sta $03F2
        lda #>exitlink
        sta $03F3
        eor #$A5
        sta $03F4
        jsr probe
        jsr readid
        jsr drawscreen

main:
        ifdef FAULTS
        lda #0                  ; a byte's fault lasts for one operation
        sta fltact
        lda fltdone             ; and a sector's until its sector is read
        beq mnarmed
        lda #0
        sta fltpend
        sta fltdone
mnarmed:
        endif
        lda $C000               ; Escape on the Apple ends CassoLink
        cmp #$9B
        beq mnesc
        cmp #$C6                ; F or f asks for a Full Send
        beq mnfull
        cmp #$E6
        beq mnfull
        cmp #$D6                ; V or v turns a Full Send's verification on or off
        beq mnverify
        cmp #$F6
        beq mnverify
        cmp #$D1                ; Q or q ends a Full Send
        beq mnquit
        cmp #$F1
        bne mnokey
mnquit: ldx fsmode
        beq mnokey
        sta $C010
        lda #$D1
        jsr sendkey
        jmp mnokey
mnverify: ldx fsmode
        beq mnokey
        sta $C010
        lda #$D6
        jsr sendkey
        jmp mnokey
mnfull: ldx fsmode
        bne mnokey
        sta $C010
        lda #$C6
        jsr sendkey
        jmp mnokey
mnesc:  sta $C010
        ldx fsmode
        bne mnokey
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
        ifdef FAULTS
        bcc mrgot               ; main is out of a branch's range
        jmp main
mrgot:
        else
        bcs main
        endif
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
        bne notexit
        jmp exitlink
notexit: cmp #CMDSCREEN
        bne notscreen
        jmp doscreen
notscreen: cmp #CMDCOLS
        bne notcols
        jmp docols
notcols: cmp #CMDREADCRC
        bne notcrc
        jmp docrcs
notcrc: cmp #CMDKEY
        bne notkey
        jmp dowaitkey
notkey: cmp #CMDKRUNCH
        bne notkrunch
        jmp dokrunch
notkrunch:
        ifdef FAULTS
        cmp #CMDFAULT
        bne tomain
        jmp dofault
        endif
tomain: jmp main

; exitlink: CassoLink ends, by Control-Reset, Escape or the PC's Exit, and
; control returns to BASIC. The drive is in read mode with its motor off and
; the 6551's DTR off, so the PC's DSR drops; the zero page and reset
; vector the shim saved are put back; BASIC's program is emptied as NEW
; does, since the program was loaded over it; input and output are the
; keyboard and screen; and DOS's warm start follows, or Applesoft's warm
; start when there is no DOS.
exitlink: sei
        cld
        jsr col40
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
        bne lvwarm
        lda $03D2
        cmp #$9D
        bcc lvwarm
        jmp $03D0
lvwarm: jmp $E003               ; Applesoft's warm start, at its ] prompt

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
; speed the host sends the program at; and with the dial command marked as
; dialed, as a modem link is connected already. A W5100's connection is left open, with what
; was read given back and what was put sent.
        ifdef W5100
doload: jsr col40
        jsr flush
        jsr rxdone
        jmp SHIM
        else
doload: jsr col40
        ldx slotx
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
dlgo:   lda SHIMDIAL            ; the dial command marked as dialed, which the
        beq dlnone              ; shim skips and the program reads
        ora #$80
        sta SHIMDIAL
dlnone: jmp SHIM
        endif
; cancel: the operation stops where it is, and the loop starts again.
cancel: jsr motoroff
        jsr kmapoff             ; the history again, after a krunch's block map
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
; sending in turn would wait most of a revolution for every sector. A sector
; not read is skipped and the status has NOT FOUND. The PC then requests
; only those sectors again.
doread: lda #OPREAD
        jsr begin
        bcs opdone
        jsr readtrack
rdnext: jsr showprog
        ldy index
        lda PKTBUF+3,y
        sta sector
        and #15
        tax
        lda PENDING,x
        bne rdskip
        jsr rawslot
        ifdef FAULTS
        lda #FLTDATA            ; a bit of the data flipped before it is sent
        jsr fltmatch
        bne rdsend
        ldy #0
        lda (dstp),y
        eor #$01
        sta (dstp),y
rdsend:
        endif
        jsr sendsector
rdskip: jsr pollcancel
        inc index
        lda index
        cmp count
        bne rdnext
        jsr showprog
        lda err
        beq opdone
        jsr firstpend
; The motor runs on after an operation, so the next track's read does not wait
; for it to spin up again; Idle, with nothing more to do, stops it. During a
; Full Send the error row stays clear: an empty drive's check fails with NO
; SECTOR, and the drive's row has EMPTY OR NO RECOGNIZED SECTORS for it. The
; same is true of any operation on a drive just marked that way, such as the
; PC's read of an empty drive's catalog when it connects, and of a check the PC
; reports itself.
opdone: jsr notedisk
        lda fsmode
        bne odstatus
        lda quiet
        bne odstatus
        ldx drive
        dex
        cpx #2
        bcs odshow
        lda nodisk,x
        bne odstatus
odshow: jsr showerr
odstatus: jsr sendstatus
        jmp main

; notedisk: the drive's EMPTY OR NO RECOGNIZED SECTORS, set by an operation that found no
; address field for eight revolutions in a row, which is no disk or an
; unformatted one, and cleared by one that succeeded.
notedisk: ldx drive
        dex
        cpx #2
        bcs nddone
        lda misses
        cmp #8
        bcc ndok
        lda #1
        sta nodisk,x
        rts
ndok:   lda err
        bne nddone
        sta nodisk,x
nddone: rts

nodisk: .byte 0, 0
quiet:  .byte 0                 ; nonzero for a check the PC reports itself

; Read CRCs: the requested sectors read as for Read sectors, and each one's CRC
; sent with a flag for whether it was read. A sector not read isn't an error here,
; as the host is counting them.
docrcs: lda #OPREAD
        jsr begin
        ifdef FAULTS
        bcc crbegun             ; crdone is out of a branch's range
        jmp crdone
crbegun:
        else
        bcs crdone
        endif
        jsr readtrack
        lda #ERRNONE
        sta err
        lda count
        asl a
        clc
        adc count
        sta tmp
        lda #$C1
        jsr putb
        lda tmp
        jsr putb
        lda #0
        jsr putb
        lda #CMDCRCS
        jsr putb
        lda tmp
        eor #$C1 ^ CMDCRCS
        jsr putb
        lda #0
        sta peor
crnext: jsr showprog
        ldy index
        lda PKTBUF+3,y
        and #15
        sta sector
        tax
        lda PENDING,x
        bne crmiss
        jsr rawslot
        jsr sectorcrc
        ifdef FAULTS
        lda #FLTCRC
        jsr fltmatch
        bne crsend
        lda crclo
        eor #$01
        sta crclo
crsend:
        endif
        lda crclo
        jsr putpe
        lda crchi
        jsr putpe
        lda #1
        jsr putpe
        jmp cradv
crmiss: lda #0
        jsr putpe
        lda #0
        jsr putpe
        lda #0
        jsr putpe
cradv:  jsr pollcancel
        inc index
        lda index
        cmp count
        bne crnext
        lda peor
        jsr putb
crdone: jmp opdone

; sectorcrc: crclo and crchi for the sector at dst and dstp, from its raw values
; as sendsector sends them.
sectorcrc: ldy #85
scaux:  lda (dst),y
        tax
        lda AUXT0,x
        sta AUXA0,y
        lda AUXT1,x
        sta AUXA1,y
        lda AUXT2,x
        sta AUXA2,y
        dey
        bpl scaux
        ldy #0
        sty crclo
        sty crchi
        lda #<AUXA0
        sta scora+1
        lda #>AUXA0
        sta scora+2
scpass: ldx #0
scdata: lda (dstp),y
        asl a
        asl a
scora:  ora AUXA0,x
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
        beq scdone
        inx
        cpx #86
        bne scdata
        lda scora+1
        clc
        adc #86
        sta scora+1
        bcc scpass
        inc scora+2
        jmp scpass
scdone: rts

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
        jsr startverify
        jsr verifysecs
        jsr endverify
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
        jsr startverify
        jsr verifytrack
        jsr endverify
        jsr showprog
        jmp opdone

; startverify and endverify: the status row reads VERIFYING while a written
; track is read back. endverify saves and restores the carry flag, which
; is verify's result.
startverify: lda #1
        sta vflag
        jmp showstatus
endverify: php
        lda #0
        sta vflag
        plp
        rts

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
; with err when the drive is not 1 or 2. Bit 7 of the drive marks a check the
; PC reports itself, such as its read of a disk's catalog when it connects:
; the operation's error is not drawn.
begin:  sta op
        ifdef FAULTS
        lda fltpend             ; the fault armed for this operation
        sta fltact
        cmp #FLTUNREAD          ; a sector's stays armed until its sector is read
        bcs bgarmed
        lda #0
        sta fltpend
bgarmed:
        endif
        lda #ERRNONE
        sta err
        lda #0
        sta index
        sta idles
        sta misses
        lda PKTBUF
        and #QUIETDRIVE
        sta quiet
        lda PKTBUF
        and #$7F
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
drvok:  ldx drive               ; the drive's FAILED, cleared by its next operation
        lda #0
        sta barfail-1,x
        jsr motoron
        jsr seek
        clc
        rts


; ---------------------------------------------------------------------------
; Packets
; ---------------------------------------------------------------------------

; sendready: Ready with this program's version, the shim's and the drives:
; bit 0 for drive 1, bit 1 for drive 2. Then the ROM's identification bytes,
; and the program's CRC as selfcrc computed it.
sendready: ldx #0
srloop: lda readypkt,x
        jsr putb
        inx
        cpx #readyend-readypkt
        bne srloop
        rts

readypkt: .byte $C1, $0B, $00, CMDREADY, $C1 ^ $0B ^ $00 ^ CMDREADY
          .byte <PROGVER, >PROGVER, READYVER
readymap: .byte $03                ; probe sets it
readyrom: .byte 0, 0, 0, 0, 0       ; $FBB3, $FBC0, $FBBF, $FB1E, and 1 on a IIgs
readycrc: .byte 0, 0                ; selfcrc sets it, low byte first
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
        ldx #6                  ; readyrom's 5 bytes and readycrc's 2
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
        ifdef FAULTS
        lda fltact              ; through putb while a fault is armed
        beq sdwait
        lda sbyte
        jsr putb
        jmp sdsent
        endif
sdwait: lda ACIASTAT            ; set for the port at startup
        and #TDRE
        beq sdwait
        lda sbyte
sdsta:  sta ACIADATA            ; set for the port at startup
        ifdef FAULTS
sdsent:
        endif
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
gsdatacmd: cmp #CMDBLANK
        beq gsblank
        cmp #CMDDATA
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

; A sector of zeros: its track and sector, then the payload's EOR, and SECBUF
; filled with zeros here rather than sent.
gsblank: jsr getbtq             ; track
        bcs gsfail
        jsr getbtq              ; sector
        bcs gsfail
        cmp sector
        bne gsfail
        jsr getbtq              ; the payload's EOR
        bcs gsfail
        lda #0
        tay
gszero: sta SECBUF,y
        iny
        bne gszero
        clc
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

; selfcrc: the CRC-16/XMODEM of the program as loaded, $0800 up to progend,
; into readycrc, for the host to compare with the image it sent. If a byte
; differs, as from a failing RAM chip, the host loads the program again. It
; runs before entry stores into the program's code, and takes about a third
; of a second.
selfcrc: ldy #0
        sty crclo
        sty crchi
        sty ptr
        lda #$08
        sta ptr+1
slfloop: lda (ptr),y
        eor crchi
        tax
        lda crclo
        eor CRCHI,x
        sta crchi
        lda CRCLO,x
        sta crclo
        iny
        bne slfend
        inc ptr+1
slfend: cpy #<progend
        bne slfloop
        ldx ptr+1
        cpx #>progend
        bne slfloop
        lda crclo
        sta readycrc
        lda crchi
        sta readycrc+1
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

; putraw: A into the chip's transmit memory, sent with the next wait for a
; byte, or once 128 bytes are waiting. A, X and Y are saved and restored.
putraw: stx savex
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

; putraw: a byte through the 6551. X and Y are saved and restored. The store
; is absolute, set for the port at startup: an indexed store reads its
; address first, and a read of the 6551's data register clears its receive
; flag.
;
; A byte that can't be sent in about a second and a half is dropped, as is
; every byte after it until the transmitter empties again. With the PC's
; port closed, its adapter's CTS line is false and the 6551 sends nothing, so
; without the limit the program would wait here forever, and never count
; the PC's silence or read the keyboard. tmo is free here: getbt sets it.
putraw: stx savex
        ldx slotx
        pha
        lda #0
        sta tmo
        sta tmo+1
pbwait: lda ACIASTAT,x
        and #TDRE
        bne pwsend
        lda txstuck
        bne pwdrop
        inc tmo
        bne pbwait
        inc tmo+1
        bne pbwait              ; 65,536 rounds of 22 cycles
        inc txstuck
pwdrop: pla
        ldx savex
        rts
pwsend: lda #0
        sta txstuck
        pla
pbsta:  sta ACIADATA
        ldx savex
        rts

txstuck: .byte 0                ; a byte was dropped, and the 6551 hasn't sent since
        endif

; putb: a byte through the chip. In Debug builds, the fault armed for the
; operation is applied to it first. A, X and Y are saved and restored.
        ifdef FAULTS
putb:   sta fltorig
        jsr fltbyte
        bcs pbdrop
        jsr putraw
        lda fltorig
        rts
pbdrop: lda fltorig
        clc
        rts

; fltbyte: the byte in A, flipped, or dropped with the carry set, or sent
; after a stall, when it is the one at the offset counted down from the
; operation's first byte; otherwise as it is, with the carry clear. X and Y
; are saved.
fltbyte: pha
        lda fltact
        beq fbsend
        cmp #FLTSTALL+1
        bcs fbsend
        lda fltcnt
        ora fltcnt+1
        ora fltcnt+2
        beq fbhit
        lda fltcnt
        bne fbdec0
        lda fltcnt+1
        bne fbdec1
        dec fltcnt+2
fbdec1: dec fltcnt+1
fbdec0: dec fltcnt
fbsend: pla
        clc
        rts
fbhit:  lda fltact
        sta fltkind
        lda #0                  ; one byte, once
        sta fltact
        lda fltkind
        cmp #FLTDROP
        beq fbdrop
        cmp #FLTSTALL
        beq fbstall
        pla
        eor #$01
        clc
        rts
fbdrop: pla
        sec
        rts
fbstall: lda fltms
        sta fltleft
fbunit: lda fltleft
        beq fbsend
        lda #250
        jsr delayms
        dec fltleft
        jmp fbunit

; fltmatch: Z set when the fault armed for the operation is the kind in A,
; for `track` and `sector`, which then disarms it once the operation ends.
; X and Y are saved.
fltmatch: cmp fltact
        bne fmdone
        lda track
        cmp flttrk
        bne fmdone
        lda sector
        cmp fltsec
        bne fmdone
        lda #1                  ; read: disarmed when the operation ends
        sta fltdone
        lda #0                  ; Z set again
fmdone: rts

; dofault: the fault for the next operation: its kind, then the offset of the
; byte as three bytes, least significant first, or the track and the
; sector, then a stall's length in quarter seconds. Then Ready again.
dofault: lda PKTBUF
        sta fltpend
        lda PKTBUF+1
        sta fltcnt
        sta flttrk
        lda PKTBUF+2
        sta fltcnt+1
        sta fltsec
        lda PKTBUF+3
        sta fltcnt+2
        lda PKTBUF+4
        sta fltms
        jmp main

fltpend: .byte 0                ; the kind armed for the next operation, or 0
fltact:  .byte 0                ; the kind armed for this one, or 0
fltkind: .byte 0
fltdone: .byte 0                ; a sector's fault was applied in this operation
fltcnt:  .byte 0, 0, 0          ; the bytes left before the one changed
flttrk:  .byte 0
fltsec:  .byte 0
fltms:   .byte 0                ; quarter seconds
fltleft: .byte 0
fltorig: .byte 0
        else
putb    = putraw
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
        ifdef FAULTS
        lda #FLTUNREAD          ; the sector passed by as if not found
        jsr fltmatch
        beq rtnext
        endif
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
rtnone: jsr firstpend
        lda #ERRNOTFND
        sta err
        sec
        rts
rtdone: clc
        rts

; firstpend: `sector` at the first physical sector readtrack didn't read, for
; the error.
firstpend: ldx #0
fpfind: lda PENDING,x
        bne fpfound
        inx
        cpx #15
        bne fpfind
fpfound: stx sector
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

ROW0 = $0400                    ; the title and the PC, and in 80 columns the connection
ROW1 = $0480                    ; the connection in 40 columns
ROW2 = $0500                    ; where the link goes
ROWLB = $0600                   ; row 4: the label of the user's operation
ROWD1 = $0780                   ; row 7: drive 1, its bar and its map
ROWB1 = $0428
ROWD2 = $0528                   ; row 10: drive 2, its bar and its map
ROWB2 = $05A8
ROWER = $06A8                   ; row 13: the last error, when there is one
ROWHIST = $0728                 ; row 14: the history's title, its lines below
LOGROWS = 8                     ; the history's lines, rows 15 to 22
BARCELLS = 35                   ; one a track
BARTEXT = 14                    ; the column of the track and sector over the bar
BARTEXTW = 31                   ; and in 80 columns, where a track has two cells
ROWKEYS = $07D0                 ; row 23: the keys
KEYSCOL = 4                     ; centered on the standard screen
FSKEYSCOL = 3                   ; and in a Full Send
PCGONE = 5                      ; seconds with no reply before the PC is gone
IDLEOFF = 18                    ; Idles, 110 ms apart, before the motor stops
FIELD = 12
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

; drawscreen: clear the text page's rows and draw the labels. The 8 bytes
; after each 120 are the screen holes, where the firmware stores a serial
; port's settings and the 80-column mode, in both memories. Clearing them
; breaks the port and DOS after CassoLink ends.
drawscreen: lda #$A0
        ldy #$77
dsclear: sta $0400,y
        sta $0480,y
        sta $0500,y
        sta $0580,y
        sta $0600,y
        sta $0680,y
        sta $0700,y
        sta $0780,y
        dey
        bpl dsclear
        bit wide
        bpl dsmain
        sta PAGE2ON
        ldy #$77
dsaux:  sta $0400,y
        sta $0480,y
        sta $0500,y
        sta $0580,y
        sta $0600,y
        sta $0680,y
        sta $0700,y
        sta $0780,y
        dey
        bpl dsaux
        sta PAGE2OFF
dsmain:
dsdone: jsr fschrome
        jsr showaddr
        jsr showconn
        jmp showstatus

titletxt: .text "CASSOLINK"
        .byte 0

; 40 or 80 columns. wide has its high bit set for 80, whose odd columns are in
; main memory and even ones in auxiliary memory, selected by PAGE2 under 80STORE.
PAGE2OFF = $C054
PAGE2ON  = $C055
wide:     .byte 0
rowwidth: .byte 40

; col40: 40 columns, as the shim and BASIC expect.
col40:  sta $C00C
        sta $C000
        sta PAGE2OFF
        lda #0
        sta wide
        lda #40
        sta rowwidth
        rts

; canwide: the carry clear when the machine has 80 columns: a //e, //c or IIgs
; whose auxiliary text page holds what is written to it, apart from the main
; page's.
canwide: lda $FBB3
        cmp #$06
        bne cwno
        lda $0400
        sta tmp
        sta $C001
        sta PAGE2ON
        lda #$A5
        sta $0400
        cmp $0400
        bne cwfail
        lda #$5A
        sta $0400
        cmp $0400
        bne cwfail
        sta PAGE2OFF
        sta $C000
        lda $0400
        cmp tmp
        bne cwno
        clc
        rts
cwfail: sta PAGE2OFF
        sta $C000
cwno:   sec
        rts

; docols: Set columns.
docols: jsr col40
        lda PKTBUF
        beq dcdraw
        jsr canwide
        bcs dcdraw
        sta $C00D
        sta $C001
        lda #$80
        sta wide
        lda #80
        sta rowwidth
dcdraw: lda #$FF
        sta bardrawn
        sta bardrawn+1
        jsr drawscreen
        jmp main

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

; putch: A into the field at Y, and Y on, in inverse left of barcells, so that
; text over a bar inverts where the bar has passed. Nothing is drawn at a Y of 40 or
; more, so a text from a row's start is never drawn on the row below it.
putch:  cpy rowwidth
        bcs pcnext
        cpy barcells
        bcs pcnorm
        and #$3F
        bcc pcput
pcnorm: ora #$80
pcput:  bit wide
        bmi pcwide
        sta (scr),y
pcnext: iny
        rts
pcwide: sty pcy
        pha
        tya
        lsr a
        tay
        pla
        bcs pcmain
        sta PAGE2ON
        sta (scr),y
        sta PAGE2OFF
        jmp pcwdone
pcmain: sta (scr),y
pcwdone: ldy pcy
        iny
        rts
pcy:    .byte 0


; putdec2: A in decimal, with a leading zero below 10, so a track or sector
; is always two digits and the percentage after it stays in place.
putdec2: cmp #10
        bcs putdec
        pha
        lda #'0'
        jsr putch
        pla

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

; showconn: the title and the PC on the top row, and the port and the speed the
; 6551 is set to: a //c's port, or a card's slot. They are at the top row's right
; edge in 80 columns, and on the row below in 40, where both don't fit.
showconn: ldx #<ROW0
        lda #>ROW0
        jsr fullrow
        ldx #<ROW1
        lda #>ROW1
        jsr fullrow
        lda rowwidth
        sta pcend
        bit wide
        bmi scwide
        jsr conndraw
        jmp showpc
scwide: pha                     ; with a row width of 0 nothing is drawn, and Y
        lda #0                  ; ends at the connection's length
        sta rowwidth
        ldy #0
        jsr conndraw
        sty pcend
        pla
        sta rowwidth
        sec
        sbc pcend
        sta pcend
        tay
        lda #<ROW0
        sta scr
        lda #>ROW0
        sta scr+1
        jsr conndraw
        jmp showpc

; conndraw: the connection into the row at scr from Y.
conndraw: lda #<conntext
        sta ptr
        lda #>conntext
        sta ptr+1
        jsr puttext
        ifdef W5100
        lda slotx               ; the card, its slot and the port, as
        lsr a                   ; UTHERNET II, SLOT 3, 6502
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
        ldx #0                  ; what the 6551 is: a card or a //c's port,
        lda $FBB3               ; by cable or through a Wi-Fi modem, as
        eor #$06                ; SSC, SLOT 2 or WI-FI MODEM, PORT 2. A //c
        ora $FBC0               ; has $06 at $FBB3 and $00 at $FBC0
        bne scmodem
        ldx #4
scmodem: lda SHIMDIAL
        beq scname
        inx
        inx
scname: lda connnames,x
        sta ptr
        lda connnames+1,x
        sta ptr+1
        jsr puttext
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
conntext: .text "UTHERNET II, SLOT "
        .byte 0
connport: .byte $2C
        .text " "
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
connssc: .text "SSC, SLOT "
        .byte 0
connsscm: .text "WI-FI, SLOT "
        .byte 0
connport: .text "SERIAL PORT "
        .byte 0
connportm: .text "WI-FI, PORT "
        .byte 0
conncomma: .text ", "
        .byte 0
; The names above by card or port, then cable or modem. After the text, as
; the assembler sizes a forward < or > in .byte wrongly.
connnames: .byte <connssc, >connssc, <connsscm, >connsscm
        .byte <connport, >connport, <connportm, >connportm
        endif


; showaddr: below the connection, where the link goes when it has an
; address: DIALED: and the target a Wi-Fi modem dialed, as the command has it
; after ATDT and its spaces, or ADDRESS: and the Uthernet II's address and
; port.
showaddr:
        ifndef W5100
        lda SHIMDIAL
        bne sahas
        rts
sahas:
        endif
        ldx #<ROW2
        lda #>ROW2
        jsr fullrow
        lda #<addrlabel
        sta ptr
        lda #>addrlabel
        sta ptr+1
        jsr puttext
        ldy #FIELD
        ifdef W5100
        lda #0
        sta sacount
saip:   ldx sacount             ; the address, a byte at a time, then the port
        lda SHIMADDR,x
        jsr putdec
        inc sacount
        lda sacount
        cmp #4
        beq saport
        lda #'.'
        jsr putch
        jmp saip
saport: lda #':'
        jsr putch
        lda SHIMPORT+1
        sta wnum
        lda SHIMPORT
        sta wnum+1
        jmp putword
        else
        ldx #4
saskip: lda SHIMDIAL,x
        cmp #' '
        bne satext
        inx
        bne saskip
satext: lda SHIMDIAL,x          ; in capitals, for a ][+
        beq sadone
        cmp #'a'
        bcc sacaps
        and #$DF
sacaps: jsr putch
        inx
        bne satext
sadone: rts
        endif

pcend:  .byte 0                  ; the top row's column where the PC's blanks end
        ifdef W5100
sacount: .byte 0                ; showaddr: the address's bytes drawn
addrlabel: .text "ADDRESS"
        .byte $3A, 0
        else
addrlabel: .text "DIALED"
        .byte $3A, 0
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

; showstatus: each drive's row and bar. A read or write has its bar, and any
; other time both are drawn with no text over them. A request with no share of
; the operation, as a Full Send's check, leaves its drive's bar as it is. During
; a Full Send the host sends the drives' rows.
showop:
showstatus: jsr percent
        sta pct
        lda fsmode
        bne stbar
        ldx #<ROWD1
        lda #>ROWD1
        jsr fullrow
        lda #1
        jsr stdrive1
        ldx #<ROWD2
        lda #>ROWD2
        jsr fullrow
        lda #2
        jsr stdrive1
        jmp stbar

showdrive:
showtrack:
showprog: jsr percent
        sta pct
stbar:  lda op
        beq stplain
        cmp #OPSPEED
        beq stplain
        cmp #OPDISK
        beq stplain
        lda phi
        beq stplain
        ldx drive
        dex
        cpx #2
        bcs stplain
        lda pct
        sta barpct,x
        lda #1
        jmp drawbar
stplain: lda #0
        ldx #0
        jsr drawbar
        lda #0
        ldx #1
        jmp drawbar

; stdrive1: the row at scr for the drive in A: its operation, or IDLE.
stdrive1: sta pcsum+1
        lda #'D'
        jsr putch
        lda pcsum+1
        ora #'0'
        jsr putch
        lda #$3A
        jsr putch
        lda #' '
        jsr putch
        lda #<oidle
        sta ptr
        lda #>oidle
        sta ptr+1
        ldx pcsum+1
        lda nodisk-1,x
        beq sdidle
        lda #<onodisk
        sta ptr
        lda #>onodisk
        sta ptr+1
sdidle: lda pcsum+1
        cmp drive
        bne sdput
        lda op
        beq sdput
        cmp #OPSPEED
        bcs sdput
        asl a
        tax
        lda optext,x
        sta ptr
        lda optext+1,x
        sta ptr+1
        lda vflag
        beq sdput
        lda #<overify
        sta ptr
        lda #>overify
        sta ptr+1
sdput:  jmp puttext

; drawbar: drive X + 1's bar, with the track and sector over it when A is
; nonzero, or FAILED after the drive's operation failed, and the percentage
; at the right edge. A drive with no share yet and no text has an empty row.
drawbar: sta barover
        stx barx
        lda barfail,x           ; FAILED is drawn as the track and sector are
        ora barover
        sta barover
        lda barrowlo,x
        sta scr
        lda barrowhi,x
        sta scr+1
        lda barpct,x
        ora barover
        bne dbdraw
        lda #$FF
        sta bardrawn,x
        jmp clrscr
dbdraw: ldy barpct,x
        lda barcelltab,y
        bit wide
        bpl dbtop
        asl a
dbtop:  sta bartop
        ldy #BARCELLS
        bit wide
        bpl dbwidth
        ldy #BARCELLS*2
dbwidth: sty barwidth
        lda barover
        beq dbcells
        lda bartop
        cmp bardrawn,x
        beq dbover
dbcells: lda bartop
        sta bardrawn,x
        sta barcells
        ldy #0
dbcell: lda #' '
        cpy bartop
        bcc dbput
        lda #'.'
dbput:  jsr putch
        cpy barwidth
        bne dbcell
        lda barover
        beq dbtext
dbover: lda bartop
        sta barcells
        ldy #BARTEXT
        bit wide
        bpl dbcol
        ldy #BARTEXTW
dbcol:  ldx barx
        lda barfail,x
        beq dbtrack
        lda #<ofailed
        sta ptr
        lda #>ofailed
        sta ptr+1
        jsr puttext
        jmp dbtext
dbtrack: lda #'T'
        jsr putch
        lda track
        jsr putdec2
        lda vflag
        bne dbtext
        lda #' '
        jsr putch
        lda #'S'
        jsr putch
        lda sector
        jsr putdec2
dbtext: lda #0
        sta barcells
        ldy barwidth
dbspace: lda #' '
        jsr putch
        cpy rowwidth
        bne dbspace
        ldx barx
        lda rowwidth
        sec
        sbc #2
        tay
        lda barpct,x
        cmp #10
        bcc dbpct2
        dey
        cmp #100
        bcc dbpct2
        dey
dbpct2: jsr putdec
        lda #'%'
        jmp putch

barrowlo: .byte <ROWB1, <ROWB2
barrowhi: .byte >ROWB1, >ROWB2
barpct:  .byte 0, 0             ; each drive's percentage
barover: .byte 0
barx:    .byte 0
bartop:  .byte 0
barcells: .byte 0               ; putch inverts left of this column
bardrawn: .byte $FF, $FF        ; each bar's cells filled when last drawn
barfail: .byte 0, 0              ; each drive's last operation failed, until its next
barwidth: .byte BARCELLS
; The cells filled at each percentage.
barcelltab:
        .byte 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6
        .byte 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13
        .byte 14, 14, 14, 15, 15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 18, 19, 19, 19, 20, 20
        .byte 21, 21, 21, 22, 22, 22, 23, 23, 23, 24, 24, 24, 25, 25, 25, 26, 26, 26, 27, 27
        .byte 28, 28, 28, 29, 29, 29, 30, 30, 30, 31, 31, 31, 32, 32, 32, 33, 33, 33, 34, 34
        .byte 35

stdrive:  .text " D"
          .byte 0
sttrack:  .byte $2C
          .text " T"
          .byte 0
stsector: .byte $2C
          .text " S"
          .byte 0

optext: .word oidle, oread, owrite, oformat

oidle:   .text "IDLE"
         .byte 0
onodisk: .text "EMPTY OR NO RECOGNIZED SECTORS"
         .byte 0
oread:   .text "READING"
         .byte 0
owrite:  .text "WRITING"
         .byte 0
oformat: .text "FORMATTING"
         .byte 0
overify: .text "VERIFYING"
         .byte 0
ofailed: .text "FAILED"
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
        ; falls through

; clrscr: the row at scr cleared. Y is 0, and X is kept.
clrscr: ldy #39
        lda #$A0
frclear: sta (scr),y
        dey
        bpl frclear
        bit wide
        bpl frdone
        sta PAGE2ON
        ldy #39
fraux:  sta (scr),y
        dey
        bpl fraux
        sta PAGE2OFF
frdone: ldy #0
        rts

; showpc: the title and the PC: waiting for its first reply, connected, or how
; long since its last reply.
showpc: lda #<ROW0
        sta scr
        lda #>ROW0
        sta scr+1
        ldy #0
        lda #<titletxt
        sta ptr
        lda #>titletxt
        sta ptr+1
        jsr puttext
        lda #$3A
        jsr putch
        lda #' '
        jsr putch
        lda silent
        bne spquiet
        lda #<pcconn
        sta ptr
        lda #>pcconn
        sta ptr+1
        lda pcflag
        bne sptext
        lda #<pcwait
        sta ptr
        lda #>pcwait
        sta ptr+1
sptext: jsr puttext
; spblank: the row at scr blank from Y to pcend.
spblank: cpy pcend
        bcs spdone
        lda #' '
        jsr putch
        jmp spblank
spdone: rts

spquiet: lda #<pcnone
        sta ptr
        lda #>pcnone
        sta ptr+1
        jsr puttext
        lda silent
        jsr putdec
        lda #'S'
        jsr putch
        jmp spblank

; pcok: a reply arrived. The line is drawn again only when it changes, as a
; command's reply follows. A first reply, or one after a silence, is a new
; connection, which starts with no error. A first reply also starts with empty
; bars, as the last PC's percentages no longer apply.
pcok:   lda pcflag
        bne pcback
        lda #0
        sta barpct
        sta barpct+1
        sta barfail
        sta barfail+1
        jsr stplain
        jmp pcnew
pcback: lda silent
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
        lda fsmode              ; a session with no PC is over
        beq npdraw
        jsr fsleave
npdraw: jmp showpc

pcwait: .text "WAITING"
        .byte 0
pcconn: .text "CONNECTED"
        .byte 0
pcnone: .text "NO REPLY "
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
        jsr putdec2
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

errtext: .word eother, eprotect, enotfnd, edata, everify, elink, edrive, echanged
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
echanged: .text "DISK CHANGED"
          .byte 0

; dolabel: the host's text for the user's operation across the label row,
; then Ready again.
dolabel: jsr clrlabel
lbtext: cpy lenlo
        beq lbdone
        cpy rowwidth
        beq lbdone
        lda PKTBUF,y
        jsr putch
        jmp lbtext
lbdone: jmp main

; clrlabel: the label row cleared. Y is 0.
clrlabel: ldx #<ROWLB
        lda #>ROWLB
        jmp fullrow

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
        bit wide
        bpl lgnext
        sta PAGE2ON
        ldy #39
lgaux:  lda (ptr),y
        sta (scr),y
        dey
        bpl lgaux
        sta PAGE2OFF
lgnext:
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
        cpy rowwidth
        beq lgdone
        lda PKTBUF,x
        jsr putch
        inx
        bne lgtext
lgdone: lda PKTBUF+2            ; ERR: the last operation's drive failed
        cmp #'E'
        bne lgmain
        lda PKTBUF+3
        cmp #'R'
        bne lgmain
        lda PKTBUF+4
        cmp #'R'
        bne lgmain
        ldx drive
        dex
        cpx #2
        bcs lgmain
        lda #1
        sta barfail,x
        lda #0
        jsr drawbar
lgmain: jmp main

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

; The history's lines, rows 15 to 22 of the text page.
logrowlo: .byte $A8, $50, $D0, $50, $D0, $50, $D0, $50
logrowhi: .byte $07, $04, $04, $05, $05, $06, $06, $07

; ---------------------------------------------------------------------------
; A Full Send
; ---------------------------------------------------------------------------

FSPROMPT = 0                    ; the first of Wait for key's lines, by its index in logrowlo
FSROWS   = 7                    ; Session screen's rows
FSTONE   = 1                    ; its tone, in PKTBUF
FSTEXT   = 2                    ; and the start of its rows' text

; sendkey: Key with the code in A. It is sent at the next wait for a byte, or
; for a W5100 by flush.
sendkey: sta tmp
        ldx #0
skloop: lda keyhdr,x
        jsr putb
        inx
        cpx #keyend-keyhdr
        bne skloop
        lda tmp
        jsr putb
        lda tmp
        jmp putb

keyhdr: .byte $C1, $01, $00, CMDKEY, $C1 ^ $01 ^ $00 ^ CMDKEY
keyend:

; dowaitkey: a Full Send's pause. The motor is off and a key pressed during the
; transfer is dropped, so Space means the disks in the drives now. The request's
; first byte is nonzero while a line is typed, and every key is sent; the lines
; follow it. The prompt is drawn on the history's rows, which a Full Send
; keeps blank, and they are cleared again once a key is sent.
dowaitkey: jsr motoroff
        lda #OPDISK
        sta op
        jsr showop
        sta $C010
        lda PKTBUF
        sta wkany
        lda #1
        sta left
        lda #FSPROMPT
        sta sbyte
wkline: ldx sbyte
        jsr fsrow
wkchar: ldx left
        cpx lenlo
        bcs wksec
        inc left
        lda PKTBUF,x
        beq wknext
        jsr putch
        jmp wkchar
wknext: inc sbyte
        jmp wkline
wksec:  lda #100
        sta tries
wkpoll: lda $C000
        bpl wkhost
        sta $C010
        ldx wkany
        bne wkkey
        cmp #$A0
        beq wkkey
        cmp #$B1
        beq wkkey
        cmp #$B2
        beq wkkey
        cmp #$D6
        beq wkkey
        cmp #$F6
        bne wkprefq
        lda #$D6
        bne wkkey
wkprefq: cmp #$D0
        beq wkkey
        cmp #$F0
        bne wkquitq
        lda #$D0
        bne wkkey
wkquitq:
        cmp #$F1
        beq wkquit
        cmp #$D1
        bne wkhost
wkquit: lda #$D1
        bne wkkey
wkhost: jsr pollcancel
        lda #10
        jsr delayms
        dec tries
        bne wkpoll
        lda #0
        jsr sendkey
        ifdef W5100
        jsr flush
        endif
        jmp wksec
wkkey:  jsr sendkey
        jsr fsclear
        jmp main

wkany:  .byte 0                 ; nonzero while a line is typed

; doscreen: the Session screen's rows, its tone, and Ready again. A session's
; first screen saves the history's rows in KHIST and blanks them, as a line
; there has no heading during a Full Send; fsleave draws them again.
doscreen: lda PKTBUF
        beq dsstore
        lda fsmode
        bne dsstore
        lda #0
        sta barpct
        sta barpct+1
        jsr khcopy
        jsr fsclear
        lda #1
        sta fssaved
dsstore: lda PKTBUF
        sta fsmode
        bne dsdraw
        jsr fsleave
        jmp main
dsdraw: jsr fschrome
        lda #FSTEXT
        sta left
        lda #0
        sta sbyte
dsrow:  ldx sbyte
        lda fsrowhi,x
        pha
        lda fsrowlo,x
        tax
        pla
        jsr fullrow
        sty dscol
dschar: ldx left
        cpx lenlo
        bcs dssound
        inc left
        lda PKTBUF,x
        beq dsrown
        bpl dsplain
        and #$7F                ; a high bit is inverse
        ldy rowwidth
        sty barcells
dsplain: ldy dscol
        jsr putch
        sty dscol
        ldy #0
        sty barcells
        jmp dschar
dsrown: inc sbyte
        lda sbyte
        cmp #FSROWS
        bne dsrow
dssound: jsr fssound
        jmp main

; The rows of the Session screen: the title, the counts, a line, and each
; drive's row and map.
dscol:  .byte 0
fsrowlo: .byte <ROWLB, $80, $00, <ROWD1, $A8, <ROWD2, $28
fsrowhi: .byte >ROWLB, $06, $07, >ROWD1, $04, >ROWD2, $06

; fsclear: the rows a Full Send draws on cleared, and the label and history's.
fsclear: ldx #LOGROWS-1
fcclear: jsr fsrow
        dex
        bpl fcclear
        rts

; fsleave: back to the standard screen: the session's rows cleared, the
; history saved when the session began drawn again, and the heading, keys and
; drives drawn.
fsleave: lda fssaved
        beq flnosave
        lda #1
        jsr khcopy
flnosave: lda #0
        sta fssaved
        sta fsmode
        sta sbyte
flrow:  ldx sbyte
        lda fsrowhi,x
        pha
        lda fsrowlo,x
        tax
        pla
        jsr fullrow
        inc sbyte
        lda sbyte
        cmp #FSROWS
        bne flrow
        jsr fschrome
        jmp showstatus

; fschrome: the history's heading on the standard screen and the keys on the
; last row, for either screen.
fschrome: ldx #<ROWHIST
        lda #>ROWHIST
        jsr fullrow
        lda fsmode
        bne fcnohead
        lda #<histtxt
        sta ptr
        lda #>histtxt
        sta ptr+1
        jsr puttext
fcnohead: ldx #<ROWKEYS
        lda #>ROWKEYS
        jsr fullrow
        lda #<keystxt
        sta ptr
        lda #>keystxt
        sta ptr+1
        ldy #KEYSCOL
        lda fsmode
        beq fckeys80
        lda #<fskeystxt
        sta ptr
        lda #>fskeystxt
        sta ptr+1
        ldy #FSKEYSCOL
fckeys80: bit wide
        bpl fckeys
        tya
        clc
        adc #20
        tay
fckeys: jmp puttext

histtxt: .text "HISTORY"
        .byte $3A, 0
keystxt: .text "F"
        .byte $3A
        .text " FULL SEND   PRESS ESC TO EXIT"
        .byte 0
fskeystxt: .text "V"
        .byte $3A
        .text " VERIFY  P"
        .byte $3A
        .text " PREFIX  Q"
        .byte $3A
        .text " CASSOLINK"
        .byte 0

; fsrow: scr at the history row of index X, and the row cleared. Y is 0.
fsrow:  lda logrowlo,x
        sta scr
        lda logrowhi,x
        sta scr+1
        jmp clrscr

; fssound: the tone, by $C030, with the request's byte: one note for a finished
; disk, two for a duplicate and a low pair for bad sectors. Nothing else is
; happening: the drive is not being read and the host is waiting for Ready.
fssound: lda PKTBUF+FSTONE
        beq fsndone
        cmp #4
        beq fsnprot
        cmp #2
        beq fsndup
        bcs fsnbad
        lda #40
        ldx #255
        jmp beep
fsndup: lda #40
        ldx #120
        jsr beep
        lda #27
        ldx #180
        jmp beep
fsnbad: lda #140
        ldx #80
        jsr beep
        lda #200
        ldx #60
        jmp beep
fsndone: rts
fsnprot: lda #60                ; three rising notes for a likely copy-protected disk
        ldx #100
        jsr beep
        lda #40
        ldx #120
        jsr beep
        lda #27
        ldx #160
        jmp beep

; beep: X half-cycles of about 9 times A CPU cycles each.
beep:   stx tmp
        sta tmp2
bpcycle: lda $C030
        ldy tmp2
bpwait: nop
        nop
        dey
        bne bpwait
        dec tmp
        bne bpcycle
        rts


; ---------------------------------------------------------------------------
; Krunch
; ---------------------------------------------------------------------------

; Krunch: the Apple Pascal volume's files moved toward its start in directory
; order, which combines its free space, as the Filer's K does. The directory
; is read first, and its CRC compared with the host's, so the disk is the one
; the host checked.
;
; A file is moved a part at a time, each part the blocks up to the end of a
; track at both places: read into RAWBUF, written from streams in WSLOTS and
; read back. Only then is its entry in the directory changed, read and
; written back in the one or two sectors its first and next blocks are in.
; Until then the entry indicates the old blocks, which the copy overwrites
; only where the old and new places overlap, and the new place is free space
; or blocks of the file being moved. So when a krunch stops at any point,
; every other file is whole, and the file being moved is whole unless its
; places overlap.
;
; After the request the program reads nothing from the host but a Cancel,
; between files, so a link that drops doesn't stop it.
;
; While it runs, the history's rows have a map of the volume's blocks, as at
; kmapshow, with each part's blocks drawn as they are read and
; written, and a moved file's old blocks drawn as free once its entry is
; written.
KMAPROWS = 7                    ; the block map's rows, the history's first seven
KMAPCOLS = 40                   ; and its cells in a row, two columns each in 80 columns
KMAPEND  = KMAPROWS * KMAPCOLS  ; its cells, a 140 KB disk's blocks
KMAPPAST = KMAPEND + 1          ; the first block a part can't end at in the map
KSDONE   = 0                    ; a block in its final place: its character in inverse
KSMOVE   = 1                    ; a file's block to move: its character, or # for a space
KCFILL   = $A3                  ; that #
KCFREE   = $AE                  ; a free block: .
KCREAD   = $52                  ; a block being read: a flashing R
KCWRITE  = $57                  ; a block being written: a flashing W
KCOFF    = $A0                  ; a block past the volume's end: a space
KCINVSP  = $20                  ; an inverse space
KLEGCOL80 = 15                 ; the legend's column in 80 columns, centered
KNAMEAT  = 6                    ; a directory entry's name length, then its name
KNAMEMAX = 15                   ; the characters a name has at most
RDALTCHAR = $C01E               ; a //e's or //c's alternate characters, in bit 7
ALTCHAROFF = $C00E              ; the primary characters, which flash
ALTCHARON = $C00F

dokrunch: lda PKTBUF+1
        sta kcrc
        lda PKTBUF+2
        sta kcrc+1
        lda #0
        sta PKTBUF+1            ; track 0, no sectors, and 0 to 100%
        sta PKTBUF+2
        sta PKTBUF+3
        sta kfiles
        sta kmoved
        sta kmoved+1
        lda #100
        sta PKTBUF+4
        lda #OPREAD
        jsr begin
        bcs krend
        jsr protected
        bcs krend
        jsr kchclear
        jsr krdir
        bcs krend
        jsr krplan
        bcs krend
        jsr krprog              ; the request was taken
        jsr kmapshow
        lda #1
        sta kslot
krfile: lda kcnt
        cmp kslot
        bcc krend
        jsr krentry
        bcs krend
        inc kslot
        jmp krfile
krend:  jsr kmapoff
        lda kfiles              ; the status has the files moved
        sta index
        jmp opdone

; krdir: the directory's eight sectors read from track 0 and decoded in
; order, each entry's first and next blocks into the tables, the file count
; into kcnt and the volume's blocks into keov. The carry is set, with err, when a sector doesn't read,
; the CRC differs from the host's, or the count is past 77.
krdir:  ldx #7
krdlist: lda blkphys+4,x        ; blocks 2 to 5
        sta PKTBUF+3,x
        dex
        bpl krdlist
        lda #8
        sta count
        jsr readtrack
        bcs krdfail
        lda #0
        sta crclo
        sta crchi
        sta kfield
        sta kentry
        sta kj
krdsec: ldx kj
        lda blkphys+4,x
        sta sector
        jsr rawslot
        jsr decodesec
        ldy #0
krdbyte: lda SECBUF,y
        jsr crcupd
        jsr krdfield
        iny
        bne krdbyte
        inc kj
        lda kj
        cmp #8
        bne krdsec
        lda crclo
        cmp kcrc
        bne krdchg
        lda crchi
        cmp kcrc+1
        bne krdchg
        lda kcnthi
        bne krdbad
        lda kcnt
        cmp #78                 ; 77 entries at most
        bcs krdbad
        clc
        rts
krdchg: lda #ERRCHANGED
        sta err
        sec
        rts
krdbad: lda #ERRDATA
        sta err
krdfail: sec
        rts

; krdfield: the directory byte at SECBUF,Y, byte kfield of entry kentry: the
; volume's blocks and file count from the volume's entry, and each file's
; first and next blocks and, by krname, its name. Y isn't modified.
krdfield: lda SECBUF,y
        ldx kentry
        bne kfent
        ldx kfield
        cpx #14
        bne kfeovhi
        sta keov
kfeovhi: cpx #15
        bne kfcntlo
        sta keov+1
kfcntlo: cpx #16
        bne kfcount
        sta kcnt
kfcount: cpx #17
        bne kfstep
        sta kcnthi
        jmp kfstep
kfent:  ldx kfield
        beq kff0
        dex
        beq kff1
        dex
        beq kff2
        dex
        beq kff3
        jsr krname
        jmp kfstep
kff0:   ldx kentry
        sta KFLO,x
        jmp kfstep
kff1:   ldx kentry
        sta KFHI,x
        jmp kfstep
kff2:   ldx kentry
        sta KNLO,x
        jmp kfstep
kff3:   ldx kentry
        sta KNHI,x
kfstep: inc kfield
        lda kfield
        cmp #26                 ; an entry's bytes
        bne kfdone
        lda #0
        sta kfield
        inc kentry
kfdone: rts

; krname: the directory byte in A, byte kfield of entry kentry, of a file the
; volume counts: its name's length, or a character of its name, put in KCHAR
; at the block of its place in the name when the file has that block and the
; map has its cell. Y isn't modified.
krname: sty kny
        ldx kcnthi
        bne krndone
        ldx kentry
        cpx kcnt
        beq krnfile
        bcs krndone
krnfile: ldy kfield
        cpy #KNAMEAT
        bne krnchar
        sta knlen
        jmp krndone
krnchar: pha
        tya
        sec
        sbc #KNAMEAT+1          ; the character's place in the name
        bcc krnpop
        cmp #KNAMEMAX
        bcs krnpop
        cmp knlen
        bcs krnpop
        clc
        adc KFLO,x
        sta kcb
        lda KFHI,x
        adc #0
        sta kcb+1
        lda kcb                 ; a block of the file's
        cmp KNLO,x
        lda kcb+1
        sbc KNHI,x
        bcs krnpop
        lda kcb                 ; and of the map's
        cmp #<KMAPEND
        lda kcb+1
        sbc #>KMAPEND
        bcs krnpop
        pla
        and #$7F                ; in capitals, and ? for a control character
        cmp #$60
        bcc krnupper
        and #$5F
krnupper: cmp #' '
        bcs krnput
        lda #'?'
krnput: jsr kchput
        jmp krndone
krnpop: pla
krndone: ldy kny
        rts

; krplan: ktotal, the blocks the krunch moves, with each entry checked: its
; next block past its first, and its first not before the end of the file
; ahead of it. kat is then the first data block. The carry is set, with
; ERRDATA, when an entry fails.
krplan: lda #0
        sta ktotal
        sta ktotal+1
        jsr krfirst
        lda #1
        sta kslot
kpnext: lda kcnt
        cmp kslot
        bcc kpdone
        jsr krlength
        bcc kpbad
        ora kblocks
        beq kpbad
        ldx kslot
        lda KFLO,x
        cmp kat
        lda KFHI,x
        sbc kat+1
        bcc kpbad
        jsr krstays
        beq kpstay
        clc
        lda ktotal
        adc kblocks
        sta ktotal
        lda ktotal+1
        adc kblocks+1
        sta ktotal+1
kpstay: jsr krpast
        inc kslot
        jmp kpnext
kpdone: jsr krfirst
        clc
        rts
kpbad:  lda #ERRDATA
        sta err
        sec
        rts

; krfirst: kat at the first data block, 6.
krfirst: lda #6
        sta kat
        lda #0
        sta kat+1
        rts

; krlength: kblocks, entry kslot's length, with A its high byte. The carry is
; clear when its next block is before its first.
krlength: ldx kslot
        sec
        lda KNLO,x
        sbc KFLO,x
        sta kblocks
        lda KNHI,x
        sbc KFHI,x
        sta kblocks+1
        rts

; krstays: Z set when entry kslot starts at kat.
krstays: ldx kslot
        lda KFLO,x
        cmp kat
        bne ksdone
        lda KFHI,x
        cmp kat+1
ksdone: rts

; krpast: kat past entry kslot's new place.
krpast: clc
        lda kat
        adc kblocks
        sta kat
        lda kat+1
        adc kblocks+1
        sta kat+1
        rts

; krentry: entry kslot moved to kat, when it starts past it, then kat past
; it. A Cancel is taken after a moved file's entry is written. The carry is
; set, with err, when a read or write fails.
krentry: jsr krlength
        jsr krstays
        beq kestay
        ldx kslot
        lda KFLO,x
        sta ksrc
        lda KFHI,x
        sta ksrc+1
        lda kat
        sta kdst
        lda kat+1
        sta kdst+1
        lda kblocks
        sta kleft
        lda kblocks+1
        sta kleft+1
kepart: lda kleft
        ora kleft+1
        beq keentry
        jsr krpart
        bcs kefail
        jsr krprog
        jmp kepart
keentry: jsr krdirent
        bcs kefail
        inc kfiles
        jsr kmapfree
        jsr pollcancel
kestay: jsr krpast
        clc
kefail: rts

; krpart: the next part of the move, up to the end of a track at ksrc and at
; kdst, or kleft blocks: read from ksrc's track, then written at kdst's and
; read back. ksrc, kdst and kmoved step on and kleft down by its blocks. The
; carry is set, with err, when a read or write fails.
krpart: lda ksrc
        and #7
        sta ksoff
        lda kdst
        and #7
        sta kdoff
        lda #8
        sec
        sbc ksoff
        sta klen
        lda #8
        sec
        sbc kdoff
        cmp klen
        bcs kpkeep
        sta klen
kpkeep: lda kleft+1
        bne kpread
        lda kleft
        cmp klen
        bcs kpread
        sta klen
kpread: lda ksoff
        jsr kplist
        lda ksrc
        ldx ksrc+1
        jsr ktrack
        sta track
        lda #OPREAD
        sta op
        jsr seek
        jsr kshow
        lda #KCREAD
        jsr kmarksrc
        jsr readtrack
        bcc kpframe
        rts                     ; the carry set, with err
kpframe: lda #KSMOVE
        jsr kmarksrc
        lda #0
        sta kidx
kpslot: lda ksoff              ; each destination sector's stream, from
        asl a                   ; the source sector in its place
        clc
        adc kidx
        tax
        lda blkphys,x
        asl a
        clc
        adc #>RAWBUF
        sta kpage
        lda kdoff
        asl a
        clc
        adc kidx
        tax
        lda blkphys,x
        sta sector
        lda kpage
        jsr mkslot
        inc kidx
        lda kidx
        lsr a
        cmp klen
        bne kpslot
        lda kdoff
        jsr kplist
        lda kdst
        ldx kdst+1
        jsr ktrack
        sta track
        lda #KCWRITE
        jsr kmarkdst
        jsr kwrite
        bcs kpfail
        jsr kmovechars
        lda #KSDONE
        jsr kmarkdst
        clc
        lda kmoved
        adc klen
        sta kmoved
        bcc kpstep1
        inc kmoved+1
kpstep1: clc
        lda ksrc
        adc klen
        sta ksrc
        bcc kpstep2
        inc ksrc+1
kpstep2: clc
        lda kdst
        adc klen
        sta kdst
        bcc kpstep3
        inc kdst+1
kpstep3: sec
        lda kleft
        sbc klen
        sta kleft
        bcs kpdone2
        dec kleft+1
kpdone2: clc
kpfail: rts

; kplist: PKTBUF+3 on, the sectors of klen blocks from block A of a track,
; two a block, and count.
kplist: asl a
        tax
        ldy #0
kplsec: lda blkphys,x
        sta PKTBUF+3,y
        inx
        iny
        tya
        lsr a
        cmp klen
        bne kplsec
        sty count
        rts

; ktrack: the track of the block in A, low byte, and X, high byte, in A.
ktrack: stx tmp
        lsr tmp
        ror a
        lsr a
        lsr a
        rts

; kwrite: the sectors listed from PKTBUF+3, count of them, written on `track`
; from their streams in WSLOTS and read back, up to three times. The carry is
; set, with err and sector, when the last attempt fails.
kwrite: lda #OPWRITE
        sta op
        jsr seek
        lda #3
        sta ktries
kwtry:  lda #ERRNONE
        sta err
        jsr kshow
        jsr writepass
        bcs kwnext
        lda #1                  ; VERIFYING, as startverify draws it
        sta vflag
        jsr kshow
        jsr verifysecs
        jsr endverify
        bcc kwdone
kwnext: dec ktries
        bne kwtry
        sec
kwdone: rts

; krdirent: entry kslot's first block set to kat and its next block to kat
; plus kblocks, in the one or two directory sectors those four bytes are in:
; read, changed, written back and read back. The carry is set, with err, when
; that fails.
krdirent: lda #0
        sta kpos
        sta kpos+1
        ldx kslot
kdpos:  clc                     ; kpos: the entry's offset, 26 bytes a slot
        lda kpos
        adc #26
        sta kpos
        bcc kdposnext
        inc kpos+1
kdposnext: dex
        bne kdpos
        lda kat
        sta kpatch
        lda kat+1
        sta kpatch+1
        clc
        lda kat
        adc kblocks
        sta kpatch+2
        lda kat+1
        adc kblocks+1
        sta kpatch+3
        lda kpos+1              ; the directory sectors of kpos and kpos + 3
        sta kdsec
        clc
        lda kpos
        adc #3
        lda kpos+1
        adc #0
        sta kdsec+1
        ldx kdsec
        lda blkphys+4,x
        sta PKTBUF+3
        lda #1
        sta count
        ldx kdsec+1
        cpx kdsec
        beq kdread
        lda blkphys+4,x
        sta PKTBUF+4
        inc count
kdread: lda #0
        sta track
        lda #OPREAD
        sta op
        jsr seek
        jsr kshow
        jsr readtrack
        bcs kdfail
        ldx kdsec
        jsr kdpatch
        ldx kdsec+1
        cpx kdsec
        beq kdwrite
        jsr kdpatch
kdwrite: jmp kwrite             ; the sectors as listed for the read
kdfail: rts

; kdpatch: directory sector X, as read into RAWBUF, decoded, with the bytes of
; kpatch put in at kpos where they fall in it, and encoded into its stream.
kdpatch: stx kj
        lda blkphys+4,x
        sta sector
        jsr rawslot
        jsr decodesec
        ldy #0
kdpbyte: tya
        clc
        adc kpos
        tax
        lda kpos+1
        adc #0
        cmp kj
        bne kdpnext
        lda kpatch,y
        sta SECBUF,x
kdpnext: iny
        cpy #4
        bne kdpbyte
        jsr encode
        lda #>WRAUX
        jmp mkslot

; decodesec: the raw sector at (dst) and (dstp) decoded into SECBUF, as
; sendsector decodes it.
decodesec: ldy #85
dcaux:  lda (dst),y
        tax
        lda AUXT0,x
        sta AUXA0,y
        lda AUXT1,x
        sta AUXA1,y
        lda AUXT2,x
        sta AUXA2,y
        dey
        bpl dcaux
        ldy #0
dcbyte: lda (dstp),y
        asl a
        asl a
        ora AUXA0,y
        sta SECBUF,y
        iny
        bne dcbyte
        rts

; mkslot: `sector`'s stream in WSLOTS, framed as for Write sectors, with the
; data field from the 86 groups on page A and the 256 values on the next.
mkslot: pha
        jsr frameslot
        dec ptr+1
        lda #<SYNCEND
        sta ptr
        pla
        jmp putdata

; kshow: the drive's row, and its bar at the krunch's percentage, kmoved of
; ktotal, each halved to fit a byte. count is saved and restored.
kshow:  lda count
        pha
        clc
        lda kmoved
        adc #1
        sta tmp2
        lda kmoved+1
        adc #0
        lsr a
        lda tmp2
        ror a
        sta index
        clc
        lda ktotal
        adc #1
        sta tmp2
        lda ktotal+1
        adc #0
        lsr a
        lda tmp2
        ror a
        sta count
        jsr showstatus
        pla
        sta count
        rts

; krprog: the percentage drawn, and Krunch progress sent with it and the
; files moved.
krprog: jsr kshow
        ldx #0
kpghdr: lda kproghdr,x
        jsr putb
        inx
        cpx #kprogend-kproghdr
        bne kpghdr
        lda #0
        sta peor
        lda pct
        jsr putpe
        lda kfiles
        jsr putpe
        lda peor
        jsr putb
        ifdef W5100
        jsr flush               ; at once, as nothing is read until the krunch ends
        endif
        rts

kproghdr: .byte $C1, $02, $00, CMDKPROG, $C1 ^ $02 ^ $00 ^ CMDKPROG
kprogend:

; kmapshow: the block map drawn over the history, which is saved in KHIST
; first, with BLOCKS for its heading and a legend on its last row. Each block
; is a cell, 40 to a row from block 0 at the top left, and two characters
; wide in 80 columns. A file's name is across its blocks, a character a cell
; from its first, as far as its blocks go. The boot blocks, the directory and
; each file that stays where it is are drawn as done, in inverse; each file to
; move as to move, its name in normal characters and # past it; and the rest
; as free, or blank past the volume's end. Blocks being read and written are
; a flashing R and W, so on a //e or //c the primary characters are selected
; while the map is drawn. The map has 280 cells, a 140 KB disk's blocks; a
; block past them isn't drawn. During a Full Send, whose screen is on the
; history's rows, no map is drawn.
kmapshow: lda fsmode
        beq kmsgo
        rts
kmsgo:  lda #0
        jsr khcopy
        lda #1
        sta kmapon
        lda $FBB3
        cmp #$06
        bne kmsprim
        lda RDALTCHAR
        sta kalt
        sta ALTCHAROFF
kmsprim:
        ldx #<ROWHIST
        lda #>ROWHIST
        jsr fullrow
        lda #<kmaptxt
        sta ptr
        lda #>kmaptxt
        sta ptr+1
        jsr puttext
        jsr fsclear
        lda #0                  ; free up to the volume's end, and blank past it
        sta kcfrom
        sta kcfrom+1
        lda keov
        sta kcto
        lda keov+1
        sta kcto+1
        lda #KCFREE
        jsr kcells
        lda #<KMAPEND
        sta kcto
        lda #>KMAPEND
        sta kcto+1
        lda #KCOFF
        jsr kcells
        lda #0                  ; the boot blocks and the directory
        sta kcfrom
        sta kcfrom+1
        sta kcto+1
        lda #6
        sta kcto
        lda #KSDONE
        jsr kcells
        lda #1
        sta kslot
kmsfile: lda kcnt
        cmp kslot
        bcc kmslegend
        jsr krlength
        ldx kslot
        lda KFLO,x
        sta kcfrom
        lda KFHI,x
        sta kcfrom+1
        lda KNLO,x
        sta kcto
        lda KNHI,x
        sta kcto+1
        jsr krstays
        beq kmsstay
        lda #KSMOVE
        bne kmsdraw
kmsstay: lda #KSDONE
kmsdraw: jsr kcells
        jsr krpast
        inc kslot
        jmp kmsfile
kmslegend: jsr krfirst
        ldx #LOGROWS-1
        jsr fsrow
        lda #<klegend
        sta ptr
        lda #>klegend
        sta ptr+1
        bit wide
        bpl kmsleg
        lda #<klegend80
        sta ptr
        lda #>klegend80
        sta ptr+1
        ldy #KLEGCOL80
kmsleg: jmp putinv

; kmapoff: the history drawn again over the block map, with its heading and
; the keys, and the alternate characters selected again if they were, when the
; map is drawn.
kmapoff: lda kmapon
        beq kmodone
        lda #0
        sta kmapon
        bit kalt
        bpl kmoprim
        sta ALTCHARON
kmoprim: jsr fschrome
        lda #1
        jmp khcopy
kmodone: rts

; khcopy: the history's rows copied to KHIST when A is 0, or from it back onto
; the screen when A is 1: main memory's 40 bytes of each row and then, in 80
; columns, auxiliary memory's.
khcopy: sta khdir
        lda #<KHIST
        sta khbuf
        lda #>KHIST
        sta khbuf+1
        lda #0
        sta khaux
khpass: ldx #0
khrow:  lda khdir
        bne khback
        lda logrowlo,x          ; from the screen
        sta ptr
        lda logrowhi,x
        sta ptr+1
        lda khbuf
        sta scr
        lda khbuf+1
        sta scr+1
        jmp khbytes
khback: lda khbuf               ; onto the screen
        sta ptr
        lda khbuf+1
        sta ptr+1
        lda logrowlo,x
        sta scr
        lda logrowhi,x
        sta scr+1
khbytes: ldy #39
        bit khaux
        bpl khbyte
        sta PAGE2ON
khbyte: lda (ptr),y
        sta (scr),y
        dey
        bpl khbyte
        sta PAGE2OFF
        clc
        lda khbuf
        adc #40
        sta khbuf
        bcc khnext
        inc khbuf+1
khnext: inx
        cpx #LOGROWS
        bne khrow
        bit khaux
        bmi khdone
        bit wide
        bpl khdone
        lda #$80
        sta khaux
        bne khpass
khdone: rts

; kmarksrc: the cells of the part's source blocks, klen of them from ksrc,
; drawn in the style or as the screen code in A, as kcells draws them;
; kmarkdst: those of its destination blocks, from kdst.
kmarkdst: ldx #kdst-ksrc
        bne kmark
kmarksrc: ldx #0
kmark:  pha
        lda ksrc,x
        sta kcfrom
        clc
        adc klen
        sta kcto
        lda ksrc+1,x
        sta kcfrom+1
        adc #0
        sta kcto+1
        pla
        jmp kcells

; kmapfree: the cells of entry kslot's old blocks drawn as free once its entry
; indicates its new place, apart from those the new place covers.
kmapfree: clc
        lda kat
        adc kblocks
        sta kcfrom
        lda kat+1
        adc kblocks+1
        sta kcfrom+1
        ldx kslot
        lda KFLO,x
        cmp kcfrom
        lda KFHI,x
        sbc kcfrom+1
        bcc kmfnew
        lda KFLO,x
        sta kcfrom
        lda KFHI,x
        sta kcfrom+1
kmfnew: lda KNLO,x
        sta kcto
        lda KNHI,x
        sta kcto+1
        lda #KCFREE
        jmp kcells

; kcells: the cells of blocks kcfrom up to kcto, and not past the map's last,
; drawn when the map is drawn: in the style in A, KSDONE or KSMOVE, with each
; block's character in KCHAR, or as the screen code in A. kcfrom is left at
; the first block not drawn.
kcells: sta kcch
        lda kmapon
        beq kcdone
        lda kcto
        cmp #<KMAPEND
        lda kcto+1
        sbc #>KMAPEND
        bcc kcnext
        lda #<KMAPEND
        sta kcto
        lda #>KMAPEND
        sta kcto+1
kcnext: lda kcfrom
        cmp kcto
        lda kcfrom+1
        sbc kcto+1
        bcs kcdone
        jsr kcell
        inc kcfrom
        bne kcnext
        inc kcfrom+1
        jmp kcnext
kcdone: rts

; kcell: the cell of block kcfrom drawn in the style or as the screen code in
; kcch: on row kcfrom / 40 of the history's rows at kcfrom mod 40, which in 80
; columns is the same offset in both memories and so two columns. There a
; name's character is in the left column, auxiliary memory's, and a space of
; its kind in the right; any other cell is the same code twice. A block past
; the map isn't drawn.
kcell:  lda kcfrom
        sta kcv
        lda kcfrom+1
        sta kcv+1
        ldx #0
kcdiv:  lda kcv+1
        bne kcsub
        lda kcv
        cmp #KMAPCOLS
        bcc kcput
kcsub:  sec
        lda kcv
        sbc #KMAPCOLS
        sta kcv
        lda kcv+1
        sbc #0
        sta kcv+1
        inx
        cpx #KMAPROWS
        bne kcdiv
        rts
kcput:  lda logrowlo,x
        sta scr
        lda logrowhi,x
        sta scr+1
        lda kcch
        sta kcode
        sta kcright
        cmp #KSMOVE+1
        bcs kcdraw
        lda kcfrom              ; a style, with the block's character
        sta kcb
        lda kcfrom+1
        sta kcb+1
        jsr kchget
        ldx kcch
        bne kcmove
        and #$3F                ; done: in inverse
        sta kcode
        lda #KCINVSP
        bne kcpair
kcmove: cmp #' '
        bne kcname
        lda #KCFILL             ; to move, with no character: # in both columns
        sta kcode
        sta kcright
        bne kcdraw
kcname: ora #$80
        sta kcode
        lda #KCOFF
kcpair: sta kcright
kcdraw: ldy kcv
        bit wide
        bmi kcwide
        lda kcode
        sta (scr),y
        rts
kcwide: lda kcright
        sta (scr),y
        lda kcode
        sta PAGE2ON
        sta (scr),y
        sta PAGE2OFF
        rts

; kchclear: each block's character in KCHAR a space.
kchclear: lda #' '
        ldy #0
kcclear: sta KCHAR,y
        sta KCHAR+KMAPEND-256,y
        iny
        bne kcclear
        rts

; kchget: A, the character in KCHAR of block kcb, which is in the map. Y is
; overwritten.
kchget: ldy kcb
        lda kcb+1
        bne kcghigh
        lda KCHAR,y
        rts
kcghigh: lda KCHAR+256,y
        rts

; kchput: A put in KCHAR as the character of block kcb, which is in the map.
; Y is overwritten.
kchput: ldy kcb
        pha
        lda kcb+1
        bne kcphigh
        pla
        sta KCHAR,y
        rts
kcphigh: pla
        sta KCHAR+256,y
        rts

; kmovechars: the characters of the part's source blocks, klen of them from
; ksrc, copied to its destination blocks from kdst, first to last, which is
; safe as the destination is before the source. Nothing is copied for a source
; past the map.
kmovechars: clc
        lda ksrc
        adc klen
        sta kcb
        lda ksrc+1
        adc #0
        sta kcb+1
        lda kcb
        cmp #<KMAPPAST
        lda kcb+1
        sbc #>KMAPPAST
        bcs kmcdone
        lda #0
        sta kmi
kmcnext: lda kmi
        cmp klen
        beq kmcdone
        clc
        adc ksrc
        sta kcb
        lda ksrc+1
        adc #0
        sta kcb+1
        jsr kchget
        pha
        clc
        lda kmi
        adc kdst
        sta kcb
        lda kdst+1
        adc #0
        sta kcb+1
        pla
        jsr kchput
        inc kmi
        jmp kmcnext
kmcdone: rts

; putinv: the 0-terminated text at (ptr) into the field from Y, as puttext
; draws it, with each character whose high bit is set in inverse, and each
; byte below $20 as the flashing character $40 above it.
putinv: sty tmp
        ldy #0
pitext: lda (ptr),y
        beq pidone
        sty tmp2
        ldy tmp
        cmp #' '
        bcs pinorm
        ora #$40
        jsr pcput               ; the screen code as it is
        jmp pinext
pinorm: ldx #0
        cmp #$80
        bcc piput
        and #$7F
        ldx rowwidth
piput:  stx barcells
        jsr putch
pinext: sty tmp
        ldy tmp2
        iny
        bne pitext
pidone: ldx #0
        stx barcells
        ldy tmp
        rts

kmaptxt: .text "BLOCKS"
        .byte $3A, 0
; The legend, a high bit for inverse, and in 80 columns with its cells doubled.
klegend: .byte $A0
        .text " DONE  # TO GO  . FREE  "
        .byte KCREAD-$40
        .text " READ  "
        .byte KCWRITE-$40
        .text " WRITE"
        .byte 0
klegend80: .byte $A0, $A0
        .text " DONE   ## TO GO   .. FREE   "
        .byte KCREAD-$40, KCREAD-$40
        .text " READ   "
        .byte KCWRITE-$40, KCWRITE-$40
        .text " WRITE"
        .byte 0

kcrc:    .byte 0, 0             ; the host's CRC of the directory
kcnt:    .byte 0                ; the volume's file count
kcnthi:  .byte 0
kslot:   .byte 0                ; the entry being moved, from 1
kfiles:  .byte 0                ; the files moved
kat:     .byte 0, 0             ; where the next file starts once moved
ktotal:  .byte 0, 0             ; the blocks to move
kmoved:  .byte 0, 0             ; and moved so far
kblocks: .byte 0, 0             ; the entry's length
ksrc:    .byte 0, 0             ; the next block to move
kdst:    .byte 0, 0             ; and where it goes
kleft:   .byte 0, 0             ; the entry's blocks left to move
klen:    .byte 0                ; krpart: its blocks
ksoff:   .byte 0                ; its first block's place in the source track
kdoff:   .byte 0                ; and in the destination track
kidx:    .byte 0                ; the sector being framed
kpage:   .byte 0                ; its source's first page in RAWBUF
ktries:  .byte 0                ; kwrite: attempts left
kpos:    .byte 0, 0             ; krdirent: the entry's offset in the directory
kpatch:  .byte 0, 0, 0, 0       ; its new first and next blocks
kdsec:   .byte 0, 0             ; the directory sectors they are in
kj:      .byte 0                ; a directory sector, 0 to 7
kfield:  .byte 0                ; krdfield: the byte in the entry
kentry:  .byte 0                ; and the entry
keov:    .byte 0, 0             ; the volume's blocks
kmapon:  .byte 0                ; nonzero while the block map is drawn
kcfrom:  .byte 0, 0             ; kcells: the next block to draw
kcto:    .byte 0, 0             ; and the block after the last
kcch:    .byte 0                ; their screen code
kcv:     .byte 0, 0             ; kcell: the block, less 40 for each row above it
khdir:   .byte 0                ; khcopy: 1 back onto the screen
fssaved: .byte 0                ; nonzero while KHIST has the history from before a Full Send
khaux:   .byte 0                ; its high bit set for auxiliary memory's columns
khbuf:   .byte 0, 0             ; where the row goes in KHIST, or comes from
kalt:    .byte 0                ; kmapshow: bit 7 set when the alternate characters were selected
kcode:   .byte 0                ; kcell: the cell's screen code
kcright: .byte 0                ; and in 80 columns its right column's
kcb:     .byte 0, 0             ; kchget and kchput: the block
kmi:     .byte 0                ; kmovechars: the block being copied
kny:     .byte 0                ; krname: Y as it came
knlen:   .byte 0                ; the name's length

; Each block of a track's two sectors, as ProDOS and Apple Pascal map them;
; blocks 2 to 5, the directory, are from the fifth.
blkphys: .byte 0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15

        .byte 0

; The end of the program, where selfcrc stops.
progend:

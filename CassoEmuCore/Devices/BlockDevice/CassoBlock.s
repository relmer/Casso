; Casso's block firmware: the slot page of a hard disk card that Casso runs
; without Apple's ROM.
;
; The card copies each block to or from the Apple's memory itself, so a call
; takes a few dozen cycles rather than a byte loop's thousands. The page
; has the ID bytes of a SmartPort device: $20, $00, $03 and $00 at $Cn01 to
; $Cn07, which the boot ROM's slot scan looks for. At $CnFB to $CnFF it has
; the SmartPort ID byte, a block count of 0 (the driver's status call returns
; it), a status byte for two volumes that read, write, format and report
; status, and the ProDOS driver's entry. The SmartPort entry is 3 bytes past
; the ProDOS entry.
;
; The ProDOS driver writes ProDOS's request at $42 to $47 to the card's
; registers and reads the result:
;
;   $C0n0   the command: 0 status, 1 read, 2 write, 3 format
;   $C0n1   the unit, drive 2 in bit 7
;   $C0n2   the buffer, low byte, and $C0n3 the high byte
;   $C0n4   the block, low byte, and $C0n5 the high byte
;   $C0n6   read: runs the command and returns its error, 0 for none
;   $C0n7   read: X after the last call, and $C0n8 Y
;
; The SmartPort entry writes the caller's return address to the card. It
; points at the call's command byte and parameter list. The card reads them,
; runs the call and returns the address past them:
;
;   $C0n9   the return address, low byte, and $C0nA the high byte
;   $C0nB   read: runs the SmartPort call and returns its error
;   $C0nC   read: the address to return to, low byte, and $C0nD high
;   $C0nE   read: the last call's error
;
; The error is in A, with the C flag set for an error, which is ProDOS's and
; SmartPort's convention. X and Y have the block count for a ProDOS status
; call and the byte count for a SmartPort call.
;
; Boot reads block 0 of drive 1 to $0800 and jumps to $0801 with the slot
; times 16 in X. With no bootable disk it returns to the boot ROM's slot
; scan, which goes on to the next slot down.
;
; SLOT is assigned ahead of this source.

IO      = $C080 + SLOT * 16
BASE    = $C000 + SLOT * 256
CMD     = $42
UNIT    = $43
BUFLO   = $44
BUFHI   = $45
BLKLO   = $46
BLKHI   = $47
SLOOP   = $FABA                 ; the boot ROM's slot scan, past this slot

        .org BASE
        ldx #$20                ; $Cn01 = $20
        ldy #$00                ; $Cn03 = $00
        ldx #$03                ; $Cn05 = $03
        ldy #$00                ; $Cn07 = $00, a SmartPort device

boot    lda #1                  ; read block 0 to $0800
        sta CMD
        lda #SLOT * 16
        sta UNIT
        lda #0
        sta BUFLO
        sta BLKLO
        sta BLKHI
        lda #$08
        sta BUFHI
        jsr driver
        bcs noboot
        lda $0800
        beq noboot              ; an empty block 0 has nothing to run
        ldx #SLOT * 16
        jmp $0801

noboot  jmp SLOOP

driver  jmp prodos               ; the ProDOS entry

smart   pla                     ; the SmartPort entry: the return address
        sta IO + 9
        pla
        sta IO + 10
        lda IO + 11             ; runs the call
        lda IO + 13             ; return past the command and parameters
        pha
        lda IO + 12
        pha
        ldx IO + 7
        ldy IO + 8
        lda IO + 14
        cmp #1                  ; C is set for an error
        rts

prodos  lda CMD
        sta IO + 0
        lda UNIT
        sta IO + 1
        lda BUFLO
        sta IO + 2
        lda BUFHI
        sta IO + 3
        lda BLKLO
        sta IO + 4
        lda BLKHI
        sta IO + 5
        lda IO + 6              ; runs the command
        ldx IO + 7
        ldy IO + 8
        cmp #1                  ; C is set for an error
        rts

        .ds BASE + $FB - *
        .byte $80               ; SmartPort ID: extended calls
        .word 0                 ; the driver's status call returns the size
        .byte $1F               ; two volumes; status, read, write and format
        .byte <driver           ; the ProDOS entry

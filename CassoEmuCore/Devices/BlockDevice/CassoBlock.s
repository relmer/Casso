; Casso's block firmware: the slot page of a hard disk card that Casso runs
; without Apple's ROM.
;
; The card copies each block to or from the Apple's memory itself, so a call
; takes a few dozen cycles rather than a byte loop's thousands. The page
; has the ID bytes of a ProDOS block device: $20, $00, $03 and $3C at
; $Cn01 to $Cn07, which the boot ROM's slot scan looks for, and at $CnFC to
; $CnFF a block count of 0 (the driver's status call returns it), a status
; byte for two volumes that read, write and report status, and the driver's
; entry.
;
; The driver writes ProDOS's request at $42 to $47 to the card's registers
; and reads the result:
;
;   $C0n0   the command: 0 status, 1 read, 2 write, 3 format
;   $C0n1   the unit, drive 2 in bit 7
;   $C0n2   the buffer, low byte, and $C0n3 the high byte
;   $C0n4   the block, low byte, and $C0n5 the high byte
;   $C0n6   read: runs the command and returns its error, 0 for none
;   $C0n7   read: the unit's block count, low byte, and $C0n8 the high byte
;
; The error is in A, with the C flag set for an error, which is ProDOS's
; convention, and the block count is in X and Y.
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
        ldy #$3C                ; $Cn07 = $3C

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

driver  lda CMD
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
        ldx IO + 7
        ldy IO + 8
        lda IO + 6
        cmp #1                  ; C is set for an error
        rts

        .ds BASE + $FC - *
        .word 0                 ; the driver's status call returns the size
        .byte $17               ; two volumes; status, read and write
        .byte <driver           ; the driver's entry

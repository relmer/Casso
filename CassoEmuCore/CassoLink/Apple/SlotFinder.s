; Casso's CassoLink: the slot finder.
;
; Typed in, or read from tape, ahead of the 6551 shim, and run in its place.
; It writes the 6551's slot into the shim's header at $3B38 and starts the
; shim at $3B00. The slot is the input hook's when IN# set it to a slot's
; firmware, which a bootstrap typed in through the Apple's serial input has;
; otherwise the first slot, from 7 down, whose firmware has the Super Serial
; Card's identification bytes, as after a bootstrap from tape; otherwise the
; slot the shim is assembled with. On a //c, IN#1 and IN#2 select ports 1
; and 2, whose 6551s are where slots 1's and 2's would be.
;
; The shim saves the machine's zero page at $3A00 when it starts, over this
; code, which has then run.
;
; Built with SERIALLOAD, it is typed in alone, at $3A50, and reads the shim
; from the 6551 as raw bytes rather than as typed lines. It sends $A5 when it
; is ready, then takes a page count, that many pages for $3B00 up, and the
; XOR of every byte. With the right XOR it writes the slot and starts the
; shim; otherwise it jumps to the monitor, and the PC's wait for Ready ends
; in an error. It doesn't touch the zero page, so the shim saves the zero
; page as the user left it. Interrupts are off while it reads, so the
; firmware's interrupt handler doesn't take any of the bytes.

SHIM     = $3B00
SHIMSLOT = $3B38                ; the shim's 6551 slot
KSWH     = $39                  ; the input hook's high byte

        ifdef SERIALLOAD
ORIGIN   = $3A50
        else
ORIGIN   = $3AA0
        endif

        .org ORIGIN

        lda KSWH                ; $C1 to $C7: IN# and a slot
        cmp #$C1
        bcc scan
        cmp #$C8
        bcs scan
        and #$07
        bne found               ; always taken

scan:   ldy #7
next:   tya                     ; the slot's firmware page, $Cn
        ora #$C0
        sta id1+2
        sta id2+2
        sta id3+2
        sta id4+2
id1:    lda $C005
        cmp #$38
        bne skip
id2:    lda $C007
        cmp #$18
        bne skip
id3:    lda $C00B
        cmp #$01
        bne skip
id4:    lda $C00C
        cmp #$31
        bne skip
        tya
        bne found               ; always taken
skip:   dey
        bne next
        lda SHIMSLOT            ; no card found: the shim's own slot

found:  bit $CFFF               ; no card's expansion ROM left selected
        ifdef SERIALLOAD
        sta slotn
        asl a                   ; the 6551's index, slot * 16
        asl a
        asl a
        asl a
        tax
        php
        sei
        lda $C088,x             ; a byte left in the 6551 is dropped
txwait: lda $C089,x
        and #$10
        beq txwait
        lda #$A5                ; ready for the shim
        sta $C088,x
        jsr getb
        sta pages
        ldy #0
        sty sum
load:   jsr getb
store:  sta SHIM,y
        eor sum
        sta sum
        iny
        bne load
        inc store+2
        dec pages
        bne load
        jsr getb                ; the PC's XOR
        cmp sum
        bne bad
        plp
        lda slotn
        sta SHIMSLOT
        jmp SHIM
bad:    plp
        jmp $FF69               ; the monitor
getb:   lda $C089,x
        and #$08
        beq getb
        lda $C088,x
        rts
slotn:  .byte 0
pages:  .byte 0
sum:    .byte 0
        else
        sta SHIMSLOT
        jmp SHIM
        endif

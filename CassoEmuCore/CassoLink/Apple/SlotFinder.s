; Casso's CassoLink: the slot finder.
;
; Typed in, or read from tape, ahead of the 6551 shim, and run in its place.
; It writes the 6551's slot into the shim's header at $3D38 and starts the
; shim at $3D00. The slot is the input hook's when IN# set it to a slot's
; firmware, which a bootstrap typed in through the Apple's serial input has;
; otherwise the first slot, from 7 down, whose firmware has the Super Serial
; Card's identification bytes, as after a bootstrap from tape; otherwise the
; slot the shim is assembled with. On a //c, IN#1 and IN#2 select ports 1
; and 2, whose 6551s are where slots 1's and 2's would be.
;
; The shim saves the machine's zero page at $3C00 when it starts, over this
; code, which has then run.

SHIM     = $3D00
SHIMSLOT = $3D38                ; the shim's 6551 slot
KSWH     = $39                  ; the input hook's high byte

        .org $3CA0

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

found:  sta SHIMSLOT
        jmp SHIM
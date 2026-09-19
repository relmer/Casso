.include "hello.inc"
.macro twoinx
        inx
        inx
.endmacro
.segment "CODE"
start:  ldx #0
        twoinx
        jsr sub
        rts
sub:    lda #COUNT
        rts

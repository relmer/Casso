; ============================================================================
;  mockingboard-speech-engine.inc -- what both speech demos are made of
;
;  The chip is driven the same way whichever screen the eye is drawn on, so the
;  streams, the pacing, the song and the caption layout live here and are
;  assembled into both demos. What differs is the picture: how a character
;  reaches a text row, how the eye is painted, and how the screen is set up.
;
;  The including file must, BEFORE this file, define CAPLAST and CAPFIT --
;  the last column of a caption row, and one past the width, for the
;  does-this-word-fit test -- and declare its own soft switches. It must also,
;  anywhere, provide:
;
;      ScreenInit  set the display up and draw the eye at rest
;      EyeFlash / EyeFlashQ / EyeDecay / EyeRest   the eye. Each returns carry
;                  SET when it painted, so the delay after it can charge for
;                  the time the paint took
;      PutChar     character A at column Y of the row at CAPPTR
;      FLASHU, SNGFLASHL, decayUnits, decayLaps    what a paint costs, from
;                  the generator, in speech units and in song laps left
;
;  Nothing here is conditional on which version is being assembled. Where the
;  two machines differ the difference is behind one of those names, and where
;  a version has nothing to charge for it says so with a zero rather than with
;  an .ifdef.
; ============================================================================


; -- SSI 263A, slot 4, speech socket 1 ---------------------------------------
SPHON   = $C440            ; DR duration in D7:6, phoneme in D5:0; reads D7 = request
SINF    = $C441            ; inflection I10-I3
SRATE   = $C442            ; rate in D7:4, I11 in D3, I2-I0 in D2:0
SCTL    = $C443            ; CTL in D7, articulation in D6:4, amplitude in D3:0
SFILT   = $C444            ; vocal tract filter clock

; -- Attribute values --------------------------------------------------------
MODEDR  = $C0              ; DR=11 while still powered down: A/R active, phoneme timing
CTLUP   = $80              ; CTL set -- Power Down
CTLDN   = $5C              ; CTL clear, articulation 5, amplitude $C
FILTLO  = $B4              ; ~11.8 kHz tract clock -- a lower, heavier voice
FILTNOM = $D3              ; ~19.9 kHz -- the datasheet's nominal setting
FILTHI  = $E2              ; ~29.8 kHz -- a lighter, smaller voice
PHMASK  = $3F              ; the phoneme code within a phoneme byte
VOWELMAX = $1C             ; codes $01-$1C are the vowels, the syllable nuclei
DECAYU  = $08              ; spoken-path units between flash decay steps
SNGTAIL = $03              ; sung units over which a note's flash collapses
DELAY1L = $21              ; laps of the song's delay loop in one unit

; -- Stream escapes ----------------------------------------------------------
ESCCAP  = $FB
ESCWAIT = $FC
ESCPTCH = $FD
ESCFILT = $FE
ESCEND  = $FF

; -- Caption geometry --------------------------------------------------------
;  Mixed mode: HAL's eye fills the 160 graphics rows and the four text rows
;  beneath it carry three caption rows and the title. Text row bases are the
;  Apple's interleaved layout:
;  base = $400 + (row & 7) * $80 + (row >> 3) * 40. How wide those rows are,
;  and how a character reaches them, is the including file's business.
CAPROWS = $03
TITLE   = $07D0            ; text row 23

; -- Zero page ---------------------------------------------------------------
PTR     = $06              ; stream pointer (2 bytes)
UNITS   = $08              ; delay-unit countdown
CURPH   = $09              ; phoneme byte being sustained
NDUR    = $0A              ; remaining units in the current note
CAPPTR  = $0B              ; current caption row base (2 bytes)
CAPCOL  = $0D              ; next column in that row
CAPROW  = $0E              ; which of the four caption rows
CAPLEN  = $0F              ; characters left to print
TMP     = $10
RATEC   = $11              ; 16 - the current speech rate, for the poll deadline
HALF    = $12              ; units left after the mid-phoneme reload
GLOWPH  = $13              ; how many flash bands are currently white, 0-3
DECAYT  = $14              ; units until the next decay step
EYEP    = $15              ; screen pointer for the eye (2 bytes)
SRCP    = $17              ; source pointer: image, then span lists (2 bytes)
TMP2    = $19
TMP3    = $1A
FLASHQ  = $1B              ; a vowel has been written; flash at the reload
RLROW   = $1C              ; unpacker: graphics row
RLCOL   = $1D              ; unpacker: byte column
LAPS    = $1E              ; song: delay laps left in the current unit

start
    sei                         ; reset masks interrupts and nothing here
                                ; unmasks them; A/R reaches VIA #1's CA1 and
                                ; no handler stands behind $03FE

    jsr ScreenInit

    lda #$08                    ; a rate of 8, until a stream states its own
    sta RATEC

    ; Leave Power Down with the mode bits staged. Writing the duration bits
    ; while CTL is still set, then dropping CTL, is what latches the operating
    ; mode: A/R active, phoneme timing, transitioned inflection.
    lda #MODEDR
    sta SPHON
    lda #CTLUP
    sta SCTL
    lda #CTLDN
    sta SCTL

    lda #FILTNOM
    sta SFILT

    ; Rate and pitch both live in $C442 and both arrive from the streams, so
    ; there is nothing to set here. Leaving the inflection register alone would
    ; be a bug: it powers up at 0, which is 54.6 Hz -- a glottal period so long
    ; that a short phoneme spans barely two pulses and no vowel is resolvable.
    ; Every stream opens with a pitch escape for exactly that reason.

    ; ---- 1. HAL. A flat, deliberate line; the stream carries its own slower
    ;         rate in the high nibble of its opening pitch escape.
    lda #<halData
    ldx #>halData
    jsr Speak

    ; ---- 2. Joshua. A greeting that falls, then a question that rises
    ;         continuously under its final word.
    lda #<wargData
    ldx #>wargData
    jsr Speak

    ; ---- 3. A Centurion: the heavy tract, and a stream carrying its own low
    ;         pitch and slow rate.
    lda #FILTLO
    sta SFILT
    lda #<cylonData
    ldx #>cylonData
    jsr Speak
    lda #FILTNOM
    sta SFILT

    ; ---- 4. And then it sings.
    jsr Sing

    ; Silence the chip and stop. Power Down is the part's own quiescent state.
    lda #CTLUP
    sta SCTL
    jsr EyeRest
halt
    jmp halt


;  Speak -- play a phoneme stream, pacing on the chip's own request line
;
;  Entry: A = stream address low, X = stream address high
; ============================================================================
Speak
    sta PTR
    stx PTR+1

spk_next
    ldy #$00
    lda (PTR),y
    cmp #ESCEND
    beq spk_done
    cmp #ESCFILT
    beq spk_filt
    cmp #ESCPTCH
    beq spk_pitch
    cmp #ESCWAIT
    beq spk_wait
    cmp #ESCCAP
    beq spk_cap

    ; A plain phoneme: start it, step the pointer, then wait for the chip to
    ; ask for the next. The pointer advance happens BEFORE the poll so the
    ; answer to the request is as prompt as possible -- the chip stops sounding
    ; the moment its countdown expires, and every cycle spent before the next
    ; write is a cycle of decaying output.
    sta SPHON
    pha
    and #PHMASK                 ; a vowel is a syllable: queue a flash for it
    beq spk_novowel
    cmp #VOWELMAX+1
    bcs spk_novowel
    lda #$01
    sta FLASHQ
spk_novowel
    pla
    jsr PhonemeWait             ; A still holds the phoneme byte
    jsr Advance1
    jmp spk_next

spk_cap
    jsr Advance1
    jsr Caption
    jmp spk_next

spk_filt
    jsr Advance1
    ldy #$00
    lda (PTR),y
    sta SFILT
    jsr Advance1
    jmp spk_next

spk_pitch
    jsr Advance1
    ldy #$00
    lda (PTR),y                 ; I10-I3
    sta SINF
    jsr Advance1
    ldy #$00
    lda (PTR),y                 ; rate + I11 + I2-I0
    sta SRATE
    lsr a
    lsr a
    lsr a
    lsr a                       ; the rate nibble alone
    sta TMP
    lda #$10
    sec
    sbc TMP
    sta RATEC
    jsr Advance1
    jmp spk_next

spk_wait
    jsr Advance1
    ldy #$00
    lda (PTR),y
    jsr DelayUnits
    jsr Advance1
    jmp spk_next

spk_done
    rts


; ============================================================================
;  PhonemeWait -- hold for the length the phoneme's own DR bits asked for
;
;  A phoneme sounds for 4096 * (16 - rate) * (4 - DR) chip ticks. RATEC carries
;  16 - rate, refreshed by every pitch escape, and DR is D7:6 of the phoneme
;  byte, so the product is the length in units of 4096 ticks: 1 unit at the
;  fastest rate and shortest duration, 64 at the slowest and longest, which is
;  the 146 ms ceiling quoted at the top of this file.
;
;      DR = %11 -> 1 unit       DR = %01 -> 3 units
;      DR = %10 -> 2 units      DR = %00 -> 4 units
;
;  A unit here is 4096 machine cycles, matching what the chip's own countdown
;  spends on one, so the tempo is the one these streams were written against.
;
;  Halfway through, the phoneme is written again. That reloads a countdown that
;  is still running rather than restarting a chip that has fallen silent, which
;  keeps the line seamless whether the host's phoneme timer is fast or slow.
;
;  Entry: A = the phoneme byte, DR in D7:6.  Clobbers A, X, Y, TMP, UNITS,
;  HALF and CURPH -- none of which is live across a phoneme.
; ============================================================================
PhonemeWait
    sta CURPH                   ; kept for the reload halfway through

    lsr a
    lsr a
    lsr a
    lsr a
    lsr a
    lsr a                       ; DR alone, 0 to 3
    sta TMP
    lda #$04
    sec
    sbc TMP
    sta TMP                     ; 4 - DR, so 1 to 4

    lda #$00
pw_add
    clc
    adc RATEC
    dec TMP
    bne pw_add                  ; (16 - rate) * (4 - DR), 1 to 64

    sta TMP
    lsr a
    sta HALF                    ; the back half
    lda TMP
    sec
    sbc HALF
    sta UNITS                   ; the front half, never zero
    jsr BurnUnits

    ; The flash comes AFTER the reload. Painting three bands costs several
    ; thousand cycles, and the reload is what keeps a short phoneme sounding
    ; through the second half; it must not wait for the paint.
    lda HALF
    beq pw_tail
    sta UNITS
    lda CURPH
    sta SPHON                   ; reload the phoneme, still sounding
    jsr EyeFlashQ
    bcc pw_burn
    lda UNITS                   ; the flash spent FLASHU of the back half
    sec
    sbc #FLASHU
    bcc pw_done
    beq pw_done
    sta UNITS
pw_burn
    jmp BurnUnits
pw_tail
    jmp EyeFlashQ
pw_done
    rts


;  BurnUnits -- spend UNITS units of 4096 machine cycles
;
;  The inner pass is bit-dey-bne, eight cycles, and 256 of them make 2048; two
;  laps make the unit. The bit is there only to spend three cycles on nothing,
;  and it reads TMP because TMP is dead by now.
; ============================================================================
BurnUnits
bu_unit
    ldy #$00
bu_in1
    bit TMP
    dey
    bne bu_in1                  ; 256 passes, 2048 cycles
    ldy #$E0
bu_in2
    bit TMP
    dey
    bne bu_in2                  ; 224 passes, 1792: the eye has the rest
    dec DECAYT
    bne bu_next
    lda #DECAYU
    sta DECAYT
    jsr EyeDecay
    bcc bu_next
    lda UNITS                   ; the paint spent decayUnits,x: take them off,
    sec                         ; keeping at least one for the loop to end on
    sbc decayUnits,x
    bcc bu_next
    beq bu_next
    sta UNITS
bu_next
    dec UNITS
    bne bu_unit
    rts


;  Sing -- play the song stream
;
;  Each record holds a phoneme, a pitch, and a length in delay units. The
;  phoneme is re-written once per unit so the note sustains past the chip's
;  own duration limit; re-writing a sounding phoneme reloads its countdown and
;  nothing else, so the tone continues unbroken.
; ============================================================================
Sing
    lda #<songData
    sta PTR
    lda #>songData
    sta PTR+1

sng_next
    ldy #$00
    lda (PTR),y
    cmp #ESCEND
    beq sng_done
    cmp #ESCCAP                 ; tested before the record step, which is fixed-size
    bne sng_rec
    jsr Advance1
    jsr Caption
    jmp sng_next

sng_rec
    sta CURPH

    iny
    lda (PTR),y
    sta SINF                    ; I10-I3
    iny
    lda (PTR),y
    sta SRATE                   ; rate 0 + I11 + I2-I0
    iny
    lda (PTR),y
    sta NDUR

    ; Step over the four bytes of this record.
    lda PTR
    clc
    adc #$04
    sta PTR
    bcc sng_note
    inc PTR+1

sng_note
    ldx #DELAY1L
    stx LAPS
    ; A new note: if it is a vowel, it is a syllable, and the eye flashes.
    ; (The branch above once went straight to sng_hold, skipping this unless
    ; the record happened to cross a page -- so the song never flashed.)
    lda CURPH
    and #PHMASK
    beq sng_hold
    cmp #VOWELMAX+1
    bcs sng_hold
    jsr EyeFlash
    ldx #SNGFLASHL              ; the flash spent part of this unit
    stx LAPS

sng_hold
    lda CURPH
    sta SPHON                   ; (re)start the phoneme; sustains if already sounding
    ldx LAPS
    jsr DelayLaps
    ldx #DELAY1L
    stx LAPS
    dec NDUR
    bne sng_more
    jmp sng_next
sng_more
    ; A held note keeps the eye lit. The flash collapses over the note's last
    ; SNGTAIL units, one ring per unit, so the next syllable's flash still
    ; reads as a fresh pulse.
    lda NDUR
    cmp #SNGTAIL+1
    bcs sng_hold
    jsr EyeDecay
    bcc sng_hold
    lda decayLaps,x             ; the paint spent part of the next unit
    sta LAPS
    jmp sng_hold

sng_done
    rts


;  Advance1 -- step the stream pointer one byte
; ============================================================================
Advance1
    inc PTR
    bne adv_done
    inc PTR+1
adv_done
    rts


; ============================================================================
;  DelayUnits -- burn A delay units (A = 0 does nothing)
;  Delay1     -- burn one unit, about 41 ms of emulated time
;  DelayLaps  -- burn X laps of the unit's loop, 1280 cycles each
;
;  The unit is this program's own clock. Note length is measured in these, not
;  in chip frames, so the melody keeps its tempo regardless of how the chip's
;  phoneme timer is calibrated.
; ============================================================================
DelayUnits
    cmp #$00
    beq del_done
    sta UNITS
    jsr EyeRest                 ; nothing is being said
del_loop
    jsr Delay1
    dec UNITS
    bne del_loop
del_done
    rts

Delay1
    ldx #DELAY1L                ; 33 laps of 256 passes, ~5 cycles each
DelayLaps
d1_outer
    ldy #$00
d1_inner
    dey
    bne d1_inner
    dex
    bne d1_outer
    rts


;  Caption -- print the escape's text, or clear the caption area
;
;  Entry: PTR points at the length byte. A length of zero clears and resets.
;  Words are separated by a space, and a word that will not fit on the current
;  row starts the next one, so a caption never wraps mid-word.
; ============================================================================
Caption
    ldy #$00
    lda (PTR),y
    sta CAPLEN
    jsr Advance1
    lda CAPLEN
    bne cap_fit

    ; Length 0: blank the caption rows and start again at the top.
    lda #$00
    sta CAPROW
cap_clr
    jsr CaptionRow
    lda #$A0
    ldy #CAPLAST
cap_clr1
    jsr PutChar
    dey
    bpl cap_clr1
    inc CAPROW
    lda CAPROW
    cmp #CAPROWS
    bne cap_clr
    lda #$00
    sta CAPROW
    jsr CaptionRow
    rts

cap_fit
    ; Does the word (plus a leading space when mid-row) fit in a row?
    lda CAPCOL
    beq cap_nospace
    clc
    adc #$01                    ; the separating space
    sta TMP
    clc
    adc CAPLEN
    cmp #CAPFIT
    bcc cap_space
    jsr CaptionNext             ; start a fresh row instead of splitting a word
    jmp cap_nospace

cap_space
    ldy CAPCOL
    lda #$A0
    jsr PutChar
    lda TMP
    sta CAPCOL

cap_nospace
    ldy #$00
cap_put
    cpy CAPLEN
    beq cap_done
    lda (PTR),y
    sty TMP
    ldy CAPCOL
    jsr PutChar
    inc CAPCOL
    ldy TMP
    iny
    bne cap_put

cap_done
    ; Step the stream pointer past the characters just printed.
    lda CAPLEN
    clc
    adc PTR
    sta PTR
    bcc cap_ret
    inc PTR+1
cap_ret
    rts


; ============================================================================
;  CaptionNext -- move to the next caption row, wrapping to the first
;  CaptionRow -- point CAPPTR at row CAPROW and reset the column
; ============================================================================
CaptionNext
    inc CAPROW
    lda CAPROW
    cmp #CAPROWS
    bcc CaptionRow
    lda #$00
    sta CAPROW

CaptionRow
    ldx CAPROW
    lda capRowLo,x
    sta CAPPTR
    lda capRowHi,x
    sta CAPPTR+1
    lda #$00
    sta CAPCOL
    ; Blank the row we are about to write into.
    lda #$A0
    ldy #CAPLAST
cr_blank
    jsr PutChar
    dey
    bpl cr_blank
    rts


; Text rows 20-22 on page 1, the three above the title.
capRowLo
    .byte $50,$D0,$50
capRowHi
    .byte $06,$06,$07

titleText
    .byte $CD,$CF,$C3,$CB,$C9,$CE,$C7,$C2,$CF,$C1,$D2,$C4,$A0
    .byte $D3,$D0,$C5,$C5,$C3,$C8,$A0,$C3,$C8,$C9,$D0
    .byte $00

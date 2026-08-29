; ============================================================================
;  casso-rocks-stage2.a65 ? second-stage loader for the casso-rocks demo
;
;  Loaded by stage 1 (casso-rocks.a65) into $1000-$1FFF: stage 1 reads
;  track 3 (with DBASE=$10) which lands sector S at page $10+S. Sectors
;  0-1 of track 3 contain this stage 2 code (entered at $1000); sectors
;  2-5 contain the 1 KB precomputed LoRes test pattern (lands at
;  $1200-$15FF and gets memcpy'd into text page 1 when the user cycles
;  to LoRes mode).
;
;  At entry, the disk head is at track 3, HGR page 1 ($2000-$3FFF) holds
;  the cassowary framebuffer, and X = $60 (slot 6 << 4) for indexed
;  soft-switch reads on the disk controller.
;
;  Stage 2:
;
;    Ask phase:
;      1. Puts the monitor question on the text screen. NOTHING CAN
;         DETECT THE MONITOR -- no Apple II can. Every video connector
;         the machine has is output only: no sense line, and no way to
;         read the signal back. So the demo asks, the way period
;         software carrying two sets of art did. TEXT is the one mode
;         that reads equally well on both kinds of monitor, which is
;         what makes the question safe to display before knowing the
;         answer.
;
;    Load phase (behind the question, so it costs no wall clock):
;      2. Loads tracks 4+5 into main $8000-$9FFF (the monochrome DHGR
;         main half; stage 1 already loaded the aux half to $6000
;         before jumping here), then calls stage_dhgr to copy main
;         $8000 -> main $2000 and main $6000 -> aux $2000
;         (RAMWRT-toggled). Copies only: the display stays in TEXT.
;         This has to happen HERE, before the color halves land on the
;         same staging further down.
;      3. Loads tracks 6+7 into main $4000-$5FFF, which IS HGR page 2.
;         That is the monochrome HGR cassowary, and because it lands on
;         a real display page it needs no stash and no copy: mode 1 is
;         one soft-switch write.
;      4. Loads tracks 8+9 and 10+11 back into the two staging areas
;         the init phase has just finished with, main $6000-$7FFF and
;         main $8000-$9FFF. They now hold the COLOR DHGR cassowary's
;         aux and main halves, so mode 2 is a second enter_dhgr call
;         over the same addresses and costs no memory of its own.
;      5. Loads tracks 12+13 into main $A000-$BFFF (the color HGR
;         cassowary, loaded directly to its stash location --
;         mode_hgr_color just memcpys $A000 -> $2000 on demand).
;
;      Because the staging areas are reused, mode 0 is the one mode
;      that cannot be re-entered: the monochrome halves it was built
;      from have been overwritten by the color ones.
;
;    Answer:
;      6. Waits for C or M -- folded to upper case, so either case
;         works -- and starts the cycle on the pair that answer picks:
;         M at mode 0, C at mode 2. Everything is already resident by
;         now, so the picture appears immediately. A key pressed during
;         the loads is still latched in the keyboard when we get here,
;         so an impatient answer is not lost. ESC quits from the
;         question as well as from the cycle.
;
;    Polling loop:
;      Each keystroke advances the mode by 1, and past the last mode
;      the cycle WRAPS to mode 1 rather than exiting -- the question
;      chooses where the cycle starts, so a color user would otherwise
;      never reach the monochrome images at all. Mode 0 is the one it
;      cannot rejoin, for the reason above. ESC ($9B) exits to BASIC at
;      any time, and is now the only way out.
;
;      THE SAME PHOTO FOUR WAYS, the monochrome pair first and then the
;      color pair. Every one of these framebuffers is decoded two
;      different ways depending on the monitor, and the two decodes want
;      opposite things from the encoder, so each image is authored for
;      one of them and reads as noise on the other. Nothing on this disk
;      can DETECT which monitor is attached, so all four ship and the
;      question above picks the pair to start on. Only the title is
;      drawn on the images now; it goes on the color-cell grid, which
;      keeps it legible through EITHER decode, because the cycle wraps
;      and either answer can end up looking at the other pair. See
;      scripts/DhgrCassowaryGen.py and scripts/HgrCassowaryGen.py.
;
;      Modes:
;        0 = DHGR monochrome: 560x192 one-bit, on aux+main $2000-$3FFF
;            (entered automatically at boot). The highest resolution
;            the machine has.
;        1 = HGR monochrome: 280x192 one-bit, on HGR page 2
;            ($4000-$5FFF). Half the horizontal resolution of mode 0,
;            because an HGR pixel paints two half-dots rather than one.
;        2 = DHGR color: 140 sixteen-color cells, restored from the
;            staging at main $6000 and main $8000.
;        3 = HGR color: 6-color NTSC artifact, restored from main $A000.
;        4 = LoRes (GR): 16-color bars on text page 1 ($0400-$07FF).
;
;      The HGR color test bands used to be a mode here. They were a
;      by-eye check that the renderer had not swapped blue/orange or
;      violet/green, and that question is now asked directly by
;      VideoModeTests, so the 8 KB they held became HGR page 2 for the
;      monochrome HGR image instead.
;
;    Exit:
;      JMP ($FFFC) hands off to the //e RESET.MGR which does the
;      vanilla power-on cleanup before entering Applesoft. We
;      pre-clear both text pages (main and aux) ourselves because
;      the reset handler doesn't.
;
;  Stage 2 calls back into stage 1's still-resident subroutines via
;  hard-coded entry-point addresses (stage 1 lives at $0801-$08FF).
;  Both files must be edited in lockstep if either is moved.
; ============================================================================

        ORG $1000

; -- I/O & soft switches ----------------------------------------------------
KBD        EQU $C000          ; keyboard data + strobe (bit 7)
STORE80OFF EQU $C000          ; same address as KBD; 80STORE off (write-only)
STROBE     EQU $C010          ; clear keyboard strobe (any access)
STORE80ON  EQU $C001          ; PAGE2 routes to aux when set
COL80OFF   EQU $C00C          ; 40-column text mode
COL80ON    EQU $C00D          ; 80-column text mode on
RAMWRTOFF  EQU $C004          ; writes go to main
RAMWRTON   EQU $C005          ; writes go to aux
TXTOFF     EQU $C050
TXTON      EQU $C051
MIXOFF     EQU $C052
PG2OFF     EQU $C054
PG2ON      EQU $C055
HIRESOFF   EQU $C056
HIRESON    EQU $C057
DHIRESON   EQU $C05E          ; DHIRES on  (with COL80ON+STORE80ON+HIRES)
DHIRESOFF  EQU $C05F          ; DHIRES off (back to normal HGR)

MOTOROFF   EQU $C0E8          ; slot 6 Disk II motor-off soft switch
                           ; (writing arms the ~1s spindown timer; the
                           ; //e RESET.MGR does NOT touch this, so a
                           ; demo that doesn't issue it explicitly
                           ; leaves the drive spinning until Ctrl-Reset)

; -- Stage 1 entry points (must match casso-rocks.a65 exactly) -------------
S1_LOAD_A     EQU $0817       ; sta DBASE / step head a / jmp read_track
S1_LOAD_B     EQU $0828       ; sta DBASE / step head b / jmp read_track

; -- Zero page ---------------------------------------------------
MODE       EQU $46            ; current display mode (0..4)
SRC_PTR    EQU $48            ; ($48),y indirect-indexed source pointer
DST_PTR    EQU $4A            ; ($4A),y indirect-indexed dest pointer


START
    ; --- Ask first, load behind the question ------------------------------
    ; The whole ~2.2 seconds of disk loading happens while the user is
    ; reading the question, so asking costs no wall clock at all. See
    ; the header for why asking is the only option available.
    JSR SHOW_PROMPT
    LDX #$60               ; show_prompt clobbers X; the loader wants $60

    ; --- Load: the monochrome DHGR main half, then stage the pair ---------
    ; Stage 1 already loaded the monochrome aux half to main $6000 and
    ; the head is at track 3. Two more disk reads and both halves are in
    ; the staging.
    LDA #$80
    JSR S1_LOAD_B          ; tracks 3 -> 4 (odd->even)
    LDA #$90
    JSR S1_LOAD_A          ; tracks 4 -> 5 (even->odd)

    ; Copy them into the framebuffer NOW, while the staging still holds
    ; them -- the color halves land on those same 16 KB below. Copies
    ; only; the display stays in TEXT behind the question.
    JSR STAGE_DHGR

    ; stage_dhgr -> copy_block clobbers X, but stage 1's load_a/load_b
    ; need X=$60 (slot 6 << 4) for the indexed soft-switch reads
    ; into the disk controller. Restore it before resuming disk I/O.
    LDX #$60

    ; --- Background: load the monochrome HGR cassowary onto HGR page 2 ---
    ; $4000-$5FFF is a real display page, so this one needs no stash and
    ; no copy: mode 1 just flips PAGE2. Loading in mode order also means
    ; the data for the next mode is always the next thing to arrive.
    LDA #$40
    JSR S1_LOAD_B          ; tracks 5 -> 6
    LDA #$50
    JSR S1_LOAD_A          ; tracks 6 -> 7

    ; --- Background: load the color DHGR halves over the staging ---------
    ; enter_dhgr has already copied both monochrome halves out of $6000
    ; and $8000 into the framebuffer, so those 16 KB are free. The color
    ; halves go straight back into them, which makes mode 2 nothing but
    ; a second enter_dhgr call.
    LDA #$60
    JSR S1_LOAD_B          ; tracks 7 -> 8
    LDA #$70
    JSR S1_LOAD_A          ; tracks 8 -> 9
    LDA #$80
    JSR S1_LOAD_B          ; tracks 9 -> 10
    LDA #$90
    JSR S1_LOAD_A          ; tracks 10 -> 11

    ; --- Background: load the color HGR cassowary to its stash location --
    ; Mode 3 restores it by copying $A000 -> $2000, so we can land the
    ; disk read straight there and skip a memcpy.
    LDA #$A0
    JSR S1_LOAD_B          ; tracks 11 -> 12
    LDA #$B0
    JSR S1_LOAD_A          ; tracks 12 -> 13

    ; --- The answer -------------------------------------------------------
    ; Everything is resident by now, so whichever pair they pick appears
    ; at once. A key pressed during the loads is still sitting in the
    ; keyboard latch, so an early answer is honored rather than eaten.
WAIT_ANSWER
    LDA KBD
    BPL WAIT_ANSWER
    BIT STROBE
    AND #$DF               ; fold lower case onto upper; ESC ($9B) has
                           ; bit 5 clear already and passes through
    CMP #$C3               ; "C" -> start on the color pair
    BEQ ANSWER_COLOR
    CMP #$CD               ; "M" -> start on the monochrome pair
    BEQ ANSWER_MONO
    CMP #$9B               ; ESC -> quit without showing anything
    BEQ DO_EXIT
    JMP WAIT_ANSWER

ANSWER_MONO
    ; Mode 0. stage_dhgr has already put the monochrome halves in the
    ; framebuffer, so all that is left is the display switches.
    LDA #0
    STA MODE
    JSR SHOW_DHGR
    JMP POLL

ANSWER_COLOR
    ; Mode 2. The staging holds the color halves now, and re-staging
    ; from it is exactly what mode_dhgr_color does.
    LDA #2
    STA MODE
    JMP MODE_DHGR_COLOR


POLL
    LDA KBD
    BPL POLL
    CMP #$9B               ; ESC?
    BEQ DO_EXIT
    BIT STROBE

    INC MODE
    LDA MODE
    CMP #1
    BEQ MODE_HGR_MONO
    CMP #2
    BEQ MODE_DHGR_COLOR
    CMP #3
    BEQ MODE_HGR_COLOR
    CMP #4
    BEQ MODE_LORES

    ; Past the last mode, wrap to mode 1 rather than exiting. The
    ; question chooses where the cycle STARTS, so a color user who
    ; began at mode 2 would otherwise never reach the monochrome
    ; images. Mode 0 is the one the cycle cannot rejoin -- the staging
    ; it was built from holds the color halves now -- so it rejoins at
    ; mode 1. ESC is the way out.
    LDA #1
    STA MODE
    JMP MODE_HGR_MONO


DO_EXIT
    ; Drop to BASIC via the official //e reset vector. Going through
    ; ($FFFC) hands off to the //e RESET.MGR which does the full
    ; power-on cleanup (40-col TEXT, no DHIRES, no 80STORE, etc.)
    ; before entering Applesoft. JMP $E000 directly is fragile ?
    ; even after manual soft-switch cleanup it JMPs through stale
    ; work-area pointers and lands in video memory.
    ;
    ; The reset handler doesn't clear the screen, so we wipe both
    ; text pages first. THE GOTCHA: with 80STORE on (which DHGR
    ; mode set), writes to $0400-$07FF route by PAGE2 status, NOT
    ; by RAMWRT. So we turn 80STORE off entirely ? simpler than
    ; juggling PAGE2, and the reset handler will sort the rest.
    STA STORE80OFF         ; 80STORE off ? RAMWRT now routes text writes

    STA RAMWRTON
    JSR CLEAR_TEXT_PAGE1
    STA RAMWRTOFF
    JSR CLEAR_TEXT_PAGE1

    STA MOTOROFF           ; arm Disk II spindown -- without this the
                           ; drive motor stays latched on across the
                           ; reset because the //e RESET.MGR never
                           ; touches $C0E8.

    JMP ($FFFC)            ; reset vector -> //e RESET.MGR -> Applesoft


; ----------------------------------------------------------------------------
; clear_text_page1 ? stamp $0400-$07FF with $A0 (space). Routing to
;                    aux vs main is the caller's responsibility (set
;                    RAMWRT first; only meaningful with 80STORE off).
; ----------------------------------------------------------------------------
CLEAR_TEXT_PAGE1
    LDX #0
    LDA #$A0
CLEAR_TEXT_LOOP
    STA $0400,X
    STA $0500,X
    STA $0600,X
    STA $0700,X
    INX
    BNE CLEAR_TEXT_LOOP
    RTS


; ----------------------------------------------------------------------------
; mode_hgr_mono -- show the monochrome HGR cassowary. It was loaded
;                 straight onto HGR page 2 and nothing has touched it
;                 since, so there is no copy to do: leave DHGR, select
;                 HGR, and flip to PAGE2.
;
;                 The DHGR switches have to come off explicitly. 80STORE
;                 in particular reroutes $2000-$3FFF by PAGE2, and while
;                 that is exactly what this mode wants PAGE2 to mean, it
;                 is not what it means for the NEXT mode's copies.
; ----------------------------------------------------------------------------
MODE_HGR_MONO
    STA STORE80OFF
    STA COL80OFF
    BIT DHIRESOFF

    BIT TXTOFF
    BIT HIRESON
    BIT MIXOFF
    BIT PG2ON
    JMP POLL


; ----------------------------------------------------------------------------
; mode_dhgr_color -- show the color DHGR cassowary. The background load
;                  has replaced the monochrome halves in the staging
;                  areas with the color ones, so this is enter_dhgr over
;                  the exact same addresses.
;
;                  80STORE MUST GO OFF FIRST. enter_dhgr's second copy
;                  reaches aux $2000 by turning RAMWRT on, and that only
;                  works while 80STORE is off -- with 80STORE on and
;                  HIRES on, the //e routes $2000-$3FFF by PAGE2 and
;                  ignores RAMWRT entirely. The boot call gets away
;                  without this because it runs before enter_dhgr has
;                  ever set 80STORE; a later call does not, and lands
;                  the aux half in main $2000 on top of the main half.
;                  enter_dhgr turns 80STORE back on at the end.
; ----------------------------------------------------------------------------
MODE_DHGR_COLOR
    STA STORE80OFF
    JSR ENTER_DHGR
    JMP POLL


; ----------------------------------------------------------------------------
; mode_hgr_color -- restore the color HGR cassowary. Copies the stashed
;                  image from main $A000 back to main $2000 (DHGR has
;                  clobbered it), turns off DHGR, and selects HGR PAGE1.
;                  PAGE1 matters as much as the copy: mode 1 left PAGE2
;                  selected, and page 2 is the monochrome image.
; ----------------------------------------------------------------------------
MODE_HGR_COLOR
    ; Disable DHGR-specific switches before restoring the framebuffer
    ; so the renderer doesn't briefly try to draw HGR-as-DHGR.
    STA STORE80OFF
    STA COL80OFF
    BIT DHIRESOFF

    LDX #$A0
    LDY #$20
    JSR COPY_BLOCK

    BIT TXTOFF
    BIT HIRESON
    BIT MIXOFF
    BIT PG2OFF
    JMP POLL


MODE_LORES
    ; Copy 1 KB LoRes pattern from $1200-$15FF to $0400-$07FF, then
    ; switch to LoRes display (TEXT off, HIRES off, MIXED off,
    ; PAGE2 off). All four 256-byte copies done in a single loop
    ; with X as the offset (low byte of each address).
    BIT PG2OFF
    BIT HIRESOFF
    LDX #0
COPY_LO
    LDA $1200,X
    STA $0400,X
    LDA $1300,X
    STA $0500,X
    LDA $1400,X
    STA $0600,X
    LDA $1500,X
    STA $0700,X
    INX
    BNE COPY_LO
    JMP POLL


; ----------------------------------------------------------------------------
; enter_dhgr ? populate aux+main $2000 from the staged DHGR scratch
;              areas at main $8000 (main half) and main $6000 (aux half),
;              then enable the //e DHGR display switches. Called from
;              start (mode 0 init); not part of the cycle dispatch.
;
;              See copy_block below for the RAMWRT-vs-zp interaction
;              that makes the aux copy work.
; ----------------------------------------------------------------------------
ENTER_DHGR
    JSR STAGE_DHGR
    ; falls through into show_dhgr


; ----------------------------------------------------------------------------
; show_dhgr -- enable the DHGR display switches: 80STORE on, AN3/DHIRES
;             on, 80COL on, TEXT off, HIRES on, PAGE1. Split out from
;             enter_dhgr because the boot path stages the framebuffer
;             long before it wants to leave the text screen: the
;             question is still up while the disk loads.
; ----------------------------------------------------------------------------
SHOW_DHGR
    STA STORE80ON
    STA DHIRESON
    STA COL80ON
    BIT TXTOFF
    BIT HIRESON
    BIT PG2OFF
    RTS


; ----------------------------------------------------------------------------
; stage_dhgr -- populate aux+main $2000 from the staging areas without
;              touching the display. See copy_block below for the
;              RAMWRT-vs-zp interaction that makes the aux copy work,
;              and mode_dhgr_color for why 80STORE has to be off.
; ----------------------------------------------------------------------------
STAGE_DHGR
    ; Copy main $8000-$9FFF -> main $2000-$3FFF (clobbers whatever HGR
    ; page 1 held, which is why the HGR images have stashes of their own).
    LDX #$80
    LDY #$20
    JSR COPY_BLOCK

    ; Copy main $6000-$7FFF -> aux $2000-$3FFF.
    STA RAMWRTON
    LDX #$60
    LDY #$20
    JSR COPY_BLOCK
    STA RAMWRTOFF
    RTS


SHOW_PROMPT
    ; 40-column text, page 1, main RAM. Nothing has touched 80STORE or
    ; RAMWRT yet, so writes to $0400-$07FF land in main text page 1,
    ; which is what a 40-column display reads.
    STA COL80OFF
    BIT TXTON
    BIT PG2OFF
    BIT MIXOFF
    JSR CLEAR_TEXT_PAGE1

    ; Row 11 column 4 is $05AC, row 13 column 4 is $06AC, by the text
    ; screen's own interleave: base = $0400 + 128*(row mod 8) + 40*(row/8).
    LDX #0
SP_ASK
    LDA ASK_LINE,X
    BEQ SP_KEYS_INIT
    ORA #$80               ; the text screen wants high ASCII. The
    STA $05AC,X            ; strings are stored plain so that as65 and
    INX                    ; Merlin emit the same bytes for them --
    BNE SP_ASK             ; their high-ASCII string forms differ.
SP_KEYS_INIT
    LDX #0
SP_KEYS
    LDA KEYS_LINE,X
    BEQ SP_DONE
    ORA #$80
    STA $06AC,X
    INX
    BNE SP_KEYS
SP_DONE
    RTS


ASK_LINE
    ASC 'WHICH MONITOR ARE YOU USING?'
    DFB $00
KEYS_LINE
    ASC 'C = COLOR      M = MONOCHROME'
    DFB $00


; ----------------------------------------------------------------------------
; copy_block -- copy 8 KB (32 pages) from main:src to (main or aux):dst.
;              Caller sets X = src high byte, Y = dst high byte, and toggles
;              RAMWRT before/after the call when an aux dest is needed.
;
;              Uses (SRC_PTR),y / (DST_PTR),y so all addressing -- including
;              the page-counter increment -- flows through zp, which under
;              ALTZP=off is always main RAM and so unaffected by RAMWRT.
;              Self-modifying-code variants would break here because RAMWRT
;              routes the inc's writeback to aux, breaking the running code.
; ----------------------------------------------------------------------------
COPY_BLOCK
    STX SRC_PTR+1
    STY DST_PTR+1
    LDA #0
    STA SRC_PTR
    STA DST_PTR
    LDX #32                ; 32 pages = 8 KB
COPY_BLOCK_PAGE
    LDY #0
COPY_BLOCK_BYTE
    LDA (SRC_PTR),Y
    STA (DST_PTR),Y
    INY
    BNE COPY_BLOCK_BYTE
    INC SRC_PTR+1
    INC DST_PTR+1
    DEX
    BNE COPY_BLOCK_PAGE
    RTS

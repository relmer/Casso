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
;         works. THE ANSWER SELECTS THE PAIR, it does not merely pick a
;         starting point: a monochrome answer shows the monochrome
;         images and nothing else. Everything is already resident by
;         now, so the picture appears immediately. A key pressed during
;         the loads is still latched in the keyboard when we get here,
;         so an impatient answer is not lost. ESC quits from the
;         question as well as from the cycle.
;
;    Polling loop:
;      Three steps, and which images they are depends on the answer.
;      Each keystroke advances one step; past the last one the demo
;      exits to BASIC. ESC ($9B) exits from anywhere, the question
;      included.
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
;      Steps, monochrome answer:
;        0 = DHGR: 560x192 one-bit, on aux+main $2000-$3FFF. The
;            highest resolution the machine has.
;        1 = HGR: 280x192 one-bit, on HGR page 2 ($4000-$5FFF). Half
;            the horizontal resolution of step 0, because an HGR pixel
;            paints two half-dots rather than one.
;
;      Steps, color answer:
;        0 = DHGR: 140 sixteen-color cells, staged from main $6000 and
;            main $8000.
;        1 = HGR: 6-color NTSC artifact, restored from main $A000.
;
;      Step 2, both answers: the hi-res color-mask sweep, which runs
;      until a key is pressed. It replaced the LoRes bars, which were a
;      palette demo that a monochrome monitor renders as sixteen levels
;      of one color -- whereas the sweep is worth watching either way,
;      since half of what it walks through is dot PATTERN rather than
;      hue. See mode_sweep.
;
;      The two images the answer did not pick stay loaded and simply
;      never appear.
;
;      The HGR color test bands used to be a mode here. They were a
;      by-eye check that the renderer had not swapped blue/orange or
;      violet/green, and that question is now asked directly by
;      VideoModeTests, so the 8 KB they held became HGR page 2 for the
;      monochrome HGR image instead.
;
;    Exit:
;      Both text pages are wiped, the sign-off goes onto page 1, the
;      firmware hooks and the 40-column text window go back, and then
;      Applesoft's cold start takes over. All of it happens while the
;      display is still showing the picture, so the screen changes
;      once: from the last image to the sign-off with the prompt
;      under it.
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

; -- Monitor entry points (published, and the same on the ][, ][+, //e, //c)
BASIC      EQU $E000          ; Applesoft cold start
INIT       EQU $FB2F          ; 40-column TEXT on, full window, cursor to row 23
VTAB       EQU $FC22          ; recompute BASL/BASH from CV
SETKBD     EQU $FE89          ; KSW back to KEYIN
SETVID     EQU $FE93          ; CSW back to COUT1

; -- Stage 1 entry points (must match casso-rocks.a65 exactly) -------------
S1_LOAD_A     EQU $0817       ; sta DBASE / step head a / jmp read_track
S1_LOAD_B     EQU $0828       ; sta DBASE / step head b / jmp read_track

; -- Zero page ---------------------------------------------------
STEP       EQU $46            ; step within the chosen pair's cycle (0..2)
PAIR       EQU $47            ; 0 = the monochrome images, 1 = the color ones
MASK       EQU $48            ; the sweep's current color mask
ODDMASK    EQU $49            ; what it paints odd bytes with
SRC_PTR    EQU $4C            ; ($4C),y indirect-indexed source pointer
DST_PTR    EQU $4A            ; ($4A),y indirect-indexed dest pointer

; -- Monitor zero page (where BASIC's prompt lands on the way out) --------
CH         EQU $24            ; cursor column within the text window
CV         EQU $25            ; cursor row


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

    ; Every track is in, so stop the drive. Nothing reads it again, and
    ; nothing else in the demo -- or in the //e's reset handler --
    ; touches $C0E8, so without this the motor stays latched on for as
    ; long as the demo is up, and past the exit as well.
    STA MOTOROFF

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
    CMP #$C3               ; "C" -> the color images
    BEQ ANSWER_COLOR
    CMP #$CD               ; "M" -> the monochrome images
    BEQ ANSWER_MONO
    CMP #$9B               ; ESC -> quit without showing anything
    BEQ DO_EXIT
    JMP WAIT_ANSWER

; THE QUESTION COMES OFF THE TEXT PAGE THE MOMENT IT IS ANSWERED, and
; not on the way out. The text page is not on display once the pictures
; are up, but it is still what the machine falls back to: Ctrl-Reset,
; the emulator's own reset button, and anything else that puts the //e
; back in TEXT shows whatever is sitting there. Leaving the question
; there meant those landed on a screen that still read as a prompt and
; no longer answered to C or M.
ANSWER_MONO
    ; stage_dhgr has already put the monochrome halves in the
    ; framebuffer, so step 0 is the display switches alone.
    LDA #0
    STA PAIR
    STA STEP
    JSR CLEAR_TEXT_BOTH
    JSR SHOW_DHGR
    JMP POLL

ANSWER_COLOR
    LDA #1
    STA PAIR
    LDA #0
    STA STEP
    JSR CLEAR_TEXT_BOTH
    JMP MODE_DHGR          ; takes the color branch and stages it


POLL
    LDA KBD
    BPL POLL
    BIT STROBE             ; BEFORE the ESC test, not after it. ESC used
                           ; to leave the strobe set, so Applesoft's
                           ; GETLN read the ESC as its own first
                           ; keystroke, went into cursor-move mode, and
                           ; swallowed the character typed after it.
    CMP #$9B               ; ESC?
    BEQ DO_EXIT

    ; Three steps: the chosen pair's DHGR, then its HGR, then the
    ; hi-res sweep. Past the last one, fall through into do_exit.
    INC STEP
    LDA STEP
    CMP #1
    BNE POLL_CHK2
    JMP MODE_HGR           ; JMP, not beq: DO_EXIT sits between the
POLL_CHK2                  ; dispatch and the handlers, and it has grown
    CMP #2                 ; past a relative branch's reach
    BNE DO_EXIT
    JMP MODE_SWEEP


DO_EXIT
    ; THE DISPLAY STAYS IN GRAPHICS UNTIL THE TEXT PAGE IS FINISHED.
    ; Everything below runs before INIT turns TEXT back on, so the
    ; screen changes once -- from the last picture to the sign-off with
    ; the prompt under it -- rather than showing a half-built page on
    ; the way.
    ;
    ; The soft switches go back by hand rather than being left to the
    ; reset handler, which restores enough for a machine that was
    ; already in text but does not clear 80COL. Exiting from a DHGR step
    ; therefore dropped into Applesoft with the 80-column hardware on
    ; and the 80-column firmware not hooked up to match: half-width
    ; glyphs, and typing that behaved strangely because COUT was writing
    ; 40-column output onto an 80-column screen.
    ;
    ; 80STORE goes off first for a second reason: with it on, writes to
    ; $0400-$07FF route by PAGE2 rather than by RAMWRT, and the wipe
    ; below depends on RAMWRT.
    STA STORE80OFF         ; 80STORE off -- RAMWRT now routes text writes
    STA COL80OFF
    BIT DHIRESOFF
    BIT MIXOFF
    BIT PG2OFF

    JSR CLEAR_TEXT_BOTH

    ; The sign-off, poked onto the page the way the question was. Row 21
    ; column 2 is $06D2 by the text screen's own interleave, and 35
    ; characters at column 2 is centered in 40.
    LDX #0
SP_BYE
    LDA BYE_LINE,X
    BEQ SP_BYE_DONE
    ORA #$80
    STA $06D2,X
    INX
    BNE SP_BYE
SP_BYE_DONE

    ; The firmware hooks, then INIT. SETKBD/SETVID first is
    ; load-bearing: without them the //e's own reset path finds the
    ; 80-column firmware still hooked up, restores it, and turns 80COL
    ; back on.
    JSR SETKBD
    JSR SETVID
    JSR INIT               ; 40-column TEXT on, full window

    ; Where BASIC's prompt lands. Applesoft emits exactly two carriage
    ; returns between here and its first prompt -- one in the cold start
    ; and one at the head of the READY loop -- and GETLN then prints "]"
    ; wherever the cursor ended up. Starting on the sign-off's own row
    ; therefore puts the prompt on row 23 with a blank line between,
    ; and nothing scrolls. INIT would have left the cursor on row 23,
    ; where those two returns would have scrolled the sign-off up two.
    LDA #21
    STA CV
    JSR VTAB               ; BASL/BASH from CV
    LDA #0
    STA CH

    ; Applesoft's cold start, direct. It sets up its own work area and
    ; leaves the screen alone, so the sign-off survives into BASIC.
    ; $E003, the warm start, does not: nothing has ever set that work
    ; area up on a machine that booted straight to this disk, so it
    ; lands in the monitor dumping registers.
    ;
    ; NOTE FOR ANYONE CHANGING THIS: BootDiskTests exercises the exit on
    ; both answers through a real //e power-on -- the processor is left
    ; on its own reset vector and the ROM boots the disk itself -- so
    ; the firmware hooks and the soft-entry vector at $03F2 are the ones
    ; a real machine has. The older harness jumped straight into the
    ; boot PROM at $C600 and reproduced neither.
    JMP BASIC


; ----------------------------------------------------------------------------
; clear_text_both -- wipe text page 1 in BOTH banks. 80STORE must be off:
;                    with it on, writes to $0400-$07FF route by PAGE2 and
;                    ignore RAMWRT entirely.
; ----------------------------------------------------------------------------
CLEAR_TEXT_BOTH
    STA RAMWRTON
    JSR CLEAR_TEXT_PAGE1
    STA RAMWRTOFF
    JMP CLEAR_TEXT_PAGE1   ; tail call: its rts returns to our caller


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
; mode_dhgr -- step 0: the chosen pair's DHGR cassowary.
;
;             The monochrome one is already in the framebuffer. The boot
;             path staged it there before the color halves landed on the
;             staging, and no step of the monochrome cycle writes
;             $2000-$3FFF, so coming back to it is the display switches
;             alone.
;
;             The color one is re-staged out of main $6000/$8000 every
;             time, and 80STORE MUST GO OFF FIRST. stage_dhgr's second
;             copy reaches aux $2000 by turning RAMWRT on, and that only
;             works while 80STORE is off -- with 80STORE on and HIRES
;             on, the //e routes $2000-$3FFF by PAGE2 and ignores RAMWRT
;             entirely, landing the aux half in main $2000 on top of the
;             main half. enter_dhgr turns 80STORE back on at the end.
; ----------------------------------------------------------------------------
MODE_DHGR
    LDA PAIR
    BEQ MODE_DHGR_MONO

    STA STORE80OFF
    JSR ENTER_DHGR         ; stage_dhgr + show_dhgr
    JMP POLL

MODE_DHGR_MONO
    JSR SHOW_DHGR
    JMP POLL


; ----------------------------------------------------------------------------
; mode_hgr -- step 1: the chosen pair's HGR cassowary. Either way the
;            DHGR switches come off first, so the renderer doesn't
;            briefly try to draw HGR-as-DHGR.
;
;            The monochrome one lives on HGR page 2, where the loader
;            put it and nothing has touched it since, so it needs no
;            copy at all -- selecting PAGE2 puts it on screen. The
;            color one is stashed at main $A000 and has to be copied
;            back to page 1, because DHGR has clobbered page 1.
; ----------------------------------------------------------------------------
MODE_HGR
    STA STORE80OFF
    STA COL80OFF
    BIT DHIRESOFF

    LDA PAIR
    BEQ MODE_HGR_MONO

    LDX #$A0
    LDY #$20
    JSR COPY_BLOCK

    BIT TXTOFF
    BIT HIRESON
    BIT MIXOFF
    BIT PG2OFF
    JMP POLL

MODE_HGR_MONO
    BIT TXTOFF
    BIT HIRESON
    BIT MIXOFF
    BIT PG2ON
    JMP POLL


; ----------------------------------------------------------------------------
; mode_sweep -- step 2: the hi-res color-mask sweep. It is this Applesoft
;              one-liner, copied off a Beagle Bros. chart:
;
;                HGR : FOR J=0 TO 255 : POKE 228,J : HPLOT 0,0 :
;                      CALL -3082 : NEXT
;
;              HCOLOR= can only reach the eight masks in the ROM table at
;              $F6F6; poking 228 reaches all 256, so the sweep walks the
;              hi-res renderer's entire input space -- every 7-bit dot
;              pattern against both palette states -- as stable,
;              full-screen, uniform fields. Runs until a key.
;
;              PAINTED HERE RATHER THAN BY CALLING THE ROM. Driving the
;              real HPLOT/BKGND pair in a loop worked headlessly and
;              failed on the machine after tens of seconds of sweeping,
;              in a way that did not reproduce in the test harness and
;              that neither the stack pointer nor the demo's own state
;              explained. Painting the page directly is a dozen or so
;              bytes more, keeps the demo free of firmware entry points
;              while it is running, and polls the keyboard three times
;              as often as a ROM fill did.
;
;              WHAT BKGND ACTUALLY PAINTS, reproduced below: the working
;              color into every byte of the page, advancing the color
;              phase after each one. For a mask whose pixel bits land in
;              $20-$5F that means odd bytes carry mask EOR $7F and even
;              bytes carry the mask; every other mask paints uniformly.
;              UnitTest's ExpectedFillByte models the same rule against
;              the real routine.
; ----------------------------------------------------------------------------
MODE_SWEEP
    ; Hi-res page 1, full screen. Step 1 may have left PAGE2 selected --
    ; the monochrome HGR image lives there -- so PAGE1 is not optional.
    STA STORE80OFF
    STA COL80OFF
    BIT DHIRESOFF
    BIT TXTOFF
    BIT HIRESON
    BIT MIXOFF
    BIT PG2OFF

    LDA #0
    STA MASK

SWEEP_PAINT
    ; The two bytes this mask paints with.
    LDA MASK
    STA ODDMASK
    AND #$7F
    CMP #$20
    BCC SWEEP_FILL
    CMP #$60
    BCS SWEEP_FILL
    LDA MASK
    EOR #$7F
    STA ODDMASK

SWEEP_FILL
    LDA #0
    STA DST_PTR
    LDA #$20               ; hi-res page 1
    STA DST_PTR+1
    LDX #32                ; 32 pages = 8 KB
SWEEP_PAGE
    LDY #0
SWEEP_BYTE
    LDA MASK
    STA (DST_PTR),Y
    iny
    LDA ODDMASK
    STA (DST_PTR),Y
    iny
    BNE SWEEP_BYTE
    INC DST_PTR+1
    dex
    BNE SWEEP_PAGE

    LDA KBD
    BPL SWEEP_NEXT
    JMP POLL               ; a key ends the sweep; poll takes it from here
SWEEP_NEXT
    INC MASK
    JMP SWEEP_PAINT


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
BYE_LINE
    ASC 'THANKS FOR WATCHING THE CASSO DEMO.'
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

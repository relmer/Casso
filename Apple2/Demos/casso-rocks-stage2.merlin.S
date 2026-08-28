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
;    Init phase (fast path to first frame):
;      1. Loads tracks 4+5 into main $8000-$9FFF (DHGR main pattern,
;         the second half of the DHGR framebuffer; stage 1 already
;         loaded the aux half to $6000 before jumping here).
;      2. Calls enter_dhgr to copy main $8000 -> main $2000 and main
;         $6000 -> aux $2000 (RAMWRT-toggled), then enables the //e
;         DHGR display switches. THE CASSOWARY IS NOW VISIBLE ? total
;         time from boot is roughly 5 disk tracks (~1.25s).
;
;    Background load (continues after first frame):
;      3. Loads tracks 6+7 and 8+9 back into the two staging areas the
;         init phase has just finished with, main $6000-$7FFF and main
;         $8000-$9FFF. They now hold the MONOCHROME cassowary's aux and
;         main halves, so mode 1 is a second enter_dhgr call over the
;         same addresses and costs no memory of its own.
;      4. Loads tracks 10+11 into main $A000-$BFFF (HGR1 cassowary,
;         loaded directly to its stash location -- mode_hgr1 just
;         memcpys $A000 -> $2000 on demand).
;      5. Loads tracks 12+13 into main $4000-$5FFF (HGR2 bands, at
;         their final destination).
;
;      Because the staging areas are reused, mode 0 is the one mode
;      that cannot be re-entered: by the time the user can press a
;      key, the color halves have been overwritten. Cycling is one
;      way, as it always was.
;
;    Polling loop:
;      Each keystroke advances the mode by 1. ESC ($9B) exits to
;      BASIC at any time. Past the last mode (LoRes), next keystroke
;      also exits. The background loads run in mode order, so each
;      mode's data lands well before a user cycling one key at a time
;      can reach it: mode 1 about a second after first frame, the
;      last mode about two.
;
;      Modes:
;        0 = DHGR: 16-color cassowary on aux+main $2000-$3FFF
;            (entered automatically at boot).
;        1 = DHGR: the same photo authored for a monochrome monitor --
;            a 560x192 one-bit dither of its luminance instead of 140
;            color cells. Which of the two reads as a photograph
;            depends entirely on the monitor, and nothing on this disk
;            can tell which is attached, so each image carries a
;            caption naming the monitor it was drawn for. Both
;            captions are drawn on the color-cell grid, which is what
;            keeps them legible through either decode. See
;            scripts/DhgrCassowaryGen.py.
;        2 = HGR1: 6-color cassowary, restored from main $A000.
;        3 = HGR2: 6-color test bands on main page 2 ($4000-$5FFF).
;        4 = LoRes (GR): 16-color bars on text page 1 ($0400-$07FF).
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
MODE       EQU $46            ; current display mode (0..3)
SRC_PTR    EQU $48            ; ($48),y indirect-indexed source pointer
DST_PTR    EQU $4A            ; ($4A),y indirect-indexed dest pointer


START
    ; --- Init: load DHGR main pattern into main $8000-$9FFF (staged) ------
    ; Stage 1 already loaded the DHGR aux pattern to main $6000 and the
    ; head is at track 3. Two more disk reads (tracks 4+5) and we have
    ; everything we need for first-frame DHGR.
    LDA #$80
    JSR S1_LOAD_B          ; tracks 3 -> 4 (odd->even)
    LDA #$90
    JSR S1_LOAD_A          ; tracks 4 -> 5 (even->odd)

    ; --- Init: enter DHGR (mode 0 ? visible NOW) --------------------------
    ; Total time to first frame: ~5 disk tracks at ~250ms each =
    ; roughly 1.25s. With the original ordering (load HGR1, HGR2,
    ; DHGR-aux, DHGR-main, then enter_dhgr) it was ~9 tracks (~2.25s).
    LDA #0
    STA MODE
    JSR ENTER_DHGR

    ; enter_dhgr -> copy_block clobbers X, but stage 1's load_a/load_b
    ; need X=$60 (slot 6 << 4) for the indexed soft-switch reads
    ; into the disk controller. Restore it before resuming disk I/O.
    LDX #$60

    ; --- Background: load the monochrome cassowary over the staging ------
    ; enter_dhgr has already copied both color halves out of $6000 and
    ; $8000 into the framebuffer, so those 16 KB are free. The mono
    ; halves go straight back into them, which makes mode 1 nothing but
    ; a second enter_dhgr call. Loading in mode order also means the
    ; data for the next mode is always the next thing to arrive.
    LDA #$60
    JSR S1_LOAD_B          ; tracks 5 -> 6
    LDA #$70
    JSR S1_LOAD_A          ; tracks 6 -> 7
    LDA #$80
    JSR S1_LOAD_B          ; tracks 7 -> 8
    LDA #$90
    JSR S1_LOAD_A          ; tracks 8 -> 9

    ; --- Background: load HGR1 cassowary directly to its stash location ---
    ; Mode 2 (HGR1) restores cassowary by copying $A000 -> $2000, so we
    ; can land the disk read straight there and skip a memcpy. This
    ; happens AFTER first frame so the user sees the DHGR cassowary
    ; while we keep loading.
    LDA #$A0
    JSR S1_LOAD_B          ; tracks 9 -> 10
    LDA #$B0
    JSR S1_LOAD_A          ; tracks 10 -> 11

    ; --- Background: load HGR2 bands at their final destination $4000 -----
    LDA #$40
    JSR S1_LOAD_B          ; tracks 11 -> 12
    LDA #$50
    JSR S1_LOAD_A          ; tracks 12 -> 13

POLL
    LDA KBD
    BPL POLL
    CMP #$9B               ; ESC?
    BEQ DO_EXIT
    BIT STROBE

    INC MODE
    LDA MODE
    CMP #1
    BEQ MODE_DHGR_MONO
    CMP #2
    BEQ MODE_HGR1
    CMP #3
    BEQ MODE_HGR2
    CMP #4
    BEQ MODE_LORES
    ; fall through (past mode 4) -> exit


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
; mode_dhgr_mono -- show the monochrome cassowary. The background load
;                  has replaced the color halves in the staging areas
;                  with the mono ones, so this is enter_dhgr over the
;                  exact same addresses. The display switches are
;                  already set from mode 0; re-issuing them is harmless.
;
;                  80STORE MUST GO OFF FIRST. enter_dhgr's second copy
;                  reaches aux $2000 by turning RAMWRT on, and that only
;                  works while 80STORE is off -- with 80STORE on and
;                  HIRES on, the //e routes $2000-$3FFF by PAGE2 and
;                  ignores RAMWRT entirely. The boot call gets away
;                  without this because it runs before enter_dhgr has
;                  ever set 80STORE; the second call does not, and lands
;                  the aux half in main $2000 on top of the main half.
;                  enter_dhgr turns 80STORE back on at the end.
; ----------------------------------------------------------------------------
MODE_DHGR_MONO
    STA STORE80OFF
    JSR ENTER_DHGR
    JMP POLL


; ----------------------------------------------------------------------------
; mode_hgr1 ? restore the HGR1 cassowary. Copies the stashed cassowary
;             from main $A000 back to main $2000 (DHGR may have clobbered
;             it), turns off DHGR, and selects HGR PAGE1.
; ----------------------------------------------------------------------------
MODE_HGR1
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


MODE_HGR2
    ; HGR + page 2 visible. HIRES is already on from mode 1; just
    ; flip PAGE2.
    BIT PG2ON
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
    ; Copy main $8000-$9FFF -> main $2000-$3FFF (clobbers cassowary,
    ; which is why we stashed a copy at $A000 during init).
    LDX #$80
    LDY #$20
    JSR COPY_BLOCK

    ; Copy main $6000-$7FFF -> aux $2000-$3FFF.
    STA RAMWRTON
    LDX #$60
    LDY #$20
    JSR COPY_BLOCK
    STA RAMWRTOFF

    ; Enable DHGR display: 80STORE on, AN3/DHIRES on, 80COL on,
    ; TEXT off, HIRES on, PAGE1.
    STA STORE80ON
    STA DHIRESON
    STA COL80ON
    BIT TXTOFF
    BIT HIRESON
    BIT PG2OFF
    RTS


; ----------------------------------------------------------------------------
; copy_block ? copy 8 KB (32 pages) from main:src to (main or aux):dst.
;              Caller sets X = src high byte, Y = dst high byte, and toggles
;              RAMWRT before/after the call when an aux dest is needed.
;
;              Uses (SRC_PTR),y / (DST_PTR),y so all addressing ? including
;              the page-counter increment ? flows through zp, which under
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

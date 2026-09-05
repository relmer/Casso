; ============================================================================
;  casso-rocks.a65 ? bootable Apple //e demo disk with DHGR cassowary mascot
;
;  Loaded by the //e Disk2.rom slot 6 boot firmware: the boot ROM reads
;  track 0 sector 0 (256 bytes from file offset 0) into RAM at $0800-$08FF
;  and JMPs to $0801. Stage 1 (this file) takes over from there:
;
;    1. Steps the head to track 1 + 2 and reads them into main $6000-$7FFF
;       (the MONOCHROME DHGR aux half, staged in main RAM because RAMWRT-on
;       conflicts with the RWTS scratch buffer in pages $02-$03; stage 2
;       will copy to aux later via the copy_block routine).
;    2. Steps to track 3 and reads all 16 sectors with DBASE=$10 into
;       $1000-$1FFF. Track 3 holds the stage 2 code at sectors 0-1
;       (lands at $1000-$11FF) plus the LoRes test pattern at sectors
;       2-5 (lands at $1200-$15FF).
;    3. JMPs to stage 2 at $1000.
;
;  Stage 2 (assembled and shipped as a separate sector ? see
;  casso-rocks-stage2.a65 alongside this file) then:
;
;    4. Puts the monitor question on the text screen, because nothing
;       can detect the monitor -- no Apple II can, every video connector
;       it has is output only. TEXT reads equally well on either kind,
;       so the question is safe to show before the answer is known.
;    5. Loads tracks 4+5 -> main $8000-$9FFF (the monochrome DHGR main
;       half) and copies both halves into the aux+main framebuffer at
;       $2000-$3FFF. Copies only: the display stays on the question.
;       This has to happen before step 6 lands the color halves on the
;       same staging.
;    6. Loads the rest behind the question, in the order the modes will
;       ask for it: tracks 6+7 -> $4000, which IS HGR page 2, so the
;       monochrome HGR cassowary needs no stash and no copy; tracks 8+9
;       and 10+11 -> $6000 and $8000, the staging step 5 has just
;       emptied, which is where the COLOR DHGR halves go; tracks 12+13
;       -> $A000 (the color HGR cassowary, loaded directly to its stash
;       location).
;    7. Waits for C or M and starts the cycle on that pair, then polls
;       the keyboard for cycle / exit keystrokes.
;
;  ALL THIRTEEN TRACKS LOAD BEHIND THE QUESTION, which is why asking
;  costs nothing: the ~2.2 seconds of disk work happen while the user is
;  reading it, and a key pressed during the loads is still latched when
;  the loader gets around to looking. Disk order matches cycle order, so
;  whatever the user reaches next is whatever finished loading last.
;
;  Stage 2 calls back into stage 1's still-resident subroutines
;  (load_a, load_b, read_track, read_sector, chk, wait_d5_aa). The
;  fixed addresses of those entry points are hard-coded in stage 2
;  via the labels exported below, so the two assemblies must move
;  together if either is edited.
;
;  Self-contained while it is RUNNING: loading, decoding and displaying
;  use no firmware entry points at all. The only piece of disk2.rom we
;  re-use is the inverse 6-and-2 nibble decode table the boot PROM
;  builds at $02D6 (= $0356 - $80) during cold boot.
;
;  Handing control BACK is the exception. Quitting calls INIT, SETKBD
;  and SETVID and then Applesoft's cold start, because going through
;  the reset vector lets the //e RESET.MGR re-run the video firmware
;  init and turn 80COL back on, which drops the user into Applesoft in
;  80 columns. See do_exit in casso-rocks-stage2.a65.
;
;  Tested by BootDiskTests::CassoRocks_DemoDisk_DisplaysHgrCassowary.
; ============================================================================

        ORG $0801

; -- I/O & soft switches ----------------------------------------------------
DATA     EQU $C08C            ; Q6L (slot 6 indexed via X=$60)
NIBL     EQU $02D6            ; disk2.rom decode table[raw_nibble]

; -- Zero page scratch ------------------------------------------------------
DLO      EQU $26              ; ($26),Y dest pointer lo (always 0)
DHI      EQU $27              ; ($26),Y dest pointer hi (page)
SECW     EQU $3C              ; saved Y across read loops
DBASE    EQU $3D              ; per-track DHI base ($20, $30, $40, $50, $09)
COUNT    EQU $40              ; sectors-remaining-on-track counter
TMP      EQU $41              ; chk + expand scratch


START
    ; The disk2.rom boot PROM hands us X = $60 (slot 6 << 4) via its
    ; final LDX $2B before JMP $0801, and we never touch $2B, so X
    ; stays valid for the indexed soft-switch reads below.
    LDA #0
    STA DLO

    ; Step to track 1 + 2, read DHGR aux pattern into main $6000-$7FFF.
    ; This is the FIRST half of the DHGR cassowary; loading it first
    ; lets stage 2 enter DHGR mode after only 2 more tracks (track 3
    ; for stage 2 itself + tracks 4+5 for the DHGR main half) instead
    ; of all 9 ? cuts first-frame latency roughly in half.
    LDA #$60
    JSR LOAD_A
    LDA #$70
    JSR LOAD_B

    ; Step to track 3, read it into $1000-$1FFF. Every sector of track
    ; 3 contains identical stage 2 code, so the page at $1000 is
    ; guaranteed to be the stage 2 entry point regardless of which
    ; physical sector the head landed on first. DBASE must have its
    ; low nibble = 0 because read_sector merges DHI = DBASE | sector
    ; via OR (cheaper than ADD); $10 satisfies this and falls in
    ; otherwise-unused main RAM.
    LDA #$10
    JSR LOAD_A

    ; Hand control to stage 2 at $1000.
    JMP $1000


; ----------------------------------------------------------------------------
; load_a / load_b -- step the head one track and read all 16 sectors of the
;                    new track into RAM at the page given by A on entry.
;                    load_a advances 0->1 / 2->3 (even-to-odd track),
;                    load_b advances 1->2 / 3->4 (odd-to-even). Both fall
;                    through into read_track via tail-call (jmp).
; ----------------------------------------------------------------------------
LOAD_A
    STA DBASE
    LDA $C083,X            ; phase 1 ON
    LDA $C080,X            ; phase 0 OFF
    LDA $C085,X            ; phase 2 ON
    LDA $C082,X            ; phase 1 OFF
    JMP READ_TRACK

LOAD_B
    STA DBASE
    LDA $C087,X            ; phase 3 ON
    LDA $C084,X            ; phase 2 OFF
    LDA $C081,X            ; phase 0 ON
    LDA $C086,X            ; phase 3 OFF
    JMP READ_TRACK


; ----------------------------------------------------------------------------
; chk -- wait for the next valid disk nibble in A and compare it against the
;        byte already in TMP (which the caller stored). Returns Z=1 on match.
; ----------------------------------------------------------------------------
CHK
    STA TMP
CHK_W
    LDA DATA,X
    BPL CHK_W
    CMP TMP
    RTS


; ----------------------------------------------------------------------------
; read_track -- read 16 sectors in whatever physical order they arrive on
;               the current track. Each sector's address mark identifies
;               the logical sector number, which becomes the destination
;               page offset (DHI = DBASE + sector). Caller sets DBASE.
; ----------------------------------------------------------------------------
READ_TRACK
    ; Read more than 16 sectors so we tolerate the odd missed sector
    ; that can happen when the LSS hasn't fully resynced after a head
    ; step (read_sector spins forever on garbage nibbles until it
    ; finds a real D5/AA/96 mark, but the FIRST sector it picks up
    ; can occasionally be a corrupted/phantom address mark that
    ; consumes one slot in the loop without storing valid data).
    ; Reading 18 ensures every logical sector comes around at least
    ; once even if 1-2 slots are wasted on phantoms; duplicates just
    ; overwrite the same destination page with identical bytes.
    LDA #18
    STA COUNT
RT_LP
    JSR READ_SECTOR
    DEC COUNT
    BNE RT_LP
    RTS


; ----------------------------------------------------------------------------
; wait_d5_aa -- spin until the next disk nibble is $D5 then the next is $AA.
;               Caller does the third-nibble compare directly. Used by both
;               the address-mark prologue ($D5 $AA $96) and the data
;               prologue ($D5 $AA $AD), so the only divergent byte stays
;               at the call site and the shared "find $D5 $AA" lives once.
; ----------------------------------------------------------------------------
WAIT_D5_AA
    LDA #$D5
    JSR CHK
    BNE WAIT_D5_AA
    LDA #$AA
    JSR CHK
    BNE WAIT_D5_AA
    RTS


; ----------------------------------------------------------------------------
; read_sector -- spin until the next D5 AA 96 address-prologue, parse the
;                logical sector number out of the 4-and-4 address field,
;                set DHI = DBASE + sector, then spin until the matching
;                D5 AA AD data prologue and decode the data field into
;                256 bytes at ($26),Y for Y=0..255.
; ----------------------------------------------------------------------------
READ_SECTOR
SC_D5
    JSR WAIT_D5_AA
    LDA #$96
    JSR CHK
    BNE SC_D5

    ; address field: skip 4 nibbles (volume H, volume L, track H, track L)
    LDY #4
SK1
    LDA DATA,X
    BPL SK1
    DEY
    BNE SK1

    ; read sector H/L (4-and-4 encoded), decode = ((H<<1)|1) & L
RSH
    LDA DATA,X
    BPL RSH
    SEC
    ROL A
    STA TMP
RSL
    LDA DATA,X
    BPL RSL
    AND TMP

    ; A = sector number 0..15. DBASE is $20, $30, $40, or $50 (low nibble
    ; = 0), so OR-merging is equivalent to ADD here and one byte cheaper.
    ORA DBASE
    STA DHI

    ; skip 2 checksum nibbles (Y-loop saves bytes vs two unrolled waits)
    LDY #2
SK2
    LDA DATA,X
    BPL SK2
    DEY
    BNE SK2

    ; wait for D5 AA AD data prologue
DD5
    JSR WAIT_D5_AA
    LDA #$AD
    JSR CHK
    BNE DD5

    ; read 86 secondary nibbles into $0300-$0355 in forward order, A =
    ; running checksum (XOR). Inlined rather than going through a
    ; subroutine because the round-trip JSR/RTS push us over the ~32-
    ; cycle disk-nibble window per iteration and the LSS races ahead,
    ; making us miss every other nibble. The base trick (sta $0256,y,
    ; Y starting at $AA) lets the loop exit naturally when Y wraps.
    LDA #0
    LDY #$AA
SEC_LP
    STY SECW
SEC_W
    LDY DATA,X
    BPL SEC_W
    EOR NIBL,Y
    LDY SECW
    STA $0256,Y
    INY
    BNE SEC_LP

    ; read 256 primary nibbles into ($26),Y (Y wraps), A = running checksum
PRI_LP
    STY SECW
PRI_W
    LDY DATA,X
    BPL PRI_W
    EOR NIBL,Y
    LDY SECW
    STA ($26),Y
    INY
    BNE PRI_LP

    ; read final checksum nibble; we don't verify (best-effort, deterministic
    ; nibble stream in the test harness)
CHK_W2
    LDA DATA,X
    BPL CHK_W2

    ; --- Expand: combine 86 secondary + 256 primary into 256 8-bit bytes ---
    ; primary[i] currently holds the 6 high bits in low position (bits 5..0).
    ; Secondary alias bases let one loop body cover all three groups:
    ;     i =   0..85   -> lda $0300,y (bits 1..0 of secondary[i])
    ;     i =  86..171  -> lda $02AA,y (bits 1..0 after >>2)
    ;     i = 172..255  -> lda $0254,y (bits 1..0 after >>4)
    ; The final iny/bne wraps Y from $FF -> $00 to terminate.
    LDY #0
EXP_LP
    CPY #172
    BLT EXP_L172
    LDA $0254,Y
    LSR A
    LSR A
    LSR A
    LSR A
    JMP EXP_JOIN
EXP_L172
    CPY #86
    BLT EXP_L86
    LDA $02AA,Y
    LSR A
    LSR A
    JMP EXP_JOIN
EXP_L86
    LDA $0300,Y
EXP_JOIN
    AND #$03               ; A = raw 2-bit pair (b1 b0) for primary[Y]
    STA TMP                ; reverse the pair inline: output = (b0 b1)
    ASL A
    AND #$02               ; bit 1 of result = original bit 0
    LSR TMP                ; carry = original bit 0; TMP bit 0 = original bit 1
    ORA TMP                ; A = reversed 2-bit pair in low position
    STA TMP
    LDA ($26),Y
    ASL A
    ASL A
    ORA TMP
    STA ($26),Y
    INY
    BNE EXP_LP
    RTS


; --- end ---

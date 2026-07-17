#pragma once

#include "Pch.h"




////////////////////////////////////////////////////////////////////////////////
//
//  ParallelFirmware
//
//  Original slot firmware for the parallel printer card, generated from
//  Devices/Printer/ParallelFirmware.a65. This header embeds BOTH the assembled
//  bytes and the exact source text; FirmwareParityTests re-assembles the
//  source with the in-repo assembler and asserts equality with the byte
//  array, so the two can never silently drift (FR-003 provenance).
//
//  The card installs s_kParallelFirmwareBytes at $Cn00 for its configured
//  slot and pads the rest of the page. s_kParallelFirmwareOrigin is the fixed
//  assembly origin (default slot 1); the routine itself is slot-independent.
//
//  To regenerate after editing the .a65: assemble it with CassoCli, extract
//  the bytes at the $C100 page, and update both arrays below.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Word   s_kParallelFirmwareOrigin = 0xC100;

static constexpr Byte   s_kParallelFirmwareBytes[] =
{
    0x4C, 0x0D, 0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x48, 0x98, 0x48, 0x20, 0x13, 0xC1, 0x68, 0x68, 0x29, 0x0F, 0x0A,
    0x0A, 0x0A, 0x0A, 0xAA, 0xBD, 0x81, 0xC0, 0x10, 0xFB, 0x68, 0xA8, 0x68,
    0x9D, 0x80, 0xC0, 0xC9, 0x8D, 0xF0, 0x05, 0xC9, 0x0D, 0xF0, 0x01, 0x60,
    0x48, 0xBD, 0x81, 0xC0, 0x10, 0xFB, 0xA9, 0x0A, 0x9D, 0x80, 0xC0, 0x68,
    0x60,
};

static constexpr const char *   s_kParallelFirmwareSource = R"ASM(; ============================================================================
;  ParallelFirmware.a65 -- original slot firmware for the Casso parallel
;  printer interface card. Assembled by the in-repo CassoCore assembler; the
;  byte output is embedded in ParallelFirmware.h and checked for parity by
;  FirmwareParityTests (source <-> bytes can never drift).
;
;  A DUMB, output-only parallel card, exactly like the classic Apple Parallel
;  Interface Card and the Grappler in transparent mode. It deliberately does
;  NOT advertise the Pascal 1.1 "intelligent firmware" signature ($Cn05=$38 /
;  $Cn07=$18 / $Cn0B=$01): a printer is output-only, and claiming that protocol
;  makes the Apple IIe hook the card for INPUT too on PR#n -- its input path
;  then reads this stub's (unsupported) read entry, gets garbage, and BASIC
;  echoes it forever (a flood of stray characters into the printer). With no
;  signature, PR#n simply points CSW at $Cn00 and every COUT character flows
;  through OUTPUT, which is all a printer needs.
;
;  Layout of the slot ROM page ($Cn00-$CnFF; shown for the default slot 1 =
;  $C100). The card installs these bytes at $Cn00 for its configured slot and
;  pads the remainder of the page.
;
;    $Cn00      JMP OUTPUT        -- CSW per-character entry (PR#n output)
;    $Cn03-$Cn0C  zeroed -- NOT the Pascal 1.1 firmware signature (see above)
;    OUTPUT     per-character output routine
;
;  Slot-independent: the output routine discovers its own slot at runtime via
;  the return-address on the stack, so the same bytes work in any slot; only
;  the assembly origin is fixed (for the parity test).
;
;  Print Shop and other direct-hardware drivers ignore this firmware entirely:
;  they drive the card's I/O locations ($C08n data / $C08n+1 status) themselves
;  and probe the status byte, not the ROM -- so the dumb-card firmware serves
;  BASIC / DOS listing (CSW output) while direct drivers are unaffected.
; ============================================================================

ROMBASE = $C100          ; default slot-1 ROM page (assembly origin only)
IODATA  = $C080          ; data latch base; runtime index = slot*16
IOSTAT  = $C081          ; status base;     runtime index = slot*16

        .org ROMBASE

        jmp OUTPUT               ; $Cn00: CSW per-character entry point

        ; $Cn03-$Cn0C: deliberately blank. In particular $Cn05 / $Cn07 / $Cn0B
        ; are NOT $38 / $18 / $01, so the IIe sees no Pascal 1.1 firmware card
        ; and PR#n takes the simple set-CSW-to-$Cn00 path (output only).
        .byte $00, $00, $00, $00, $00, $00, $00, $00, $00, $00

; ----------------------------------------------------------------------------
;  OUTPUT -- per-character output (char in A). Preserves A (returns the
;  character) and Y; uses X as the slot*16 I/O index. Honors the ready bit,
;  so if the card ever de-asserts ready (high-water backpressure) the guest
;  waits here instead of losing a byte.
;
;  Like Apple's parallel interface card, the firmware sends a line feed after
;  every carriage return: BASIC / DOS output (PR#n, LIST, CATALOG) emits bare
;  CRs ($8D or $0D) and relies on the interface to advance the paper. Software
;  that must control line feeds itself (Print Shop et al.) drives the card's
;  I/O locations directly and never passes through this routine -- exactly as
;  on the real card.
; ----------------------------------------------------------------------------
OUTPUT:
        pha                      ; save the character
        tya
        pha                      ; save Y

        jsr HERE                 ; push return address (high byte = $Cn)
HERE:
        pla                      ; discard return low byte
        pla                      ; A = return high byte = $Cn
        and #$0F                 ; A = slot number
        asl a
        asl a
        asl a
        asl a                    ; A = slot * 16
        tax                      ; X = slot*16 I/O index

WAIT:
        lda IOSTAT,x             ; status: bit 7 set = ready
        bpl WAIT                 ; wait while busy (normally falls straight through)

        pla                      ; A = saved Y
        tay                      ; restore Y
        pla                      ; A = saved character
        sta IODATA,x             ; latch the character to the card
        cmp #$8D                 ; carriage return (COUT sends high-bit ASCII)?
        beq CRLF
        cmp #$0D                 ; or a plain CR
        beq CRLF
        rts

CRLF:
        pha                      ; keep the character to return in A
LFWAIT:
        lda IOSTAT,x             ; honor ready for the injected byte too
        bpl LFWAIT
        lda #$0A
        sta IODATA,x             ; line feed after CR (like Apple's card)
        pla
        rts
)ASM";

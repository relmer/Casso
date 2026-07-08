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
    0x4C, 0x11, 0xC1, 0x00, 0x00, 0x38, 0x00, 0x18, 0x00, 0x00, 0x00, 0x01,
    0x12, 0x2C, 0x2D, 0x2F, 0x32, 0x48, 0x98, 0x48, 0x20, 0x17, 0xC1, 0x68,
    0x68, 0x29, 0x0F, 0x0A, 0x0A, 0x0A, 0x0A, 0xAA, 0xBD, 0x81, 0xC0, 0x10,
    0xFB, 0x68, 0xA8, 0x68, 0x9D, 0x80, 0xC0, 0x60, 0x60, 0x38, 0x60, 0x4C,
    0x11, 0xC1, 0x60,
};

static constexpr const char *   s_kParallelFirmwareSource = R"ASM(; ============================================================================
;  ParallelFirmware.a65 -- original slot firmware for the Casso parallel
;  printer interface card. Assembled by the in-repo CassoCore assembler; the
;  byte output is embedded in ParallelFirmware.h and checked for parity by
;  FirmwareParityTests (source <-> bytes can never drift).
;
;  Layout of the slot ROM page ($Cn00-$CnFF; shown for the default slot 1 =
;  $C100). The card installs these bytes at $Cn00 for its configured slot and
;  pads the remainder of the page.
;
;    $Cn00      JMP OUTPUT        -- CSW per-character entry (PR#n output)
;    $Cn05 $38  \
;    $Cn07 $18   > Pascal 1.1 firmware identification signature
;    $Cn0B $01  /
;    $Cn0C      device class byte (printer)
;    $Cn0D-$Cn10  Pascal 1.1 entry offsets: init / read / write / status
;    OUTPUT     per-character output routine
;
;  Slot-independent: the output routine discovers its own slot at runtime via
;  the return-address on the stack, so the same bytes work in any slot; only
;  the assembly origin is fixed (for the parity test).
;
;  PROVISIONAL VALUES pending the Print Shop byte capture (spec 015 T011):
;  the device-class byte and the exact status ready convention are best
;  guesses from the ImageWriter / parallel-card conventions. Functional
;  behavior (PR#1 + LIST, and Print Shop accepting the card as a valid
;  "Apple II Parallel Interface") is validated at US6 / T011; this file's
;  unit gate only proves the source assembles to the embedded bytes.
; ============================================================================

ROMBASE = $C100          ; default slot-1 ROM page (assembly origin only)
IODATA  = $C080          ; data latch base; runtime index = slot*16
IOSTAT  = $C081          ; status base;     runtime index = slot*16
PCLASS  = $12            ; Pascal device class: printer (provisional)

        .org ROMBASE

        jmp OUTPUT               ; $Cn00: CSW per-character entry point
        .byte $00                ; $Cn03
        .byte $00                ; $Cn04
        .byte $38                ; $Cn05: Pascal 1.1 signature
        .byte $00                ; $Cn06
        .byte $18                ; $Cn07: Pascal 1.1 signature
        .byte $00                ; $Cn08
        .byte $00                ; $Cn09
        .byte $00                ; $Cn0A
        .byte $01                ; $Cn0B: Pascal 1.1 signature
        .byte PCLASS             ; $Cn0C: device class byte
        .byte PINIT - ROMBASE    ; $Cn0D: Pascal init  entry offset
        .byte PREAD - ROMBASE    ; $Cn0E: Pascal read  entry offset
        .byte PWRITE - ROMBASE   ; $Cn0F: Pascal write entry offset
        .byte PSTATUS - ROMBASE  ; $Cn10: Pascal status entry offset

; ----------------------------------------------------------------------------
;  OUTPUT -- per-character output (char in A). Preserves A (returns the
;  character) and Y; uses X as the slot*16 I/O index. Honors the ready bit,
;  so if the card ever de-asserts ready (high-water backpressure) the guest
;  waits here instead of losing a byte.
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
        rts

; ----------------------------------------------------------------------------
;  Pascal 1.1 entry stubs. The flagship path is CSW output (above) and Print
;  Shop's direct slot I/O; these exist so the signature is honest and a
;  Pascal caller does not crash. Full Pascal semantics are out of scope.
; ----------------------------------------------------------------------------
PINIT:
        rts                      ; init: nothing to set up
PREAD:
        sec                      ; read: input not supported -> error
        rts
PWRITE:
        jmp OUTPUT               ; write: identical to CSW output
PSTATUS:
        rts                      ; status: always ready
)ASM";

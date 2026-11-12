#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureStream
//
//  A raw read of one quarter-track position, as the capture module stores
//  it: each disk byte in order, with its top bit clear when zero bits
//  followed it, as they follow a sync byte. The read is a little over two
//  revolutions, so the revolution is where the bytes start to repeat.
//
//  One revolution is written as bits for a WOZ track: each byte's eight
//  bits, and two zero bits after a byte both revolutions mark as followed by
//  zero bits. The mark is read once a byte, at a moment that varies by up
//  to a poll's length, so a byte is now and then marked when it isn't one;
//  both revolutions marking it rules that out.
//
////////////////////////////////////////////////////////////////////////////////

class CaptureStream
{
public:
    //  The shortest revolution: a 16-sector track all of sync bytes, at
    //  40 cycles each, is about 5,100 bytes.
    static constexpr size_t  kMinRevolutionBytes = 4000;
    static constexpr size_t  kMatchBytes         = 512;

    //  The bytes of a revolution: the first length from kMinRevolutionBytes
    //  at which kMatchBytes bytes repeat, with every byte after them that
    //  overlaps the next revolution the same but for one in a hundred. False
    //  for a read that doesn't repeat, as noise from an unformatted track
    //  doesn't.
    static bool  FindRevolution  (std::span<const Byte> bytes, size_t & outLength);

    //  One revolution of `length` bytes as bits, one to a byte, most
    //  significant first.
    static void  BuildRevolution (std::span<const Byte> bytes, size_t length, std::vector<uint8_t> & outBits);

    //  The bits of the byte at t of a revolution of length bytes.
    static void  AppendByte      (std::span<const Byte> bytes, size_t at, size_t length, std::vector<uint8_t> & ioBits);

    //  Whether a byte as stored was followed by zero bits.
    static bool  IsSync (Byte stored) { return (stored & s_kTopBit) == 0; }

    //  The capture module's encoding of `count` stored bytes (Capture.s): the
    //  revolution's length, low byte first, then each byte as itself but 0,
    //  which begins a token: 0 for a 0, 1 to 127 for a copy of 3 to 129 bytes
    //  a revolution earlier, $80 and up for a repeat of 3 to 130 of the next
    //  byte. Bytes past the last are padding. False when the tokens decode to
    //  more or fewer bytes, or copy from before the first.
    static bool  Decode (std::span<const Byte> encoded, size_t count, std::vector<Byte> & outBytes);

private:
    static constexpr Byte    s_kTopBit        = 0x80;
    static constexpr size_t  s_kMismatchShare = 100;
    static constexpr Byte    s_kEscape        = 0x00;
    static constexpr Byte    s_kRepeatToken   = 0x80;   // and up: a repeat; below, a copy
    static constexpr Byte    s_kCountMask     = 0x7F;
    static constexpr size_t  s_kCopyBase      = 2;
    static constexpr size_t  s_kRepeatBase    = 3;
};

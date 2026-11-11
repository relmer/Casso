#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkPacket
//
//  One framed packet: its command byte and its payload. For Send Image Data
//  the payload is the two-byte starting block followed by the decoded data,
//  so the page encoding and the CRC are handled inside the codec alone.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkPacket
{
    Byte               command = 0;
    std::vector<Byte>  payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkCodec
//
//  CassoLink's framing, ADTPro's and Casso's alike:
//
//      $C1, payload length (LSB first), command, EOR of those four bytes,
//      payload, EOR of the payload (absent when the payload is empty)
//
//  Send Image Data ($D3) differs: its length field is the data's length
//  after decoding; its payload is a two-byte starting block and the data
//  encoded page by page; and a CRC-16/XMODEM of the decoded data, LSB first,
//  takes the place of the payload EOR.
//
//  The page encoding writes each byte as its difference from the previous
//  decoded byte, starting from 0 in each packet. A repeat of the previous
//  byte is written as 0 and an offset within the current 256-byte page, the
//  repeat running up to that offset; 0 for the offset is the end of the page.
//  A repeat cannot cross a page, and a repeat of one byte cannot be written
//  any other way, so the encoding of given data has one form.
//
//  Sector data ($CA), Casso's own, is ordinary framing: track, sector, the
//  256 bytes unencoded and their CRC-16/XMODEM, LSB first.
//
//  The reply to Read sectors ($D5) is a Sector data packet for each sector
//  read, in the order of the request, then a Status. A sector the Apple
//  didn't find has no packet, and the Status error is NOT FOUND. Each
//  packet's sector indicates which sectors arrived.
//
////////////////////////////////////////////////////////////////////////////////

class CassoLinkCodec
{
public:
    static constexpr Byte  kStart        = 0xC1;

    // ADTPro's commands.
    static constexpr Byte  kCd           = 0xC3;
    static constexpr Byte  kDirectory    = 0xC4;
    static constexpr Byte  kGet          = 0xC7;
    static constexpr Byte  kPut          = 0xD0;
    static constexpr Byte  kBatch        = 0xC2;
    static constexpr Byte  kNibble       = 0xCE;
    static constexpr Byte  kMultiple     = 0xCD;
    static constexpr Byte  kSize         = 0xDA;
    static constexpr Byte  kImageData    = 0xD3;
    static constexpr Byte  kAcknowledge  = 0xCB;
    static constexpr Byte  kSummary      = 0xD9;
    static constexpr Byte  kHome         = 0xD8;
    static constexpr Byte  kSendFile     = 0xC6;
    static constexpr Byte  kVirtualDrive = 0xC5;

    // Casso's commands.
    static constexpr Byte  kReady         = 0xD2;
    static constexpr Byte  kIdle          = 0xC9;
    static constexpr Byte  kSectorData    = 0xCA;
    static constexpr Byte  kSectorNibbles = 0xCC;
    static constexpr Byte  kLoadProgram   = 0xCF;
    static constexpr Byte  kSetSpeed      = 0xD7;
    static constexpr Byte  kReadSectors   = 0xD5;
    static constexpr Byte  kWriteSectors  = 0xD6;
    static constexpr Byte  kFormatTrack   = 0xC8;
    static constexpr Byte  kWriteTrack    = 0xDB;
    static constexpr Byte  kStatus        = 0xD4;
    static constexpr Byte  kCancel        = 0xD1;
    static constexpr Byte  kLabel         = 0xDC;
    static constexpr Byte  kLog           = 0xDD;
    static constexpr Byte  kTimeDrive     = 0xDE;
    static constexpr Byte  kExit          = 0xDF;

    //  Write track's sector of zeros, as its track and sector alone, which the
    //  Apple fills in.
    static constexpr Byte  kSectorBlank   = 0xE0;

    //  A Full Send's screen, and its pause for Space or Q. Key has the same
    //  command byte as Wait for key, and one byte: the key's code, kKeyQuit also
    //  unprompted during a Full Send, kKeyFullSend unprompted on the standard
    //  screen, and kKeyNone once a second in a pause. Wait for key's first byte
    //  is kWaitTyped while a line is typed, when every key is sent, or
    //  kWaitKeys for the session's keys alone; its lines follow it.
    static constexpr Byte  kSessionScreen = 0xE1;
    static constexpr Byte  kWaitForKey    = 0xE2;
    static constexpr Byte  kSetColumns    = 0xE3;
    static constexpr Byte  kReadCrcs      = 0xE4;
    static constexpr Byte  kSectorCrcs    = 0xE5;
    static constexpr Byte  kFault         = 0xE6;     // a Debug build's program alone
    static constexpr Byte  kKeySpace      = 0xA0;
    static constexpr Byte  kKeyQuit       = 0xD1;
    static constexpr Byte  kKeyNone       = 0x00;
    static constexpr Byte  kKeyFullSend   = 0xC6;
    static constexpr Byte  kKeyDrive1     = 0xB1;
    static constexpr Byte  kKeyDrive2     = 0xB2;
    static constexpr Byte  kKeyVerify     = 0xD6;
    static constexpr Byte  kKeyPrefix     = 0xD0;
    static constexpr Byte  kKeyReturn     = 0x8D;
    static constexpr Byte  kKeyEscape     = 0x9B;
    static constexpr Byte  kKeyLeft       = 0x88;
    static constexpr Byte  kKeyDelete     = 0xFF;
    static constexpr Byte  kWaitKeys      = 0x00;
    static constexpr Byte  kWaitTyped     = 0x01;

    //  Krunch: the drive, then the CRC-16/XMODEM of the Apple Pascal directory
    //  the host checked, low byte first. Krunch progress in reply has the
    //  percentage done and the files moved.
    static constexpr Byte  kKrunch         = 0xE7;
    static constexpr Byte  kKrunchProgress = 0xE8;

    //  Load module: with no payload, the module loaded's id and version in
    //  reply, 0 and 0 for none; with an offset into the module and bytes,
    //  an Acknowledge with the offset; with $FFFF alone, the module checked,
    //  and its id and version in reply, 0 and 0 when it fails.
    static constexpr Byte    kLoadModule      = 0xE9;
    static constexpr Word    kModuleCheck     = 0xFFFF;
    static constexpr size_t  kModuleChunk     = 240;

    //  The capture module's request: the percentages at its start and end,
    //  the drive and the quarter-track position. Its reply is each page of
    //  the read as Capture data, the page's index, its 256 bytes and their
    //  CRC-16/XMODEM, low byte first; then a Status.
    static constexpr Byte    kCapture         = 0xEA;
    static constexpr Byte    kCaptureData     = 0xEB;
    static constexpr size_t  kCapturePages    = 56;
    static constexpr int     kQuarterTracks   = 140;

    //  The longest row on the Session screen.
    static constexpr size_t  kSessionRowLength = 80;

    //  Time drive's marker, one a revolution, outside any packet.
    static constexpr Byte  kTimeMark     = 0xA5;

    //  The longest Label text the program draws.
    static constexpr size_t  kLabelLength = 38;

    //  The longest Log text the program draws, after the time.
    static constexpr size_t  kLogLength   = 34;

    // Acknowledgement kinds.
    static constexpr Byte  kAck          = 0x06;
    static constexpr Byte  kNak          = 0x15;
    static constexpr Byte  kCan          = 0x18;
    static constexpr Byte  kEnq          = 0x05;

    static constexpr size_t  kPageSize   = 256;
    static constexpr size_t  kSectorSize = 256;

    static HRESULT   Encode            (Byte command, std::span<const Byte> payload, std::vector<Byte> & out);
    static HRESULT   EncodeImageData   (Word startBlock, std::span<const Byte> data, std::vector<Byte> & out);
    static HRESULT   EncodeSectorData  (Byte track, Byte sector, std::span<const Byte> data, std::vector<Byte> & out);
    static HRESULT   EncodeSectorBlank (Byte track, Byte sector, std::vector<Byte> & out);
    static HRESULT   EncodeAcknowledge (Byte kind, Word block, std::vector<Byte> & out);

    // One packet from the front of `in`: with outComplete set, the packet and
    // the bytes it took; with it clear, nothing, as `in` ends inside the
    // packet. ERROR_INVALID_DATA when the packet is damaged, with the bytes to
    // skip -- one when the envelope is bad, so the caller looks for the next
    // start, and the whole packet when only its payload is.
    static HRESULT   Decode            (std::span<const Byte> in, size_t & outConsumed, CassoLinkPacket & outPacket, bool & outComplete);

    // The track, sector and 256 bytes of a Sector data packet, once its CRC
    // is checked.
    static HRESULT   ParseSectorData   (const CassoLinkPacket & packet, Byte & outTrack, Byte & outSector, std::span<Byte> outData);

    // Sector nibbles: the track, the sector, its data field's 343 nibbles as
    // NibblizationLayer encodes them, and their CRC. Write sectors sends
    // these, so the Apple writes them as they are rather than encoding them.
    static HRESULT   EncodeSectorNibbles (Byte track, Byte sector, std::span<const Byte> data, std::vector<Byte> & out);
    static HRESULT   ParseSectorNibbles  (const CassoLinkPacket & packet, Byte & outTrack, Byte & outSector, std::span<Byte> outData);

    static Word      ComputeCrc        (std::span<const Byte> data);
    static Byte      ComputeEor        (std::span<const Byte> data);
    static void      EncodePages       (std::span<const Byte> data, std::vector<Byte> & out);

    // Decodes `length` bytes from the front of `in`, as EncodePages wrote
    // them. outComplete is clear when `in` ends first.
    static HRESULT   DecodePages       (std::span<const Byte> in, size_t length, size_t & outConsumed, std::vector<Byte> & outData, bool & outComplete);

private:
    static constexpr size_t  kEnvelopeSize = 5;

    static void      AppendEnvelope    (Byte command, Word length, std::vector<Byte> & out);
    static HRESULT   DecodeFramed      (std::span<const Byte> in, size_t length, size_t & outConsumed, CassoLinkPacket & outPacket, bool & outComplete);
    static HRESULT   DecodeImageData   (std::span<const Byte> in, size_t length, size_t & outConsumed, CassoLinkPacket & outPacket, bool & outComplete);
};

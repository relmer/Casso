#pragma once

#include "Pch.h"

#include "Seams/IByteStream.h"
#include "Seams/RawDevice/IRawDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceProtocol
//
//  The bytes exchanged between a Casso Explorer window and the elevated
//  process that has a drive open.
//
//  A request is a header: the operation in one byte, a block number in
//  eight and a block count in four, least significant byte first. A write's
//  blocks follow its header. A reply begins with an HRESULT in four bytes.
//  After a success, a read's blocks follow, and for kInfo the drive: its
//  size in eight bytes, one byte that is 1 for a removable drive, its number
//  in four, then its model and its bus, each a length in two bytes and that
//  many characters.
//
////////////////////////////////////////////////////////////////////////////////

class RawDeviceProtocol
{
public:
    static constexpr Byte      kInfo        = 1;
    static constexpr Byte      kRead        = 2;
    static constexpr Byte      kWrite       = 3;

    static constexpr size_t    kHeaderBytes = 13;

    //  The most blocks in one read or one write.
    static constexpr uint32_t  kMaxBlocks   = 128;

    static constexpr DWORD     kReplyTimeoutMs = 60000;

    //  Fails with ERROR_BROKEN_PIPE when the stream closes first, and with
    //  ERROR_TIMEOUT when `timeoutMs` passes first. INFINITE waits for the
    //  bytes or the close.
    static HRESULT  ReadExact (IByteStream & stream, Byte * buffer, size_t count, DWORD timeoutMs);

    static void      AppendLong   (std::vector<Byte> & bytes, uint32_t value);
    static void      AppendQuad   (std::vector<Byte> & bytes, uint64_t value);
    static void      AppendText   (std::vector<Byte> & bytes, const std::string & text);
    static uint32_t  GetLong      (const Byte * bytes);
    static uint64_t  GetQuad      (const Byte * bytes);

    static HRESULT   ReadText     (IByteStream & stream, std::string & outText);

private:
    static constexpr DWORD   kSliceMs      = 250;
    static constexpr size_t  kMaxTextBytes = 256;
};

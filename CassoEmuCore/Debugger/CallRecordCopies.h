#pragma once

#include "Pch.h"

#include "Debugger/CallStack.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CallRecordCopies
//
//  Copies of the debugger's call record kept at history's keyframes, at
//  most one per keyframe: the record as it stood there, its last instruction
//  settled, which is what a rebuild of the record from history holds there.
//  A rebuild for a later position starts from the newest copy at or before
//  it rather than from where the record begins.
//
//  Each copy is packed (Pack), a few dozen bytes for a record a few calls
//  deep, and laid end to end with the others in one arena after a header
//  that counts the copies using it; a copy whose bytes match those of the
//  copy just before it uses them rather than its own. The index holds a
//  position and an offset per copy, oldest first. Over the budget the oldest
//  copies go. The arena is laid out again in place once the bytes no copy
//  uses are half of it, or it has no room for a new copy, so it grows past
//  the budget only for a copy that needs it.
//
//  Pack and Unpack touch nothing but their arguments; the rest runs on one
//  thread.
//
////////////////////////////////////////////////////////////////////////////////

class CallRecordCopies
{
public:
    static void      Pack              (const CallRecord & record, std::vector<Byte> & outBytes);
    static HRESULT   Unpack            (std::span<const Byte> bytes, CallRecord & outRecord);

    void             SetBudget         (size_t budgetBytes);
    void             Clear             ();

    //  Set replaces any copy at position; TryAdd keeps one there, and is
    //  false then.
    void             Set               (uint64_t position, std::span<const Byte> packed);
    bool             TryAdd            (uint64_t position, std::span<const Byte> packed);
    void             Drop              (uint64_t position);
    bool             TryFindAtOrBefore (uint64_t position, uint64_t & outPosition) const;

    //  Empty when no copy is kept at position.
    std::span<const Byte>  GetPacked (uint64_t position) const;

    //  The copies, oldest first.
    size_t           GetCount          () const { return m_entries.size() - m_first; }
    uint64_t         GetPosition       (size_t index) const { return m_entries[m_first + index].position; }
    size_t           GetBudget         () const { return m_budget; }

    //  The memory held, the index and the arena as allocated; and the bytes
    //  the copies use of it, which the budget limits.
    size_t           GetByteCount      () const;
    size_t           GetUsedByteCount  () const;

private:
    //  An arena block's header: the copies using its bytes, then how many
    //  bytes follow, each four bytes, low byte first.
    static constexpr size_t  kHeaderBytes = 8;
    static constexpr size_t  kFieldBytes  = 4;
    static constexpr int     kByteBits    = 8;

    //  Numbers are written seven bits to a byte, low bits first, with the top
    //  bit set on every byte but the last.
    static constexpr int     kNumberBits    = 7;
    static constexpr Byte    kNumberMask    = 0x7F;
    static constexpr Byte    kMoreBit       = 0x80;
    static constexpr int     kNumberLimit   = 64;

    //  A packed record's first byte, and a frame's flags.
    static constexpr Byte    kIsActive      = 0x01;
    static constexpr Byte    kHasLastReturn = 0x02;
    static constexpr Byte    kIsVerified    = 0x01;
    static constexpr Byte    kIsRewritten   = 0x02;
    static constexpr Byte    kHasRisen      = 0x04;

    struct Entry
    {
        uint64_t  position = 0;
        size_t    offset   = 0;      // where its block's header starts in the arena
    };

    //  Where Unpack has read to.
    struct Reader
    {
        std::span<const Byte>  bytes;
        size_t                 at = 0;
    };

    bool             Insert            (uint64_t position, std::span<const Byte> packed, bool isReplacing);
    size_t           Append            (std::span<const Byte> packed);
    void             Release           (const Entry & entry);
    void             Trim              ();
    void             Compact           ();
    size_t           FindAtOrAfter     (uint64_t position) const;
    bool             HasBytes          (size_t offset, std::span<const Byte> packed) const;
    uint32_t         ReadField         (size_t at) const;
    void             WriteField        (size_t at, uint32_t value);

    static void      WriteWord         (std::vector<Byte> & out, Word value);
    static void      WriteNumber       (std::vector<Byte> & out, uint64_t value);
    static void      WriteText         (std::vector<Byte> & out, const std::string & text);
    static void      WriteFrame        (std::vector<Byte> & out, const CallStackFrame & frame);

    static HRESULT   ReadByte          (Reader & reader, Byte & outValue);
    static HRESULT   ReadWord          (Reader & reader, Word & outValue);
    static HRESULT   ReadNumber        (Reader & reader, uint64_t & outValue);
    static HRESULT   ReadCount         (Reader & reader, size_t & outCount);
    static HRESULT   ReadText          (Reader & reader, std::string & outText);
    static HRESULT   ReadFrame         (Reader & reader, CallStackFrame & outFrame);
    static HRESULT   ReadBreak         (Reader & reader, CallStackRecorder::Break & outBreak);

    std::vector<Entry>  m_entries;               // sorted by position; those before m_first are gone
    size_t              m_first   = 0;
    std::vector<Byte>   m_arena;
    size_t              m_garbage = 0;           // arena bytes in blocks no copy uses
    size_t              m_budget  = SIZE_MAX;
};

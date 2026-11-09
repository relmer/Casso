#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveProbe
//
//  Why an archive the shell lists nothing in has nothing: empty, damaged, or
//  locked by a password, read from a zip's own directory.
//
//  THE SHELL LISTS FIRST. A zip the shell lists is read; this runs only
//  when it lists nothing, which the shell does alike for an empty zip and a
//  damaged one, and the message is the reason.
//
//  Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class ArchiveProbe
{
public:
    enum class Verdict
    {
        Empty,              // a whole zip with no entries
        HasEntries,         // a whole zip with entries, none encrypted
        PasswordProtected,  // a whole zip with an encrypted entry
        Damaged,            // a zip whose directory is missing or cut short
        NotZip,             // no zip signature at all
    };

    static Verdict  ProbeZip (std::span<const Byte> bytes);

    //  The sentence for an archive the shell lists nothing in, by the file's
    //  name; empty for a verdict with nothing to report.
    static std::wstring  GetReason (const std::wstring & name, Verdict verdict);

private:
    static constexpr uint32_t  s_kLocalSignature     = 0x04034B50;
    static constexpr uint32_t  s_kDirectorySignature = 0x02014B50;
    static constexpr uint32_t  s_kEndSignature       = 0x06054B50;
    static constexpr size_t    s_kEndBytes           = 22;
    static constexpr size_t    s_kMaxCommentBytes    = 0xFFFF;
    static constexpr size_t    s_kEntryBytes         = 46;
    static constexpr uint16_t  s_kEncryptedFlag      = 0x0001;

    static uint16_t  ReadU16 (std::span<const Byte> bytes, size_t at);
    static uint32_t  ReadU32 (std::span<const Byte> bytes, size_t at);
};

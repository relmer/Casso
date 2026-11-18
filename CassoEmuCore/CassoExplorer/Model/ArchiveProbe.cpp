#include "Pch.h"

#include "CassoExplorer/Model/ArchiveProbe.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveProbe::ReadU16
//
////////////////////////////////////////////////////////////////////////////////

uint16_t ArchiveProbe::ReadU16 (std::span<const Byte> bytes, size_t at)
{
    return (uint16_t) (bytes[at] | (bytes[at + 1] << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveProbe::ReadU32
//
////////////////////////////////////////////////////////////////////////////////

uint32_t ArchiveProbe::ReadU32 (std::span<const Byte> bytes, size_t at)
{
    return (uint32_t) ReadU16 (bytes, at) | ((uint32_t) ReadU16 (bytes, at + 2) << 16);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveProbe::ProbeZip
//
//  The end record is found by searching back from the end, past a comment of
//  up to 64 KB, and its directory must lie whole before it, each entry where
//  the one before ends. A file that starts as a zip but has no such record is
//  damaged, as one cut short in a download is.
//
////////////////////////////////////////////////////////////////////////////////

ArchiveProbe::Verdict ArchiveProbe::ProbeZip (std::span<const Byte> bytes)
{
    size_t    end       = SIZE_MAX;
    size_t    lowest    = 0;
    size_t    at        = 0;
    uint16_t  entries   = 0;
    uint32_t  dirSize   = 0;
    uint32_t  dirOffset = 0;
    bool      encrypted = false;



    if (bytes.size() < 4 || (ReadU32 (bytes, 0) != s_kLocalSignature && ReadU32 (bytes, 0) != s_kEndSignature))
    {
        return Verdict::NotZip;
    }

    if (bytes.size() < s_kEndBytes)
    {
        return Verdict::Damaged;
    }

    lowest = (bytes.size() > s_kEndBytes + s_kMaxCommentBytes) ? bytes.size() - s_kEndBytes - s_kMaxCommentBytes : 0;

    for (size_t candidate = bytes.size() - s_kEndBytes + 1; candidate-- > lowest; )
    {
        if (ReadU32 (bytes, candidate) == s_kEndSignature)
        {
            end = candidate;
            break;
        }
    }

    if (end == SIZE_MAX)
    {
        return Verdict::Damaged;
    }

    entries   = ReadU16 (bytes, end + 10);
    dirSize   = ReadU32 (bytes, end + 12);
    dirOffset = ReadU32 (bytes, end + 16);

    if ((size_t) dirOffset + dirSize > end)
    {
        return Verdict::Damaged;
    }

    at = dirOffset;

    for (uint16_t index = 0; index < entries; index++)
    {
        if (at + s_kEntryBytes > end || ReadU32 (bytes, at) != s_kDirectorySignature)
        {
            return Verdict::Damaged;
        }

        encrypted = encrypted || (ReadU16 (bytes, at + 8) & s_kEncryptedFlag) != 0;
        at       += s_kEntryBytes + ReadU16 (bytes, at + 28) + ReadU16 (bytes, at + 30) + ReadU16 (bytes, at + 32);
    }

    return (entries == 0) ? Verdict::Empty
         : encrypted      ? Verdict::PasswordProtected
                          : Verdict::HasEntries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ArchiveProbe::GetReason
//
//  A zip with entries the shell lists nothing in is reported as
//  damaged too: its directory reads, but nothing in it opens.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ArchiveProbe::GetReason (const std::wstring & name, Verdict verdict)
{
    switch (verdict)
    {
        case Verdict::Damaged:
        case Verdict::HasEntries:
            return name + L" can't be opened because it is damaged.";

        case Verdict::PasswordProtected:
            return name + L" can't be opened because it is protected by a password.";

        case Verdict::NotZip:
            return name + L" can't be opened. It may be damaged or protected by a password.";

        case Verdict::Empty:
        default:
            return std::wstring();
    }
}

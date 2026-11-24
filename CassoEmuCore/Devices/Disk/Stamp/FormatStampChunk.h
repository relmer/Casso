#pragma once

#include "Pch.h"

struct FormatStamp;
struct WozMetadata;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampChunk
//
//  The CSSO chunk that has a format stamp: in a loaded image's WOZ metadata,
//  where it is one of the chunks a save writes back, and in a WOZ file's
//  bytes, for an edit that leaves every other chunk where it is. A file with
//  more than one uses the first.
//
////////////////////////////////////////////////////////////////////////////////

enum class StampState
{
    Absent,
    Readable,
    Unreadable,
};


class FormatStampChunk
{
public:
    static constexpr Byte  kId[4] = { 'C', 'S', 'S', 'O' };

    static StampState  GetStamp          (const WozMetadata & meta, FormatStamp & outStamp);
    static HRESULT     SetStamp          (WozMetadata & ioMeta, const FormatStamp & stamp);
    static void        RemoveStamp       (WozMetadata & ioMeta);
    static bool        HasStampChunk     (const WozMetadata & meta);

    static HRESULT     GetStampFromFile  (std::span<const Byte> file, std::string & outText, StampState & outState, FormatStamp & outStamp);
    static HRESULT     SetStampInFile    (vector<Byte> & ioFile, const FormatStamp & stamp);

private:
    static bool        IsWozFile         (std::span<const Byte> file);
};

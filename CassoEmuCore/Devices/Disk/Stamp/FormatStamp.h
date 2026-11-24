#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"
#include "Devices/Disk/Inspector/DecodeSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStamp and its parts
//
//  The format of each track of a 5.25" WOZ image as Casso analyzed it, stored
//  in the file's CSSO chunk as JSON. A read of the image decodes a track with
//  its stamped marks only where they produce better sectors than detection,
//  so a stale or wrong stamp slows a read and never changes its result. The
//  file format is in docs/file-formats/woz-format-stamp.md.
//
////////////////////////////////////////////////////////////////////////////////

enum class StampSource
{
    Capture,
    Inspector,
};


enum class StampEncoding
{
    Empty,
    Sixteen,
    Thirteen,
    Both,
    Unformatted,
    Nonstandard,
};


enum class StampChecksum
{
    Standard,
    NotChecked,
    Failing,
};


//  A copy protection fact. The params are written back as they were read, so
//  a kind this build does not recognize survives a rewrite.
struct ProtectionNote
{
    std::string  kind;
    JsonValue    params;
};


struct TrackCaptureFacts
{
    bool         isWeak            = false;
    bool         isUnstable        = false;
    bool         hasSeparateStream = false;
    vector<int>  replacedSectors;
};


struct CaptureFacts
{
    int     reads            = 0;
    bool    hasRevolution    = false;
    double  revolutionCycles = 0.0;
    bool    isSynchronized   = false;
    int     betweenRead      = 0;
    int     betweenTotal     = 0;
};


struct StampTrack
{
    int                     position              = 0;
    StampEncoding           encoding              = StampEncoding::Empty;
    vector<vector<Byte>>    addressPrologs;
    vector<vector<Byte>>    addressEpilogs;
    vector<vector<Byte>>    dataPrologs;
    vector<vector<Byte>>    dataEpilogs;
    vector<Byte>            sectors;
    vector<Byte>            volumes;
    StampChecksum           addressChecksum       = StampChecksum::Standard;
    StampChecksum           dataChecksum          = StampChecksum::Standard;
    int                     sectorsFound          = 0;
    int                     sectorsGood           = 0;
    bool                    isVerified            = false;
    bool                    isChangedAfterCapture = false;
    bool                    hasCapture            = false;
    TrackCaptureFacts       capture;
    vector<ProtectionNote>  notes;
};


struct FormatStamp
{
    static constexpr int     kVersionMajor = 1;
    static constexpr int     kVersionMinor = 0;
    static constexpr int     kMaxPositions = 160;
    static constexpr size_t  kMaxBytes     = 1024 * 1024;

    int                     versionMajor = kVersionMajor;
    int                     versionMinor = kVersionMinor;
    std::string             writer;
    std::string             written;
    StampSource             source       = StampSource::Capture;
    vector<DecodeRange>     decodeRanges;
    bool                    hasCapture   = false;
    CaptureFacts            capture;
    vector<ProtectionNote>  notes;
    vector<StampTrack>      tracks;
};

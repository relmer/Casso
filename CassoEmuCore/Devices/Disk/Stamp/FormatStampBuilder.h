#pragma once

#include "Pch.h"

#include "Devices/Disk/Stamp/FormatStamp.h"

class DiskCopy;
struct LocatedField;
struct TrackAnalysis;
struct TrackCopy;





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureReport
//
//  Facts from a capture's reads, by quarter-track position: the facts
//  for the whole disk, and the positions that were weak, unstable or have a
//  separate stream, and each sector replaced from another read.
//
////////////////////////////////////////////////////////////////////////////////

struct CaptureReport
{
    CaptureFacts                 facts;
    vector<int>                  weak;
    vector<int>                  unstable;
    vector<int>                  streams;
    vector<std::pair<int, int>>  replaced;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampBuilder
//
//  A format stamp from the disk inspector's analysis of a disk copy. A track
//  with unverified sectors under the decode settings is analyzed again with
//  the marks MarkDiscovery finds on it, and the analysis with more good
//  sectors is used. Every quarter-track position with a separate record has
//  an entry: each whole track, and each half or quarter track whose record
//  differs from both neighboring whole tracks' records.
//
////////////////////////////////////////////////////////////////////////////////

class FormatStampBuilder
{
public:
    static void         Build             (const DiskCopy        &  copy,
                                           const DecodeSettings  &  settings,
                                           StampSource              source,
                                           const CaptureReport   *  capture,
                                           const std::string     &  written,
                                           FormatStamp           &  outStamp);

    static void         BuildTrack        (const DiskCopy & copy, int position, const DecodeSettings & settings, StampTrack & outTrack);
    static void         FillTrack         (const TrackAnalysis & analysis, StampTrack & outTrack);
    static vector<int>  GetStampPositions (const DiskCopy & copy);
    static std::string  GetWriter         ();
    static uint32_t     ComputeTrackCrc   (const TrackCopy & track);
    static void         KeepCaptureFacts  (const FormatStamp & earlier, FormatStamp & ioStamp);
    static std::string  FormatUtc         (std::chrono::system_clock::time_point when);

private:
    static StampEncoding  GetEncoding     (const TrackAnalysis & analysis);
    static bool           IsUnverified    (const TrackAnalysis & analysis);
    static void           AddDistinct     (vector<vector<Byte>> & ioLists, vector<Byte> && nibbles);
    static void           AddEpilog       (const LocatedField & field, vector<vector<Byte>> & ioLists);
    static void           FillChecksums   (const TrackAnalysis & analysis, StampTrack & outTrack);
    static void           FillCapture     (const CaptureReport & capture, FormatStamp & ioStamp);
};

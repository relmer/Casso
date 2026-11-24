#pragma once

#include "Pch.h"

struct FormatStamp;
struct StampTrack;





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampText
//
//  A format stamp as the table `disk get-stamp` prints: a line for the stamp's
//  source, writer and date, a line for the capture's facts when it has them,
//  then a row per track with its position, encoding, address and data
//  prologs, sectors found and good, volumes and notes.
//
////////////////////////////////////////////////////////////////////////////////

class FormatStampText
{
public:
    static std::string  FormatTable    (const FormatStamp & stamp);
    static std::string  FormatPosition (int position);

private:
    static std::string  FormatHeader   (const FormatStamp & stamp);
    static std::string  FormatRow      (const StampTrack & track);
    static std::string  FormatMarks    (const vector<vector<Byte>> & marks);
    static std::string  FormatNotes    (const StampTrack & track);
};

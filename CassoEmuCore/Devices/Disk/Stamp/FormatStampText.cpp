#include "Pch.h"

#include "Devices/Disk/Stamp/FormatStampText.h"

#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampJson.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampText::FormatTable
//
////////////////////////////////////////////////////////////////////////////////

std::string FormatStampText::FormatTable (const FormatStamp & stamp)
{
    std::string  text = FormatHeader (stamp);



    text += std::format ("\n{:<7}{:<13}{:<20}{:<20}{:<9}{:<6}{:<8}{}\n", "Track", "Encoding", "Address", "Data", "Sectors", "Good", "Volume", "Notes");

    for (const StampTrack & track : stamp.tracks)
    {
        text += FormatRow (track);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampText::FormatHeader
//
////////////////////////////////////////////////////////////////////////////////

std::string FormatStampText::FormatHeader (const FormatStamp & stamp)
{
    std::string  text = std::format ("Format stamp: {}, {}, {}\n", FormatStampJson::SourceToText (stamp.source), stamp.writer, stamp.written);



    if (stamp.hasCapture)
    {
        text += std::format ("Capture: {} reads", stamp.capture.reads);

        if (stamp.capture.hasRevolution)
        {
            text += std::format (", revolution {:.0f} cycles", stamp.capture.revolutionCycles);
        }

        text += stamp.capture.isSynchronized ? ", synchronized\n" : ", not synchronized\n";
    }

    if (!stamp.decodeRanges.empty())
    {
        text += std::format ("Decode settings: {} custom ranges\n", stamp.decodeRanges.size());
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampText::FormatPosition
//
//  A quarter-track position as a track number: 17, 17.25, 17.5 or 17.75.
//
////////////////////////////////////////////////////////////////////////////////

std::string FormatStampText::FormatPosition (int position)
{
    constexpr int          kPerTrack    = 4;
    constexpr const char * kFractions[] = { "", ".25", ".5", ".75" };



    return std::format ("{}{}", position / kPerTrack, kFractions[position % kPerTrack]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampText::FormatRow
//
////////////////////////////////////////////////////////////////////////////////

std::string FormatStampText::FormatRow (const StampTrack & track)
{
    std::string  volumes;



    for (Byte volume : track.volumes)
    {
        volumes += (volumes.empty() ? "" : ",") + std::to_string (volume);
    }

    return std::format ("{:<7}{:<13}{:<20}{:<20}{:<9}{:<6}{:<8}{}\n",
                        FormatPosition (track.position),
                        FormatStampJson::EncodingToText (track.encoding),
                        FormatMarks (track.addressPrologs),
                        FormatMarks (track.dataPrologs),
                        track.sectorsFound,
                        track.sectorsGood,
                        volumes,
                        FormatNotes (track));
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampText::FormatMarks
//
////////////////////////////////////////////////////////////////////////////////

std::string FormatStampText::FormatMarks (const vector<vector<Byte>> & marks)
{
    std::string  text;



    for (const vector<Byte> & mark : marks)
    {
        text += (text.empty() ? "" : ", ") + FormatStampJson::NibblesToText (mark);
    }

    return text.empty() ? "-" : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampText::FormatNotes
//
//  The capture's facts for the track, whether it changed after the
//  capture, and the kinds of its protection notes.
//
////////////////////////////////////////////////////////////////////////////////

std::string FormatStampText::FormatNotes (const StampTrack & track)
{
    vector<std::string>  notes;
    std::string          text;



    if (track.capture.isWeak)            { notes.push_back ("weak"); }
    if (track.capture.isUnstable)        { notes.push_back ("unstable"); }
    if (track.capture.hasSeparateStream) { notes.push_back ("separate stream"); }
    if (track.isChangedAfterCapture)     { notes.push_back ("changed after capture"); }

    if (!track.capture.replacedSectors.empty())
    {
        std::string  replaced = "replaced";

        for (int sector : track.capture.replacedSectors)
        {
            replaced += " " + std::to_string (sector);
        }

        notes.push_back (replaced);
    }

    for (const ProtectionNote & note : track.notes)
    {
        notes.push_back (note.kind);
    }

    for (const std::string & note : notes)
    {
        text += (text.empty() ? "" : ", ") + note;
    }

    return text;
}

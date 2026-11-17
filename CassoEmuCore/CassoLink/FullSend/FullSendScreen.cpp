#include "Pch.h"

#include "CassoLink/FullSend/FullSendScreen.h"

#include "CassoLink/Session/CassoLinkSessionTypes.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreen::GetRows
//
//  The rows the Apple draws: the title with the settings, the counts, a
//  line, and each drive's row and map. The prefix is quoted, so a space at
//  its end can be seen.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> FullSendScreen::GetRows() const
{
    std::string               shown = prefix.empty() ? std::string ("none") : "\"" + prefix + "\"";
    std::string               flag  = "Verify: off";
    std::vector<std::string>  rows;



    if (verify)
    {
        flag = "Verify: on";

        for (char & ch : flag)
        {
            ch = (char) (ch | 0x80);
        }
    }

    rows = { "Full Send    " + flag + "  Prefix: " + shown, std::format ("Sent {}   Dup {}   Bad {}", sent, duplicates, bad), std::string (40, '-') };

    if (recovered != 0)
    {
        rows[1] += std::format ("   Fixed {}", recovered);
    }

    if (prot != 0)
    {
        rows[1] += std::format ("   Prot {}", prot);
    }

    for (size_t i = 0; i < drives.size(); i++)
    {
        std::string  row = std::format ("D{}: ", i + 1);

        switch (drives[i])
        {
            case FullSendDriveState::Off:      row.clear();                 break;
            case FullSendDriveState::Idle:     row += "Idle";               break;
            case FullSendDriveState::Checking: row += "Checking";           break;
            case FullSendDriveState::Sending:  row += (verify ? "Verifying " : "Sending ") + names[i]; break;
            case FullSendDriveState::Empty:    row += "Unformatted, or no disk";   break;
            case FullSendDriveState::Blank:    row += names[i] + ", blank"; break;
            case FullSendDriveState::Repeat:    row += GetDuplicateText (names[i], "?"); break;
            case FullSendDriveState::Sent:     row += "Sent " + names[i];   break;
            case FullSendDriveState::Protected: row += "Protected, captured raw " + names[i]; break;
            case FullSendDriveState::Capturing: row += "Protected, capturing raw " + names[i]; break;
            case FullSendDriveState::Validating: row += "Validating image on PC, please wait"; break;
            case FullSendDriveState::ProtectedSent: row += "Protected, sent " + names[i]; break;
            case FullSendDriveState::Changed:  row += "Changed while it was read"; break;
            case FullSendDriveState::OnDeck:
                row += "On deck--";
                row += quip == 0 ? "Insert a disk to send next" : GetQuipText (quip);
                break;
        }

        rows.push_back (row);
        rows.push_back (maps[i]);
    }

    return rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreen::MakeTrackRuler
//
//  A cell a track: the tens digit at each tenth track, a colon at each fifth
//  and a dot elsewhere, so a track marked X can be read off the row.
//
////////////////////////////////////////////////////////////////////////////////

std::string & FullSendScreen::MakeTrackRuler (std::string & ioMap)
{
    ioMap.assign (NibblizationLayer::kTrackCount, '.');

    for (size_t track = 0; track < ioMap.size(); track += 5)
    {
        ioMap[track] = (track % 10 == 0) ? (char) ('0' + track / 10) : ':';
    }

    return ioMap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreen::GetDuplicateText
//
//  DUPE OF and the earlier disk's file, cut short so it and the mark fit after
//  D1: on a 40-column row. A drive's check ends in ?, as a match compares only
//  a few tracks; a match of the whole-disk hash has no mark.
//
////////////////////////////////////////////////////////////////////////////////

std::string FullSendScreen::GetDuplicateText (const std::string & file, const char * mark)
{
    static constexpr size_t  s_kWidth = 40 - 4;
    std::string              text     = "Dupe of " + file;
    size_t                   room     = s_kWidth - strlen (mark);



    if (text.size() > room)
    {
        text.resize (room);
    }

    return text + mark;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreen::GetQuipText
//
//  A prompt line, from 1. Each fits after ON DECK-- on a 40-column row.
//
////////////////////////////////////////////////////////////////////////////////

const char * FullSendScreen::GetQuipText (Byte quip)
{
    static constexpr std::array<const char *, kQuipCount>  s_kQuips =
    {
        "Is that all you've got?",
        "Another!",
        "I can haz disk?",
        "Thank you sir, another?",
        "More! Mooore!!!",
        "Feed me, Seymour!",
        "Insert coin... er, disk",
        "This drive is hungry",
        "Next victim, please",
        "Please sir, some more",
        "Bigger stack needed",
        "Need input!",
        "Me want disk! Nom nom nom",
        "Hit me again, dealer",
        "One disk is never enough",
        "Just one more, you said",
    };



    return s_kQuips[(size_t) (quip - 1) % s_kQuips.size()];
}





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendScreen::MakeTrackMap
//
//  The track ruler with each track's worst sector on it: X for a sector
//  that wasn't read, U for one that read differently from read to read,
//  and R for one recovered.
//
////////////////////////////////////////////////////////////////////////////////

std::string & FullSendScreen::MakeTrackMap (std::string & ioMap, const std::vector<SectorMark> & marks, const std::vector<std::pair<int, int>> & unreadable)
{
    MakeTrackRuler (ioMap);

    for (const SectorMark & mark : marks)
    {
        char  & cell  = ioMap[(size_t) mark.track];
        bool    clear = cell == '.' || cell == ':' || isdigit ((unsigned char) cell);

        cell = (clear || mark.mark == 'X' || (mark.mark == 'U' && cell == 'R')) ? mark.mark : cell;
    }

    for (const auto & [track, sector] : unreadable)
    {
        ioMap[(size_t) track] = 'X';
    }

    return ioMap;
}

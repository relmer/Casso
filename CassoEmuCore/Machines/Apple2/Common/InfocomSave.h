#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSave
//
//  A save disk of an Infocom game's Apple II interpreter, which has no
//  catalog: eight positions of four tracks each, position n from track 4n.
//  A position is written in physical sector order, three pages of the
//  interpreter's state and then the story's dynamic memory, which begins
//  with the story's header. Measured from Zork I, release 88, saved in
//  Casso at positions 0, 1 and 7.
//
//  The header has the story's version, release and serial number, and the
//  address of its global variables. A version 3 story's status line is the
//  first three globals: the room, an object whose name is in its property
//  table, and the score and the moves, or the hours and minutes in a story
//  whose header marks it as timed. So a save is summarized without the
//  story disk.
//
////////////////////////////////////////////////////////////////////////////////

class InfocomSave
{
public:
    struct Position
    {
        int           number   = 0;
        int           version  = 0;
        int           release  = 0;
        std::string   serial;
        std::wstring  room;
        bool          isTimed  = false;
        int           score    = 0;      // the hours, in a timed story
        int           moves    = 0;      // the minutes, in a timed story
    };

    struct Story
    {
        int           release  = 0;
        const char *  serial   = "";
        const char *  title    = "";
        int           maxScore = 0;      // zero when not known, or for a timed story
    };

    static constexpr int     kPositions         = 8;
    static constexpr int     kTracksPerPosition = 4;
    static constexpr int     kStatePages        = 3;
    static constexpr size_t  kPageBytes         = 256;
    static constexpr int     kSectorsPerTrack   = 16;

    //  The positions a disk in DOS sector order has saved, in order.
    static void  FindPositions   (const std::vector<Byte> & sectors, std::vector<Position> & outPositions);

    //  Position `number`, or false when it has no saved game.
    static bool  TryReadPosition (const std::vector<Byte> & sectors, int number, Position & outPosition);

    //  One position as a line: the title and release, the room, and the
    //  score out of the maximum and the moves, or the time. The title and
    //  the maximum are left out for a story not in the table.
    static std::string  Describe (const Position & position);

    //  The story with this release and serial number, or nullptr.
    static const Story *  FindStory (int release, const std::string & serial);

private:
    static constexpr size_t  kHeaderBytes = 64;

    static Word  GetWord (const std::vector<Byte> & memory, size_t at);
};

#pragma once

#include "Pch.h"
#include "FileTypes/ZMachineStory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InfocomSave
//
//  A save disk of an Infocom game's Apple II interpreter, which has no
//  catalog: eight positions of four tracks each, position n from track 4n,
//  or from track 4n + 3 for an early interpreter, as Zork III's release 10.
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
//  The object tree is in dynamic memory too: what the player carries is the
//  tree under the player, and what's in the room is the rest of the tree
//  under the room. The player is the object in the room whose number is in
//  a global variable after the status line's.
//
////////////////////////////////////////////////////////////////////////////////

class InfocomSave
{
public:
    //  An object in the tree, under the object depth levels up.
    struct Item
    {
        int           depth = 0;
        std::wstring  name;

        bool  operator== (const Item &) const = default;
    };

    struct Position
    {
        int                number   = 0;
        int                version  = 0;
        int                release  = 0;
        std::string        serial;
        std::wstring       room;
        bool               isTimed  = false;
        int                score    = 0;      // the hours, in a timed story
        int                moves    = 0;      // the minutes, in a timed story
        std::vector<Item>  carried;
        std::vector<Item>  here;              // in the room, but for the player
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
    static constexpr int     kEarlyFirstTrack   = 3;

    //  The positions a disk in DOS sector order has saved, in order.
    static void  FindPositions   (const std::vector<Byte> & sectors, std::vector<Position> & outPositions);

    //  Position `number`, from `firstTrack` on, or false when it has no saved game.
    static bool  TryReadPosition (const std::vector<Byte> & sectors, int number, Position & outPosition, int firstTrack = 0);

    //  One position as a line: the title and release, the room, and the
    //  score out of the maximum and the moves, or the time. The title and
    //  the maximum are left out for a story not in the table.
    static std::string  Describe (const Position & position);

    //  The story with this release and serial number, or nullptr.
    static const Story *  FindStory (int release, const std::string & serial);

private:
    static constexpr size_t  kHeaderBytes       = 64;
    static constexpr size_t  kDefaultsBytes     = 62;     // a version 1 to 3 story's 31 property defaults
    static constexpr size_t  kObjectBytes       = 9;      // a version 1 to 3 object: 4 bytes of attributes, 3 links, the properties' address
    static constexpr size_t  kParentAt          = 4;
    static constexpr int     kGlobals           = 240;
    static constexpr int     kMaxDepth          = 6;
    static constexpr size_t  kMaxItems          = 200;

    static Word  GetWord       (const std::vector<Byte> & memory, size_t at);
    static int   CountObjects  (const std::vector<Byte> & memory, Word objects);
    static int   GetObjectLink (const std::vector<Byte> & memory, Word objects, int count, int number, size_t which);
    static int   FindPlayer    (const std::vector<Byte> & memory, const ZMachineStory::Header & header, int count, int room);
    static void  AddTree       (const std::vector<Byte> & memory, const ZMachineStory::Header & header, int count, int parent, int skip, int depth, std::vector<Item> & ioItems);
};

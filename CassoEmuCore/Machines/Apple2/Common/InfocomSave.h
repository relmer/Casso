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
//  A version 4 or 5 story's later interpreter saves positions numbered from
//  1 in slots of 136 pages, or 184 for a version 5 story or a version 4
//  story too big for the smaller slot, each five pages of state and then
//  dynamic memory. Measured from Wishbringer, A Mind Forever Voyaging,
//  Trinity, Nord and Bert, Sherlock, Beyond Zork and Border Zone saved in
//  Casso. The story code of these games prints the status line, so the
//  score and moves aren't in fixed globals. A table has the global of the
//  moves, or of the clock in minutes, of each release measured.
//
//  The object tree is in dynamic memory too: what the player carries is the
//  tree under the player, and what's in the room is the rest of the tree
//  under the room. The player is an object somewhere in the room whose
//  number is in a global variable after the status line's: one called
//  "cretin", "yourself" or another usual word for the player first, then
//  one with something in it, then the lowest global's.
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
        int                number    = 0;
        int                version   = 0;
        int                release   = 0;
        std::string        serial;
        std::wstring       room;
        bool               isTimed   = false;
        int                score     = 0;      // the hours, in a timed story
        int                moves     = 0;      // the minutes, in a timed story
        bool               hasStatus = true;   // the score and moves, or the time, are known
        bool               hasScore  = true;   // the score is known, with the moves
        std::vector<Item>  carried;
        std::vector<Item>  here;               // in the room, but for the player
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
    static constexpr int     kLateStatePages    = 5;
    static constexpr size_t  kSmallSlotPages    = 136;
    static constexpr size_t  kLargeSlotPages    = 184;
    static constexpr size_t  kDiskPages         = 560;

    //  The positions a disk in DOS sector order has saved, in order.
    static void  FindPositions   (const std::vector<Byte> & sectors, std::vector<Position> & outPositions);

    //  Position `number`, from `firstTrack` on, or false when it has no saved game.
    static bool  TryReadPosition (const std::vector<Byte> & sectors, int number, Position & outPosition, int firstTrack = 0);

    //  A version 4 or 5 story's position in slot `index` of `slotPages`
    //  pages, numbered index + 1, or false when it has no saved game.
    static bool  TryReadLatePosition (const std::vector<Byte> & sectors, int index, size_t slotPages, Position & outPosition);

    //  One position as a line: the title and release, the room, and the
    //  score out of the maximum and the moves, or the time. The title and
    //  the maximum are left out for a story not in the table.
    static std::string  Describe (const Position & position);

    //  The story with this release and serial number, or nullptr.
    static const Story *  FindStory (int release, const std::string & serial);

    //  A title with its subtitle, as "Zork I: The Great Underground Empire",
    //  or the title alone for a game without one.
    static std::string  GetFullTitle (const std::string & title);

    //  The label of the moves global: "Time" in a game whose moves are its
    //  clock, as Planetfall's, else "Moves".
    static const wchar_t *  GetMovesLabel (const std::string & title);

    //  Whether the game has no score, as Suspended, whose score global has
    //  something else.
    static bool  IsUnscored (const std::string & title);

private:
    static constexpr size_t  kHeaderBytes       = 64;
    static constexpr size_t  kDefaultsBytes     = 62;     // a version 1 to 3 story's 31 property defaults
    static constexpr size_t  kObjectBytes       = 9;      // a version 1 to 3 object: 4 bytes of attributes, 3 links, the properties' address
    static constexpr size_t  kParentAt          = 4;
    static constexpr size_t  kLateDefaultsBytes = 126;    // a later story's 63 property defaults
    static constexpr size_t  kLateObjectBytes   = 14;     // a later object: 6 bytes of attributes, 3 link words, the properties' address
    static constexpr size_t  kLateParentAt      = 6;
    static constexpr int     kMaxEarlyObjects   = 255;
    static constexpr int     kMaxLateObjects    = 4000;
    static constexpr int     kGlobals           = 240;
    static constexpr int     kMaxDepth          = 6;
    static constexpr size_t  kMaxItems          = 200;
    static constexpr int     kNameScore         = 2;
    static constexpr int     kMinutesPerHour    = 60;

    static Word  GetWord       (const std::vector<Byte> & memory, size_t at);
    static void  ReadPages     (const std::vector<Byte> & sectors, size_t first, size_t count, std::vector<Byte> & outMemory);
    static bool  ReadMemory    (std::vector<Byte> & memory, int number, bool isLate, Position & outPosition);
    static int   CountObjects  (const std::vector<Byte> & memory, const ZMachineStory::Header & header);
    static int   GetObjectLink (const std::vector<Byte> & memory, const ZMachineStory::Header & header, int count, int number, size_t which);
    static int   FindLateRoom  (const std::vector<Byte> & memory, const ZMachineStory::Header & header, int count);
    static bool  IsInGlobals   (const std::vector<Byte> & memory, const ZMachineStory::Header & header, int count, int number);
    static int   FindPlayer    (const std::vector<Byte> & memory, const ZMachineStory::Header & header, int count, int room);
    static bool  IsInside      (const std::vector<Byte> & memory, const ZMachineStory::Header & header, int count, int number, int room);
    static bool  IsPlayerName  (const std::wstring & name);
    static void  AddTree       (const std::vector<Byte> & memory, const ZMachineStory::Header & header, int count, int parent, int skip, int depth, std::vector<Item> & ioItems);
};

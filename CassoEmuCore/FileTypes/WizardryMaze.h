#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryMaze
//
//  The maze of Wizardry V, which is on the scenario disks B through I, one
//  level to a disk side, while the saved game is on disk A. The side number,
//  1 for A, is the word at the end of block 1.
//
//  A level is 32 by 32 squares, x east and y north, wrapping at the edges.
//  Block 135 of its side has two tables of 256 bytes: each square's east
//  side, then its north side. A table is 128 words; square (x, y) is in word
//  (y div 8) * 32 + x, two bits at 2 * (y mod 8): open, a wall, a door, or a
//  hidden door.
//
//  A character out in the maze has the word at its record's kOutAt clear and
//  its square in the word at kLocationAt: x mod 8 in bits 0 to 2, y mod 8 in
//  bits 3 to 5, the 8 by 8 block (y div 8) * 4 + x div 8 in bits 6 to 9, and
//  the level from bit 10.
//
////////////////////////////////////////////////////////////////////////////////

class WizardryMaze
{
public:
    struct Square
    {
        int  x     = 0;
        int  y     = 0;
        int  level = 0;
    };

    static constexpr int     kSide          = 32;
    static constexpr int     kWallsBlock    = 135;
    static constexpr int     kSideBlock     = 1;
    static constexpr size_t  kSideAt        = 510;
    static constexpr size_t  kNorthAt       = 256;
    static constexpr size_t  kWallsBytes    = 512;
    static constexpr int     kFirstMazeSide = 2;
    static constexpr int     kLastMazeSide  = 9;
    static constexpr size_t  kOutAt         = 38;
    static constexpr size_t  kLocationAt    = 170;

    static Square  ReadSquare    (int word);
    static int     GetSide       (std::span<const Byte> sectors);
    static bool    FindWalls     (std::span<const std::vector<Byte>> sides, int level, std::vector<Byte> & outWalls);
    static int     GetWall       (std::span<const Byte> walls, int x, int y, bool north);
    static void    FindSides     (IDiskFileIo & fileIo, const std::string & imagePath, std::vector<std::vector<Byte>> & outSides);
    static void    LoadSaveDisks (IDiskFileIo & fileIo, const std::string & imagePath, std::vector<Byte> & outSectors, std::vector<std::vector<Byte>> & outSides);

private:
    static bool  LoadSide (IDiskFileIo & fileIo, const std::string & path, std::vector<Byte> & outSectors);

    static constexpr int       kWallBits     = 2;
    static constexpr int       kRowsPerWord  = 8;
    static constexpr int       kBlocksAcross = 4;
    static constexpr uint64_t  kMaxSideBytes = 1024 * 1024;
};

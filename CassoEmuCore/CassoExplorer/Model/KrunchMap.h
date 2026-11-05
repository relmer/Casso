#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/PascalVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap
//
//  A krunch drawn as a map of the volume's blocks, one cell a block, as a
//  defragmenter draws a disk: what is in each block at a point in the krunch,
//  from every file's place before and after, the percentage of the blocks to
//  move that are moved, and the files moved.
//
////////////////////////////////////////////////////////////////////////////////

class KrunchMap
{
public:
    enum class Cell : Byte
    {
        Free,
        System,     // the boot blocks and the directory, which never move
        InPlace,    // a file in its place after the krunch
        ToMove,     // a file that moves later in the krunch
        Reading,    // the file being moved, at its old place
        Writing,    // and the part of it written at its new place
    };

    //  A block's cell and the index in `files` of the file in it, or -1.
    struct Block
    {
        Cell  cell = Cell::Free;
        int   file = -1;
    };

    static constexpr int  kColumns = 40;
    static constexpr int  kBlocks  = 280;

    //  The blocks, kBlocks of them, for `files` in directory order. A file
    //  moves when its `to` differs from its `from`; `filesMoved` of those are
    //  in their new places. Until the Apple takes the request, `started` is
    //  false and no file is being moved.
    static std::vector<Block>  Build (const std::vector<PascalKrunchMove> & files, bool started, int percent, int filesMoved);

    //  The blocks the krunch moves, and the files.
    static int  CountBlocksToMove (const std::vector<PascalKrunchMove> & files);
    static int  CountFilesToMove  (const std::vector<PascalKrunchMove> & files);

    //  The index in `files` of the file being moved, or -1 when none is.
    static int  FindMovingFile (const std::vector<PascalKrunchMove> & files, int filesMoved);

private:
    static void  Fill (std::vector<Block> & ioBlocks, int first, int count, Cell cell, int file);
};

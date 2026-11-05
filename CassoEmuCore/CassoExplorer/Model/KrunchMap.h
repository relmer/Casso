#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/PascalVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap
//
//  A krunch drawn as a map of the volume's blocks, one cell a block, as a
//  defragmenter draws a disk: what is in each block at a point in the krunch,
//  from every file's place before and after, the parts moved, and the files
//  whose entries are rewritten.
//
//  A file moves a part at a time, as the program on the Apple moves it: the
//  blocks up to the end of a track at both places, or the blocks left. The
//  Apple sends Krunch progress once when it takes the request and once after
//  each part, so the parts done are the progress packets less one.
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
        Reading,    // a file being moved, at its old place
        Writing,    // and the parts of it written at its new place
    };

    //  A block's cell, the index in `files` of the file in it or -1, the
    //  character of the file's name drawn in it or 0, and R or W for a block
    //  of the part being read or written, which is drawn over the character.
    struct Block
    {
        Cell     cell     = Cell::Free;
        int      file     = -1;
        wchar_t  letter   = 0;
        wchar_t  activity = 0;
    };

    //  One part of a move: its file's index in `files`, and its blocks at
    //  both places.
    struct Part
    {
        int  file   = 0;
        int  from   = 0;
        int  to     = 0;
        int  blocks = 0;
    };

    static constexpr int  kColumns        = 40;
    static constexpr int  kBlocks         = 280;
    static constexpr int  kBlocksPerTrack = 8;

    //  The blocks, kBlocks of them, for `files` in directory order. A file
    //  moves when its `to` differs from its `from`. `partsDone` parts are
    //  written, and the next is being moved; `filesMoved` files have their
    //  entries rewritten. Until the Apple takes the request, `started` is
    //  false and no file is being moved.
    static std::vector<Block>  Build (const std::vector<PascalKrunchMove> & files, bool started, int partsDone, int filesMoved);

    //  The parts of every move, in the order they are moved.
    static std::vector<Part>  SplitIntoParts (const std::vector<PascalKrunchMove> & files);

    //  The blocks the krunch moves, and the files.
    static int  CountBlocksToMove (const std::vector<PascalKrunchMove> & files);
    static int  CountFilesToMove  (const std::vector<PascalKrunchMove> & files);

    //  The index in `files` of the file being moved, or -1 when none is.
    static int  FindMovingFile (const std::vector<PascalKrunchMove> & files, int partsDone);

private:
    static void  Fill      (std::vector<Block> & ioBlocks, int first, int count, Cell cell, int file);
    static void  Letter    (std::vector<Block> & ioBlocks, int first, int count, const std::string & name);
    static void  MarkBusy  (std::vector<Block> & ioBlocks, int first, int count, wchar_t activity);
};

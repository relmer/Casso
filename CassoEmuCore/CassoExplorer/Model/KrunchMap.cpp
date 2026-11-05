#include "Pch.h"

#include "CassoExplorer/Model/KrunchMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap::Build
//
//  Blocks the krunch has moved, by the percentage, are counted through the
//  moved files first, so the rest are the part of the file being moved
//  already written. A moved file's old place is free apart from where a new
//  place covers it, as new places are never past old ones.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<KrunchMap::Block> KrunchMap::Build (const std::vector<PascalKrunchMove> & files, bool started, int percent, int filesMoved)
{
    std::vector<Block>  blocks (kBlocks);
    int                 done      = CountBlocksToMove (files) * std::clamp (percent, 0, 100) / 100;
    int                 moveIndex = 0;
    int                 moved     = 0;



    Fill (blocks, 0, PascalVolume::kFirstDataBlock, Cell::System, -1);

    for (size_t i = 0; i < files.size(); i++)
    {
        const PascalKrunchMove  & file  = files[i];
        int                       index = (int) i;

        if (file.to == file.from)
        {
            Fill (blocks, file.to, file.blocks, Cell::InPlace, index);
            continue;
        }

        if (moveIndex < filesMoved)
        {
            Fill (blocks, file.to, file.blocks, Cell::InPlace, index);
            moved += file.blocks;
        }
        else if (started && moveIndex == filesMoved)
        {
            Fill (blocks, file.from, file.blocks,                               Cell::Reading, index);
            Fill (blocks, file.to,   std::clamp (done - moved, 0, file.blocks), Cell::Writing, index);
        }
        else
        {
            Fill (blocks, file.from, file.blocks, Cell::ToMove, index);
        }

        moveIndex++;
    }

    return blocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap::CountBlocksToMove
//
////////////////////////////////////////////////////////////////////////////////

int KrunchMap::CountBlocksToMove (const std::vector<PascalKrunchMove> & files)
{
    int  blocks = 0;



    for (const PascalKrunchMove & file : files)
    {
        if (file.to != file.from)
        {
            blocks += file.blocks;
        }
    }

    return blocks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap::CountFilesToMove
//
////////////////////////////////////////////////////////////////////////////////

int KrunchMap::CountFilesToMove (const std::vector<PascalKrunchMove> & files)
{
    return (int) std::count_if (files.begin(), files.end(), [] (const PascalKrunchMove & file) { return file.to != file.from; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap::FindMovingFile
//
////////////////////////////////////////////////////////////////////////////////

int KrunchMap::FindMovingFile (const std::vector<PascalKrunchMove> & files, int filesMoved)
{
    int  moveIndex = 0;
    int  found     = -1;



    for (size_t i = 0; i < files.size() && found < 0; i++)
    {
        if (files[i].to == files[i].from)
        {
            continue;
        }

        if (moveIndex == filesMoved)
        {
            found = (int) i;
        }

        moveIndex++;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap::Fill
//
//  Blocks past the map are left out.
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMap::Fill (std::vector<Block> & ioBlocks, int first, int count, Cell cell, int file)
{
    int  start = std::clamp (first,         0, (int) ioBlocks.size());
    int  end   = std::clamp (first + count, 0, (int) ioBlocks.size());



    std::fill (ioBlocks.begin() + start, ioBlocks.begin() + std::max (start, end), Block { cell, file });
}

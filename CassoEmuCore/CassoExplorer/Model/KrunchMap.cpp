#include "Pch.h"

#include "CassoExplorer/Model/KrunchMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap::Build
//
//  A file is being moved from its first part written until its entry is
//  rewritten: its old place is read, and its parts written so far and the
//  part in flight are at its new place. Its old place is free once its entry
//  is rewritten, apart from where a new place covers it, as new places are
//  never past old ones.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<KrunchMap::Block> KrunchMap::Build (const std::vector<PascalKrunchMove> & files, bool started, int partsDone, int filesMoved)
{
    std::vector<Block>  blocks (kBlocks);
    std::vector<Part>   parts     = SplitIntoParts (files);
    size_t              partIndex = 0;
    int                 moveIndex = 0;



    Fill (blocks, 0, PascalVolume::kFirstDataBlock, Cell::System, -1);

    for (size_t i = 0; i < files.size(); i++)
    {
        const PascalKrunchMove  & file    = files[i];
        int                       index   = (int) i;
        int                       first   = (int) partIndex;
        int                       written = 0;
        const Part              * busy    = nullptr;

        if (file.to == file.from)
        {
            Fill (blocks, file.to, file.blocks, Cell::InPlace, index);
            continue;
        }

        for ( ; partIndex < parts.size() && parts[partIndex].file == index; partIndex++)
        {
            if ((int) partIndex < partsDone)
            {
                written += parts[partIndex].blocks;
            }
            else if ((int) partIndex == partsDone)
            {
                busy = &parts[partIndex];
            }
        }

        if (moveIndex < filesMoved)
        {
            Fill (blocks, file.to, file.blocks, Cell::InPlace, index);
        }
        else if (started && first <= partsDone)
        {
            int  covered = written + ((busy != nullptr) ? busy->blocks : 0);

            Fill (blocks, file.from, file.blocks, Cell::Reading, index);
            Fill (blocks, file.to,   covered,     Cell::Writing, index);

            if (busy != nullptr)
            {
                MarkBusy (blocks, busy->from, busy->blocks, L'R');
                MarkBusy (blocks, busy->to,   busy->blocks, L'W');
            }
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
//  KrunchMap::SplitIntoParts
//
//  A part ends at the first track end at either place, as one read and one
//  write each cover a track at most.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<KrunchMap::Part> KrunchMap::SplitIntoParts (const std::vector<PascalKrunchMove> & files)
{
    std::vector<Part>  parts;



    for (size_t i = 0; i < files.size(); i++)
    {
        const PascalKrunchMove  & file = files[i];
        int                       done = 0;

        while (file.to != file.from && done < file.blocks)
        {
            int  from   = file.from + done;
            int  to     = file.to   + done;
            int  blocks = std::min ({ kBlocksPerTrack - from % kBlocksPerTrack, kBlocksPerTrack - to % kBlocksPerTrack, file.blocks - done });

            parts.push_back ({ (int) i, from, to, blocks });
            done += blocks;
        }
    }

    return parts;
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
//  The file of the part in flight; once every part is written, the last
//  file moved, until its entry is rewritten.
//
////////////////////////////////////////////////////////////////////////////////

int KrunchMap::FindMovingFile (const std::vector<PascalKrunchMove> & files, int partsDone)
{
    std::vector<Part>  parts = SplitIntoParts (files);
    int                found = -1;



    if (partsDone >= 0 && (size_t) partsDone < parts.size())
    {
        found = parts[(size_t) partsDone].file;
    }
    else if (!parts.empty() && partsDone >= 0)
    {
        found = parts.back().file;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap::Fill
//
//  Blocks past the map are left out. A filled block's activity is cleared.
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMap::Fill (std::vector<Block> & ioBlocks, int first, int count, Cell cell, int file)
{
    int  start = std::clamp (first,         0, (int) ioBlocks.size());
    int  end   = std::clamp (first + count, 0, (int) ioBlocks.size());



    std::fill (ioBlocks.begin() + start, ioBlocks.begin() + std::max (start, end), Block { cell, file, 0 });
}





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchMap::MarkBusy
//
////////////////////////////////////////////////////////////////////////////////

void KrunchMap::MarkBusy (std::vector<Block> & ioBlocks, int first, int count, wchar_t activity)
{
    for (int block = std::max (first, 0); block < first + count && (size_t) block < ioBlocks.size(); block++)
    {
        ioBlocks[(size_t) block].activity = activity;
    }
}

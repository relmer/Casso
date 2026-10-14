#pragma once

#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReadEdges
//
//  A device's read edges in its saved state: for each source the guest
//  reads, the value last reported, -1 before the first read. Each is a word,
//  0xFFFF for -1.
//
//  The edges are part of the state so a replay that loads a keyframe
//  reports the reads the running machine reported after it, and no others.
//  Every read updates them whether or not anything listens, so a machine
//  with listeners and one without save the same state.
//
////////////////////////////////////////////////////////////////////////////////

class ReadEdges
{
public:
    static constexpr Word  kNone = 0xFFFF;

    static void  Save (StateWriter & writer, std::span<const int> edges)
    {
        for (int edge : edges)
        {
            writer.WriteWord ((edge < 0) ? kNone : static_cast<Word> (edge));
        }
    }

    //  Reads each edge; false when one is neither a byte nor kNone, and
    //  the edges are then left as they were.
    static bool  Load (StateReader & reader, std::span<int> edges)
    {
        std::array<Word, 8>  read  = {};
        bool                 isOk  = edges.size() <= read.size();
        size_t               i     = 0;



        for (i = 0; isOk && i < edges.size(); i++)
        {
            reader.ReadWord (read[i]);
            isOk = read[i] <= 0xFF || read[i] == kNone;
        }

        for (i = 0; isOk && i < edges.size(); i++)
        {
            edges[i] = (read[i] == kNone) ? -1 : read[i];
        }

        return isOk;
    }
};

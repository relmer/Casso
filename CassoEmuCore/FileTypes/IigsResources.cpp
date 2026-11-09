#include "Pch.h"

#include "FileTypes/IigsResources.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IigsResources::ReadLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t IigsResources::ReadLong (std::span<const Byte> bytes, size_t at)
{
    return (uint32_t) bytes[at] | ((uint32_t) bytes[at + 1] << 8) | ((uint32_t) bytes[at + 2] << 16) | ((uint32_t) bytes[at + 3] << 24);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IigsResources::Find
//
//  The index starts at the map's offset to it, from the map's start.
//
////////////////////////////////////////////////////////////////////////////////

bool IigsResources::Find (std::span<const Byte> fork, Word type, uint32_t id, std::span<const Byte> & outResource)
{
    size_t  map   = 0;
    size_t  entry = 0;



    if (fork.size() < kMapAt + 4)
    {
        return false;
    }

    map = ReadLong (fork, kMapAt);

    if (map + kToIndex + 2 > fork.size())
    {
        return false;
    }

    entry = map + (size_t) (fork[map + kToIndex] | (fork[map + kToIndex + 1] << 8));

    while (entry + kEntryBytes <= fork.size())
    {
        Word    entryType = (Word) (fork[entry] | (fork[entry + 1] << 8));
        size_t  offset    = ReadLong (fork, entry + kEntryOffset);
        size_t  size      = ReadLong (fork, entry + kEntrySize);

        if (entryType == 0)
        {
            return false;
        }

        if (entryType == type && ReadLong (fork, entry + kEntryId) == id)
        {
            if (offset > fork.size() || size > fork.size() - offset)
            {
                return false;
            }

            outResource = fork.subspan (offset, size);
            return true;
        }

        entry += kEntryBytes;
    }

    return false;
}

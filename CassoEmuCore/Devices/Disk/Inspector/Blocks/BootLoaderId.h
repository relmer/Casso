#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockMap.h"

class BlockImage;





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId
//
//  The loader in a volume's boot blocks, from its first block: the ProDOS
//  loader by its first instructions, the SOS loader and its version by its
//  "SOS BOOT" text, Apple Pascal's by its "MUST BOOT FROM SLOT" message, and
//  on HFS, Macintosh boot blocks by their signature. A block of zeros has
//  no loader, and any other code is an unknown loader.
//
////////////////////////////////////////////////////////////////////////////////

class BootLoaderId
{
public:
    static std::wstring  Identify (const BlockImage & image, BlockFileSystem fileSystem);

private:
    static std::wstring  ReadText   (const BlockImage & image);
    static std::wstring  GetVersion (const std::wstring & text, size_t after);
};

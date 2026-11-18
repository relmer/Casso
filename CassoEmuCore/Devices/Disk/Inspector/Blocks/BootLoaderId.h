#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/Blocks/BlockMap.h"

class BlockImage;





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderId
//
//  What a volume's boot blocks start, from its first block: the SOS loader
//  and its version by its "SOS BOOT" text, Apple Pascal's by its "MUST BOOT
//  FROM SLOT" message, and on HFS, Macintosh boot blocks by their signature.
//  The ProDOS loader, found by its first instructions, loads the volume's
//  PRODOS file, which is ProDOS 8 with its version in its text, or on a
//  GS/OS disk a small loader beside System/GS.OS. A block of zeros has no
//  loader, and any other code is an unknown loader.
//
////////////////////////////////////////////////////////////////////////////////

class BootLoaderId
{
public:
    static constexpr size_t  kMaxReadBytes = 32768;

    static std::wstring  Identify (const BlockImage & image, const BlockMap & map);

private:
    static std::wstring  IdentifyProDos (const BlockImage & image, const BlockMap & map);
    static vector<Byte>  ReadFile       (const BlockImage & image, const BlockFileEntry & entry);
    static uint32_t      GetIndex       (const BlockImage & image, uint32_t indexBlock, size_t i);
    static std::wstring  ReadText       (const BlockImage & image, size_t length);
    static std::wstring  ToText         (const vector<Byte> & bytes);
    static std::wstring  ToUpper        (std::wstring text);
    static std::wstring  GetWord        (const std::wstring & text, size_t after);
};

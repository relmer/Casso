#pragma once

#include "Pch.h"

#include "NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeleton
//
//  Pure ProDOS volume structures for a fresh 143,360-byte sector buffer.
//  Write lays down the canonical minimal 280-block
//  volume: directory key block 2 chained through 5, volume bitmap at block 6
//  (blocks 0-6 used), boot blocks zeroed. InstallBoot copies boot blocks 0-1
//  from a caller-supplied ProDOS master image and writes PRODOS +
//  BASIC.SYSTEM as real files so the volume boots.
//
//  The buffer is kept in DOS 3.3 LOGICAL sector layout like every other
//  skeleton; ProDOS blocks map onto it through the standard block ->
//  track/sector pairing. Format ordering is applied later by the builder.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosSkeleton
{
public:
    static constexpr int  kTotalBlocks = 280;

    //  Empty formatted volume (data-only). Buffer must be kImageByteSize.
    static HRESULT  Write (vector<Byte> & buffer, const std::string & volumeName);

    //  Boot blocks + PRODOS + BASIC.SYSTEM from the master image.
    static HRESULT  InstallBoot (vector<Byte> & buffer, const vector<Byte> & usersDisk);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosReader
//
//  Minimal ProDOS volume reader: walks the volume directory and extracts one
//  named file's contents (seedling / sapling / tree). Exists to pull PRODOS
//  and BASIC.SYSTEM out of the downloaded master image.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosReader
{
public:
    static HRESULT  ExtractFile (const vector<Byte> & volume,
                                 const std::string  & fileName,
                                 vector<Byte>       & outBytes,
                                 Byte               & outFileType,
                                 Word               & outAuxType);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosFileWriter
//
//  Writes a file into a ProDosSkeleton-formatted buffer: directory entry,
//  seedling / sapling data blocks, volume bitmap kept honest.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosFileWriter
{
public:
    static HRESULT  WriteFile (vector<Byte>       & buffer,
                               const std::string  & fileName,
                               Byte                 fileType,
                               Word                 auxType,
                               const vector<Byte> & bytes);
};

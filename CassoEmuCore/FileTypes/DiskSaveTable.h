#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable
//
//  The games whose saved game is a whole disk with no file system, such as
//  an Infocom save disk. Each entry is a test of a disk's sectors, in DOS
//  logical order, and a decoder from those sectors to a document. The
//  decoder also takes a flag per sector, true for one that could not be
//  read and is zeros in the buffer, or no flags when none are known, as for
//  a sector image. A disk that a test accepts previews as its saved game in
//  Casso Explorer.
//
////////////////////////////////////////////////////////////////////////////////

class DiskSaveTable
{
public:
    using IsFoundFn = bool    (*) (const std::vector<Byte> & sectors);
    using DecodeFn  = HRESULT (*) (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);

    struct Entry
    {
        IsFoundFn  isFound = nullptr;
        DecodeFn   decode  = nullptr;
    };

    //  The first entry whose test accepts the sectors, or nullptr.
    static const Entry *  Find (const std::vector<Byte> & sectors);

    static bool     IsInfocomSave     (const std::vector<Byte> & sectors);
    static HRESULT  DecodeInfocomSave (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
    static bool     IsUltima3Save     (const std::vector<Byte> & sectors);
    static HRESULT  DecodeUltima3Save (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
};

#pragma once

#include "Pch.h"
#include "Machines/Apple2/Common/InfocomSave.h"

#include "FileTypes/DecodedFile.h"

class IDiskFileIo;





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
//  Casso Explorer. An entry may also test for a companion disk, such as the
//  game disk that has the map a player disk refers to, and decode the two
//  together when one is found beside it.
//
////////////////////////////////////////////////////////////////////////////////

class DiskSaveTable
{
public:
    using IsFoundFn         = bool    (*) (const std::vector<Byte> & sectors);
    using DecodeFn          = HRESULT (*) (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
    using CompanionDecodeFn = HRESULT (*) (const std::vector<Byte> & sectors, const std::vector<Byte> & companion, DecodedFile & outFile);
    using FindCompanionFn   = std::function<bool (IsFoundFn isCompanion, std::vector<Byte> & outSectors)>;

    struct Entry
    {
        IsFoundFn          isFound             = nullptr;
        DecodeFn           decode              = nullptr;
        IsFoundFn          isCompanion         = nullptr;
        CompanionDecodeFn  decodeWithCompanion = nullptr;
    };

    //  The first entry whose test accepts the sectors, or nullptr.
    static const Entry *  Find (const std::vector<Byte> & sectors);

    //  A disk image file's sectors decoded by the entry that accepts them.
    static HRESULT  DecodeImage (const std::vector<Byte> & bytes, const std::string & path, const FindCompanionFn & findCompanion, DecodedFile & outFile);

    //  The first disk image in the same folder as path that isCompanion accepts.
    static bool     FindBeside  (IDiskFileIo & io, const std::string & path, IsFoundFn isCompanion, std::vector<Byte> & outSectors);

    static bool     IsInfocomSave        (const std::vector<Byte> & sectors);
    static HRESULT  DecodeInfocomSave    (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
    static bool     IsUltima3Save        (const std::vector<Byte> & sectors);
    static HRESULT  DecodeUltima3Save    (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
    static bool     IsUltima4Save        (const std::vector<Byte> & sectors);
    static HRESULT  DecodeUltima4Save    (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
    static bool     IsUltima5Save        (const std::vector<Byte> & sectors);
    static HRESULT  DecodeUltima5Save    (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
    static bool     IsUltima1Save        (const std::vector<Byte> & sectors);
    static HRESULT  DecodeUltima1Save    (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
    static HRESULT  DecodeUltima1WithMap (const std::vector<Byte> & sectors, const std::vector<Byte> & game, DecodedFile & outFile);
    static bool     IsUltima1GameDisk    (const std::vector<Byte> & sectors);
    static HRESULT  DecodeUltima1Game    (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
    static bool     IsUltima1981Save     (const std::vector<Byte> & sectors);
    static HRESULT  DecodeUltima1981Save (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);
    static bool     IsUltima2Save        (const std::vector<Byte> & sectors);
    static HRESULT  DecodeUltima2Save    (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile);

private:
    static constexpr int       s_kScoreBarCells  = 20;
    static constexpr size_t    s_kItemIndent     = 4;

    static std::string  GetInfocomTitle    (const std::vector<InfocomSave::Position> & positions);
    static void         AddInfocomPosition (const InfocomSave::Position & position, std::vector<Paragraph> & ioParagraphs);
    static void         AddInfocomItems    (const wchar_t * heading, const std::vector<InfocomSave::Item> & items, std::vector<Paragraph> & ioParagraphs);
    static void         AddScoreBar        (int score, int maxScore, std::vector<Run> & ioRuns);

    static bool     TryLoadImage         (IDiskFileIo & io, const std::string & path, std::vector<Byte> & outSectors);
};

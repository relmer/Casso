#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map
//
//  The world map of Ultima I: The Beginning, from the game disk's file GEN.
//  The world is four continents of 64 by 64 tiles. Each is run-length coded:
//  a byte's top five bits are a count and its low three bits a tile, and a
//  zero byte ends the continent. A table of four words at kTableAt has
//  each continent's start, counted from the table. The format was found by watching
//  the game unpack a continent into memory at $6000.
//
////////////////////////////////////////////////////////////////////////////////

class Ultima1Map
{
public:
    static constexpr int     kContinents = 4;
    static constexpr int     kMapSide    = 64;
    static constexpr size_t  kTableAt    = 2794;

    using Continent = std::vector<Byte>;

    static bool          IsGameDisk       (const std::vector<Byte> & sectors);
    static bool          TryReadGameDisk  (const std::vector<Byte> & sectors, std::array<Continent, kContinents> & outContinents);
    static bool          TryUnpack        (std::span<const Byte> gen, std::array<Continent, kContinents> & outContinents);
    static HRESULT       DecodeGameDisk   (const std::vector<Byte> & sectors, DecodedFile & outFile);
    static void          RenderMap        (const Continent & tiles, int partyX, int partyY, Paragraph & ioMap);
    static void          AddLegend        (std::vector<Paragraph> & ioParagraphs, bool hasPlayer);
    static std::wstring  GetContinentName (int continent);
    static std::wstring  GetTileName      (Byte tile);
    static uint32_t      GetTileColor     (Byte tile);

private:
    static Paragraph     MakeLine         (const std::wstring & text, bool isBold);
};

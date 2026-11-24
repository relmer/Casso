#include "Pch.h"

#include "Machines/Apple2/Common/Ultima1Map.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





//  The tiles, with the color the map draws each in and its name in a tip.
//  Each continent has two castles, eight towns, two signposts and nine
//  dungeons, and the castle next to the player's start was checked against
//  the game's screen.
static constexpr struct { uint32_t argb; const wchar_t * pszName; } s_kTiles[] =
{
    { 0xFF2846AA, L"Water"     },
    { 0xFF5AAA46, L"Grass"     },
    { 0xFF1E692D, L"Forest"    },
    { 0xFF7D7369, L"Mountains" },
    { 0xFFFFFFFF, L"Castle"    },
    { 0xFFFFD200, L"Signpost"  },
    { 0xFFDC50DC, L"Town"      },
    { 0xFFDC3C3C, L"Dungeon"   },
};

static constexpr const wchar_t * s_kContinents[] = { L"The Lands of Lord British", L"The Lands of the Feudal Lords",
                                                     L"The Lands of the Dark Unknown", L"The Lands of Danger and Despair" };

static constexpr uint32_t  s_kPartyMark  = 0xFFFFFFFF;
static constexpr uint32_t  s_kPartyEdge  = 0xFF000000;
static constexpr int       s_kTilePixels = 6;





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::TryUnpack
//
//  Each continent from its run-length code. False when the table points
//  outside the file, or a continent doesn't come to exactly 64 by 64 tiles
//  before its zero byte.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1Map::TryUnpack (std::span<const Byte> gen, std::array<Continent, kContinents> & outContinents)
{
    constexpr size_t  kTiles     = (size_t) kMapSide * kMapSide;
    constexpr int     kCountBits = 3;
    constexpr Byte    kTileMask  = 0x07;



    if (gen.size() < kTableAt + kContinents * 2)
    {
        return false;
    }

    for (int k = 0; k < kContinents; k++)
    {
        size_t       at        = kTableAt + (size_t) (gen[kTableAt + (size_t) k * 2] | gen[kTableAt + (size_t) k * 2 + 1] << 8);
        Continent  & continent = outContinents[(size_t) k];

        continent.clear();

        while (at < gen.size() && gen[at] != 0 && continent.size() <= kTiles)
        {
            continent.insert (continent.end(), (size_t) (gen[at] >> kCountBits), (Byte) (gen[at] & kTileMask));
            at++;
        }

        if (at >= gen.size() || continent.size() != kTiles)
        {
            return false;
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::TryReadGameDisk
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1Map::TryReadGameDisk (const std::vector<Byte> & sectors, std::array<Continent, kContinents> & outContinents)
{
    ProDosVolume  volume (sectors);
    FilePayload   gen;
    HRESULT       hr     = volume.Read (FilePath::Parse ("GEN"), gen);



    return SUCCEEDED (hr) && TryUnpack (gen.bytes, outContinents);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::IsGameDisk
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1Map::IsGameDisk (const std::vector<Byte> & sectors)
{
    std::array<Continent, kContinents>  continents;



    return TryReadGameDisk (sectors, continents);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::GetContinentName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima1Map::GetContinentName (int continent)
{
    return (continent >= 0 && continent < kContinents) ? s_kContinents[continent] : std::format (L"Continent {}", continent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::GetTileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima1Map::GetTileName (Byte tile)
{
    return tile < std::size (s_kTiles) ? s_kTiles[tile].pszName : std::format (L"Tile {}", tile);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::GetTileColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Ultima1Map::GetTileColor (Byte tile)
{
    return tile < std::size (s_kTiles) ? s_kTiles[tile].argb : s_kPartyEdge;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::RenderMap
//
//  Each tile s_kTilePixels square in its color, with a tip of its column,
//  row and name. The player, when partyX isn't negative, is a white square
//  with a black edge and a ping.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima1Map::RenderMap (const Continent & tiles, int partyX, int partyY, Paragraph & ioMap)
{
    int   side     = kMapSide * s_kTilePixels;
    bool  hasParty = partyX >= 0 && partyY >= 0;



    ioMap.width      = side;
    ioMap.height     = side;
    ioMap.tipCell    = s_kTilePixels;
    ioMap.tipLeft    = 0;
    ioMap.tipTop     = 0;
    ioMap.tipColumns = kMapSide;
    ioMap.pingX      = hasParty ? partyX * s_kTilePixels + s_kTilePixels / 2 : -1;
    ioMap.pingY      = hasParty ? partyY * s_kTilePixels + s_kTilePixels / 2 : -1;
    ioMap.canZoom    = true;
    ioMap.picture.assign ((size_t) side * (size_t) side, s_kPartyEdge);
    ioMap.tips.assign ((size_t) kMapSide * (size_t) kMapSide, PictureTip());

    for (int y = 0; y < kMapSide; y++)
    {
        for (int x = 0; x < kMapSide; x++)
        {
            Byte          tile    = tiles[(size_t) y * kMapSide + (size_t) x];
            bool          isParty = hasParty && x == partyX && y == partyY;
            PictureTip  & tip     = ioMap.tips[(size_t) y * kMapSide + (size_t) x];

            for (int py = 0; py < s_kTilePixels; py++)
            {
                for (int px = 0; px < s_kTilePixels; px++)
                {
                    bool      isEdge = py == 0 || px == 0 || py == s_kTilePixels - 1 || px == s_kTilePixels - 1;
                    uint32_t  color  = isParty ? (isEdge ? s_kPartyEdge : s_kPartyMark) : GetTileColor (tile);

                    ioMap.picture[(size_t) (y * s_kTilePixels + py) * (size_t) side + (size_t) (x * s_kTilePixels + px)] = color;
                }
            }

            tip.lines.push_back ({ 0, std::format (L"Column {}, row {}", x, y) });

            if (isParty)
            {
                tip.lines.push_back ({ s_kPartyMark, L"The player" });
            }

            tip.lines.push_back ({ GetTileColor (tile), GetTileName (tile) });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::AddLegend
//
////////////////////////////////////////////////////////////////////////////////

void Ultima1Map::AddLegend (std::vector<Paragraph> & ioParagraphs, bool hasPlayer)
{
    Paragraph  legend;



    if (hasPlayer)
    {
        legend.runs.push_back (Run { L"Player", false, false, false, RunColor::Muted, s_kPartyMark });
    }

    for (const auto & known : s_kTiles)
    {
        legend.runs.push_back (Run { known.pszName, false, false, false, RunColor::Muted, known.argb });
    }

    legend.isLegend = true;
    ioParagraphs.push_back (std::move (legend));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::MakeLine
//
//  A paragraph of one run, bold or muted.
//
////////////////////////////////////////////////////////////////////////////////

Paragraph Ultima1Map::MakeLine (const std::wstring & text, bool isBold)
{
    Paragraph  line;



    line.runs.push_back (isBold ? Run { text, true } : Run { text, false, false, false, RunColor::Muted });

    return line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Map::DecodeGameDisk
//
//  A heading, each continent by name and its map, then the legend.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ultima1Map::DecodeGameDisk (const std::vector<Byte> & sectors, DecodedFile & outFile)
{
    HRESULT                             hr         = S_OK;
    std::array<Continent, kContinents>  continents;
    bool                                isRead     = false;



    isRead = TryReadGameDisk (sectors, continents);
    CBREx (isRead, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outFile        = DecodedFile();
    outFile.format   = FileFormat::UltimaSave;
    outFile.kind     = DecodedFile::Kind::Document;
    outFile.viewName = L"Map";

    outFile.paragraphs.push_back (MakeLine (L"Ultima I: The Beginning", true));
    outFile.paragraphs.push_back (MakeLine (L"Game disk, the world's four continents", false));

    for (int k = 0; k < kContinents; k++)
    {
        Paragraph  map;

        RenderMap (continents[(size_t) k], -1, -1, map);

        outFile.paragraphs.push_back (Paragraph());
        outFile.paragraphs.push_back (MakeLine (GetContinentName (k), true));
        outFile.paragraphs.push_back (std::move (map));
    }

    AddLegend (outFile.paragraphs, false);

Error:
    return hr;
}

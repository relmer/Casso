#include "Pch.h"

#include "FileTypes/WizardryMaze.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryMaze::ReadSquare
//
////////////////////////////////////////////////////////////////////////////////

WizardryMaze::Square WizardryMaze::ReadSquare (int word)
{
    Square  square;
    int     block = (word >> 6) & 0xF;



    square.x     = (word & 7) | ((block % kBlocksAcross) << 3);
    square.y     = ((word >> 3) & 7) | ((block / kBlocksAcross) << 3);
    square.level = (word >> 10) & 0x3F;

    return square;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryMaze::GetSide
//
//  The disk side number at the end of block 1, or 0 for a disk too small to
//  have one.
//
////////////////////////////////////////////////////////////////////////////////

int WizardryMaze::GetSide (std::span<const Byte> sectors)
{
    size_t  low  = 0;
    size_t  high = 0;



    if (sectors.size() < (size_t) (kWallsBlock + 1) * kWallsBytes)
    {
        return 0;
    }

    low  = ProDosSkeleton::GetBlockByteOffset (sectors.size(), kSideBlock, kSideAt);
    high = ProDosSkeleton::GetBlockByteOffset (sectors.size(), kSideBlock, kSideAt + 1);

    return sectors[low] | (sectors[high] << 8);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryMaze::FindWalls
//
//  The walls block of a level, from the side that has it: level 1 is on
//  side 2.
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryMaze::FindWalls (std::span<const std::vector<Byte>> sides, int level, std::vector<Byte> & outWalls)
{
    auto  found = std::find_if (sides.begin(), sides.end(), [level] (const std::vector<Byte> & side) { return GetSide (side) == level + 1; });



    outWalls.clear();

    if (level < 1 || found == sides.end())
    {
        return false;
    }

    for (size_t i = 0; i < kWallsBytes; i++)
    {
        outWalls.push_back ((*found)[ProDosSkeleton::GetBlockByteOffset (found->size(), kWallsBlock, i)]);
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryMaze::GetWall
//
//  The east or north side of a square, which wraps at the edges.
//
////////////////////////////////////////////////////////////////////////////////

int WizardryMaze::GetWall (std::span<const Byte> walls, int x, int y, bool north)
{
    int     column = ((x % kSide) + kSide) % kSide;
    int     row    = ((y % kSide) + kSide) % kSide;
    size_t  at     = (north ? kNorthAt : 0) + 2 * (size_t) ((row / kRowsPerWord) * kSide + column);
    int     word   = walls[at] | (walls[at + 1] << 8);



    return (word >> (kWallBits * (row % kRowsPerWord))) & 3;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryMaze::FindSides
//
//  The maze sides among the disk images in the folder of a saved game's
//  image, as sectors: files with its extension, small enough to be a disk
//  side, whose side number is a maze side's.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryMaze::FindSides (IDiskFileIo & fileIo, const std::string & imagePath, std::vector<std::vector<Byte>> & outSides)
{
    std::filesystem::path     image     = imagePath;
    std::string               folder    = image.parent_path().string();
    std::vector<std::string>  names;
    HRESULT                   hr        = fileIo.ListFiles (folder, names);



    outSides.clear();

    if (FAILED (hr))
    {
        return;
    }

    for (const std::string & name : names)
    {
        std::string        path    = (std::filesystem::path (folder) / name).string();
        std::vector<Byte>  sectors;
        bool               wanted  = _stricmp (std::filesystem::path (name).extension().string().c_str(), image.extension().string().c_str()) == 0
                                  && _stricmp (path.c_str(), imagePath.c_str()) != 0;

        if (wanted && LoadSide (fileIo, path, sectors))
        {
            outSides.push_back (std::move (sectors));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryMaze::LoadSaveDisks
//
//  The sectors of a saved game's image, for the item text on it, and the
//  maze sides in its folder. outSectors is empty when the image can't be
//  read.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryMaze::LoadSaveDisks (IDiskFileIo & fileIo, const std::string & imagePath, std::vector<Byte> & outSectors, std::vector<std::vector<Byte>> & outSides)
{
    std::vector<Byte>   bytes;
    SectorDecodeReport  report;
    HRESULT             hr     = fileIo.ReadAllBytes (imagePath, bytes);



    outSectors.clear();

    if (SUCCEEDED (hr))
    {
        hr = VolumeImage::Load (bytes, imagePath, outSectors, report);
    }

    if (FAILED (hr))
    {
        outSectors.clear();
    }

    FindSides (fileIo, imagePath, outSides);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryMaze::LoadSide
//
//  A file's sectors, when it is small enough to be a disk side, loads as a
//  disk image, and has a maze side's number.
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryMaze::LoadSide (IDiskFileIo & fileIo, const std::string & path, std::vector<Byte> & outSectors)
{
    FileStamp           stamp;
    std::vector<Byte>   bytes;
    SectorDecodeReport  report;
    int                 side   = 0;
    HRESULT             hr     = fileIo.Stat (path, stamp);
    bool                loaded = SUCCEEDED (hr) && stamp.sizeBytes <= kMaxSideBytes;



    if (loaded)
    {
        hr     = fileIo.ReadAllBytes (path, bytes);
        loaded = SUCCEEDED (hr);
    }

    if (loaded)
    {
        hr     = VolumeImage::Load (bytes, path, outSectors, report);
        loaded = SUCCEEDED (hr);
    }

    if (loaded)
    {
        side   = GetSide (outSectors);
        loaded = side >= kFirstMazeSide && side <= kLastMazeSide;
    }

    return loaded;
}

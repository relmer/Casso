#include "Pch.h"

#include "Machines/Apple2/Common/StockDisk800K.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StockDisk800K::Build
//
//  All or nothing: on failure `outVolume` is empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StockDisk800K::Build (const vector<Byte> & source, vector<Byte> & outVolume)
{
    HRESULT       hr     = S_OK;
    vector<Byte>  dosOrder;
    vector<Byte>  volume (kBytes, 0);
    std::string   name;
    bool          named  = false;
    bool          sized  = source.size() == (size_t) NibblizationLayer::kImageByteSize;
    size_t        i      = 0;



    outVolume.clear();

    CBRAEx (sized, E_INVALIDARG);

    ToDosOrder (source, dosOrder);

    named = ProDosSkeleton::TryGetVolumeName (dosOrder, name);
    CBREx (named, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ProDosSkeleton::Write (volume, name);
    CHR (hr);

    //  Blocks 0 and 1, the boot blocks, which the new volume marks as used.
    for (i = 0; i < 2 * (size_t) ProDosSkeleton::kBlockByteSize; i++)
    {
        volume[i] = source[i];
    }

    hr = CopyDirectory (ProDosVolume (dosOrder), std::string(), volume);
    CHR (hr);

    outVolume = std::move (volume);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockDisk800K::CopyDirectory
//
//  Each entry of the directory at `directory` ("" for the volume's), in
//  directory order, so the disk starts the same system file the source
//  starts. A folder is made, then filled.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StockDisk800K::CopyDirectory (const ProDosVolume & from, const std::string & directory, vector<Byte> & ioVolume)
{
    HRESULT        hr    = S_OK;
    VolumeListing  listing;
    bool           clean = false;



    hr = directory.empty() ? from.Enumerate (listing) : from.EnumerateDirectory (FilePath::Parse (directory), listing);
    CHR (hr);

    clean = listing.damage.empty();
    CBREx (clean, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (const FileEntry & entry : listing.entries)
    {
        std::string   path = directory.empty() ? entry.name : directory + "/" + entry.name;
        FilePayload   payload;
        vector<Byte>  written;



        if (entry.isDirectory)
        {
            hr = ProDosVolume (ioVolume).CreateDirectory (FilePath::Parse (path), written);
            CHR (hr);

            ioVolume = std::move (written);

            hr = CopyDirectory (from, path, ioVolume);
            CHR (hr);

            continue;
        }

        hr = from.Read (FilePath::Parse (path), payload);
        CHR (hr);

        hr = ProDosVolume (ioVolume).Write (FilePath::Parse (path), payload, written);
        CHR (hr);

        ioVolume = std::move (written);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockDisk800K::ToDosOrder
//
//  Block b of a ProDOS-order disk is at b * 512, and ProDosSkeleton finds
//  each byte of it in a DOS-order disk.
//
////////////////////////////////////////////////////////////////////////////////

void StockDisk800K::ToDosOrder (const vector<Byte> & proDosOrder, vector<Byte> & outDosOrder)
{
    int     blocks = (int) (proDosOrder.size() / ProDosSkeleton::kBlockByteSize);
    size_t  i      = 0;



    outDosOrder.assign (proDosOrder.size(), 0);

    for (int block = 0; block < blocks; block++)
    {
        for (i = 0; i < (size_t) ProDosSkeleton::kBlockByteSize; i++)
        {
            outDosOrder[ProDosSkeleton::GetBlockByteOffset (block, i)] = proDosOrder[(size_t) block * ProDosSkeleton::kBlockByteSize + i];
        }
    }
}

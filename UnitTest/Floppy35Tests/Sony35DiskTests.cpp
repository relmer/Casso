#include "Pch.h"

#include "Devices/BlockDevice/IReplayMedia.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Devices/Floppy35/Sony35BlockDisk.h"
#include "Devices/Floppy35/Sony35WozDisk.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "../EmuTests/Woz35Builder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Sony35DiskTests
//
//  The two disks a Sony35Drive takes: a block image, its tracks encoded from
//  its blocks and decoded back on Flush, and a WOZ image, its tracks its bit
//  streams.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Sony35DiskTests)
{
public:

    static constexpr const char *  kPath = "d.img";


    static std::vector<Byte> MakeBlocks (uint32_t count)
    {
        std::vector<Byte>  bytes ((size_t) count * Gcr35Track::kDataBytes, 0);



        for (size_t i = 0; i < bytes.size(); i++)
        {
            bytes[i] = (Byte) (i / Gcr35Track::kDataBytes);
        }

        return bytes;
    }


    static void AddFile (FakeDiskFileIo & io, const std::vector<Byte> & bytes)
    {
        io.files[kPath]  = bytes;
        io.stamps[kPath] = FileStamp { bytes.size(), 1 };
    }



    //  800K is two-sided, 400K one-sided, and any other size fails.
    TEST_METHOD (Block_SidesFromSize)
    {
        FakeDiskFileIo   io;
        Sony35BlockDisk  twoSided;
        Sony35BlockDisk  oneSided;
        Sony35BlockDisk  odd;
        std::wstring     error;



        AddFile (io, MakeBlocks (Gcr35Track::kBlocksTwoSides));
        Assert::AreEqual (S_OK, twoSided.Attach (io, kPath, {}, error));
        Assert::AreEqual (2, twoSided.GetSides());
        Assert::AreEqual (S_OK, twoSided.Detach());

        AddFile (io, MakeBlocks (Gcr35Track::kBlocksOneSide));
        Assert::AreEqual (S_OK, oneSided.Attach (io, kPath, {}, error));
        Assert::AreEqual (1, oneSided.GetSides());
        Assert::AreEqual (S_OK, oneSided.Detach());

        AddFile (io, MakeBlocks (1000));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT), odd.Attach (io, kPath, {}, error));
        Assert::IsFalse  (error.empty());
    }


    //  A track built from the blocks decodes back to them, and Flush writes a
    //  written track's sectors to the file.
    TEST_METHOD (Block_TrackRoundTripsThroughFlush)
    {
        FakeDiskFileIo              io;
        Sony35BlockDisk             disk;
        ISony35Disk::Tracks         tracks;
        std::set<int>               dirty;
        std::vector<Byte>           data;
        std::vector<bool>           found;
        std::wstring                error;



        AddFile (io, MakeBlocks (Gcr35Track::kBlocksTwoSides));
        Assert::AreEqual (S_OK, disk.Attach (io, kPath, {}, error));

        disk.ReadTrack (0, 1, tracks[1]);
        Gcr35Track::DecodeTrack (0, 1, tracks[1], data, found);
        Assert::IsTrue   (found[0]);
        Assert::AreEqual ((Byte) Gcr35Track::GetFirstBlock (0, 1, 2), data[0], L"side 1's first block");

        Gcr35Track::BuildTrack (0, 1, Gcr35Track::kFormatTwoSides, std::vector<Byte> (data.size(), 0xAA), tracks[1]);
        dirty.insert (1);

        Assert::AreEqual (S_OK, disk.Flush (tracks, dirty));
        Assert::IsTrue   (dirty.empty());
        Assert::AreEqual ((Byte) 0xAA, io.files[kPath][(size_t) Gcr35Track::GetFirstBlock (0, 1, 2) * Gcr35Track::kDataBytes]);
    }


    //  A track the TMAP has is its bits; an empty entry is a blank track of
    //  the zone's length.
    TEST_METHOD (Woz_TracksFromTheImage)
    {
        std::vector<Byte>  woz = Woz35Builder::Build (MakeBlocks (Gcr35Track::kBlocksTwoSides), { 3 });
        Sony35WozDisk      disk;
        std::vector<Byte>  bits;
        std::vector<Byte>  data;
        std::vector<bool>  found;
        std::wstring       error;



        Assert::AreEqual (S_OK, disk.Load (nullptr, kPath, woz, {}, error));
        Assert::AreEqual (2, disk.GetSides());
        Assert::IsTrue   (disk.IsWoz());

        disk.ReadTrack (0, 0, bits);
        Gcr35Track::DecodeTrack (0, 0, bits, data, found);
        Assert::IsTrue   (found[0]);

        disk.ReadTrack (1, 1, bits);
        Assert::AreEqual (Gcr35Track::GetTrackBits (1), bits.size());
        Assert::IsTrue   (std::all_of (bits.begin(), bits.end(), [] (Byte bit) { return bit == 0; }));
    }


    //  A replay's copy of a WOZ disk is the file's bytes, marked as WOZ, and
    //  a flush of the copy doesn't write a file.
    TEST_METHOD (Woz_ReplayCopyWritesNoFile)
    {
        std::vector<Byte>    woz   = Woz35Builder::Build (MakeBlocks (Gcr35Track::kBlocksTwoSides));
        Sony35WozDisk        disk;
        ReplayBlockMedia     copy;
        ISony35Disk::Tracks  tracks;
        std::set<int>        dirty = { 0 };
        std::wstring         error;



        Assert::AreEqual (S_OK, disk.Load (nullptr, kPath, woz, { .imageFlag = true }, error));
        Assert::IsTrue   (disk.IsWriteProtected());

        Assert::AreEqual (S_OK, disk.CopyForReplay (copy));
        Assert::IsTrue   (copy.isWoz);
        Assert::IsTrue   (copy.protect.imageFlag);
        Assert::IsTrue   (*copy.content == woz);

        disk.ReadTrack (0, 0, tracks[0]);
        Assert::AreEqual (S_OK, disk.Flush (tracks, dirty));
        Assert::IsTrue   (dirty.empty());
    }
};

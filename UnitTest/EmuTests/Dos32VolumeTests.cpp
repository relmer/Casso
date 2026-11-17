#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ThirteenSectorCodec.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Dos32VolumeTests
//
//  DOS 3.x volumes of geometries other than 35 tracks of 16 sectors: the
//  bitmap layout of each, detection from the VTOC, and files written to and
//  read from a blank 13-sector volume.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Dos32VolumeTests)
{
public:

    static constexpr Byte      kVolumeNumber      = 254;
    static constexpr size_t    kVtocOffBitmap     = 0x38;
    static constexpr size_t    kVtocOffRelease    = 0x03;
    static constexpr int       kVtocTrack         = 17;

    //  Tracks 0 to 2 and track 17 are in use on a blank volume.
    static constexpr uint32_t  kFreeOnBlankVolume = 31 * 13;

    static vector<Byte> MakeBlankVolume (const Dos33Geometry & geometry)
    {
        vector<Byte>  buffer (geometry.GetImageByteSize(), 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kVolumeNumber, geometry));

        return buffer;
    }

    static FilePayload MakeBinaryPayload (size_t length, Word loadAddress)
    {
        FilePayload  payload;
        size_t       i = 0;

        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = loadAddress;
        payload.hasLoadAddress = true;

        payload.bytes.resize (length);

        for (i = 0; i < length; i++)
        {
            payload.bytes[i] = (Byte) ((i * 3 + i / 256) & 0xFF);
        }

        return payload;
    }


    TEST_METHOD (Geometry_SixteenSectors_BitmapMatchesDos33)
    {
        Dos33Geometry  geometry;
        Byte           freeBytes[4] = {};
        size_t         base         = geometry.GetSectorOffset (kVtocTrack, 0) + kVtocOffBitmap + (size_t) 5 * 4;

        geometry.GetFreeTrackBytes (freeBytes);

        Assert::AreEqual (0xFF, (int) freeBytes[0]);
        Assert::AreEqual (0xFF, (int) freeBytes[1]);
        Assert::AreEqual (0x00, (int) freeBytes[2]);
        Assert::AreEqual (0x00, (int) freeBytes[3]);

        //  Sectors 15 to 8 are bits 7 to 0 of the first byte, and sectors 7
        //  to 0 are the second byte.
        Assert::AreEqual (base,     geometry.GetBitmapByteOffset (5, 15));
        Assert::AreEqual (0x80,     (int) geometry.GetBitmapMask (15));
        Assert::AreEqual (base,     geometry.GetBitmapByteOffset (5, 8));
        Assert::AreEqual (0x01,     (int) geometry.GetBitmapMask (8));
        Assert::AreEqual (base + 1, geometry.GetBitmapByteOffset (5, 7));
        Assert::AreEqual (0x80,     (int) geometry.GetBitmapMask (7));
        Assert::AreEqual (base + 1, geometry.GetBitmapByteOffset (5, 0));
        Assert::AreEqual (0x01,     (int) geometry.GetBitmapMask (0));
    }


    TEST_METHOD (Geometry_ThirteenSectors_LastSectorIsTheHighBit)
    {
        Dos33Geometry  geometry     = Dos33Geometry::Make (35, 13);
        Byte           freeBytes[4] = {};
        size_t         base         = geometry.GetSectorOffset (kVtocTrack, 0) + kVtocOffBitmap;

        geometry.GetFreeTrackBytes (freeBytes);

        Assert::AreEqual (0xFF, (int) freeBytes[0]);
        Assert::AreEqual (0xF8, (int) freeBytes[1]);
        Assert::AreEqual (0x00, (int) freeBytes[2]);
        Assert::AreEqual (0x00, (int) freeBytes[3]);

        Assert::AreEqual (base,     geometry.GetBitmapByteOffset (0, 12));
        Assert::AreEqual (0x80,     (int) geometry.GetBitmapMask (12));
        Assert::AreEqual (base + 1, geometry.GetBitmapByteOffset (0, 0));
        Assert::AreEqual (0x08,     (int) geometry.GetBitmapMask (0));
        Assert::AreEqual (ThirteenSectorCodec::kImageByteSize, geometry.GetImageByteSize());
    }


    TEST_METHOD (Geometry_EightyTracks_HasTwoBitmapBytesPerTrack)
    {
        Dos33Geometry  geometry     = Dos33Geometry::Make (80, 16);
        Byte           freeBytes[4] = {};
        size_t         base         = geometry.GetSectorOffset (kVtocTrack, 0) + kVtocOffBitmap + (size_t) 79 * 2;

        geometry.GetFreeTrackBytes (freeBytes);

        Assert::AreEqual (2,    geometry.bitmapBytesPerTrack);
        Assert::AreEqual (0xFF, (int) freeBytes[0]);
        Assert::AreEqual (0xFF, (int) freeBytes[1]);
        Assert::AreEqual (0x00, (int) freeBytes[2]);
        Assert::AreEqual (base,     geometry.GetBitmapByteOffset (79, 15));
        Assert::AreEqual (base + 1, geometry.GetBitmapByteOffset (79, 0));
    }


    TEST_METHOD (TryDetect_BlankVolumeOfEachGeometry_ReturnsThatGeometry)
    {
        static constexpr int  kCounts[][2] = { { 35, 16 }, { 35, 13 }, { 40, 16 }, { 50, 16 }, { 50, 32 }, { 80, 16 } };

        for (const int * counts : kCounts)
        {
            Dos33Geometry  geometry = Dos33Geometry::Make (counts[0], counts[1]);
            vector<Byte>   buffer   = MakeBlankVolume (geometry);
            Dos33Geometry  found;

            Assert::IsTrue (Dos33Geometry::TryDetect (buffer, found));
            Assert::IsTrue (found == geometry);
            Assert::IsTrue (VolumeImage::DetectFilesystem (buffer) == VolumeKind::Dos33);
        }
    }


    TEST_METHOD (TryDetect_BufferOfZeros_IsNotFound)
    {
        vector<Byte>   buffer (ThirteenSectorCodec::kImageByteSize, 0);
        Dos33Geometry  found;

        Assert::IsFalse (Dos33Geometry::TryDetect (buffer, found));
        Assert::IsTrue (VolumeImage::DetectFilesystem (buffer) == VolumeKind::Unknown);
    }


    TEST_METHOD (BlankThirteenSectorVolume_HasDos32Release_AndItsFreeSectors)
    {
        Dos33Geometry  geometry = Dos33Geometry::Make (35, 13);
        vector<Byte>   buffer   = MakeBlankVolume (geometry);
        Dos33Volume    volume (buffer);
        VolumeListing  listing;
        size_t         vtoc     = geometry.GetSectorOffset (kVtocTrack, 0);

        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual (size_t (0), listing.entries.size());
        Assert::AreEqual (kFreeOnBlankVolume, listing.freeUnits);
        Assert::AreEqual (2,  (int) buffer[vtoc + kVtocOffRelease]);
        Assert::AreEqual (12, (int) buffer[vtoc + 0x02]);
        Assert::AreEqual (13, (int) buffer[vtoc + 0x35]);
    }


    TEST_METHOD (ThirteenSectorVolume_FileWrittenReadAndDeleted)
    {
        Dos33Geometry          geometry = Dos33Geometry::Make (35, 13);
        vector<Byte>           buffer   = MakeBlankVolume (geometry);
        vector<Byte>           withFile;
        vector<Byte>           afterDelete;
        Dos33Volume            blank (buffer);
        FilePayload            payload  = MakeBinaryPayload (5000, 0x2000);
        FilePayload            back;
        VolumeListing          listing;
        VolumeIntegrityReport  report;

        AssertSucceeded (blank.Write (FilePath::Parse ("PROG"), payload, withFile));

        Assert::AreEqual (ThirteenSectorCodec::kImageByteSize, withFile.size());

        {
            Dos33Volume  written (withFile);

            AssertSucceeded (written.Enumerate (listing));
            AssertSucceeded (written.Read (FilePath::Parse ("PROG"), back));
            AssertSucceeded (written.BuildIntegrityReport (report));
            AssertSucceeded (written.Delete (FilePath::Parse ("PROG"), afterDelete));
        }

        Assert::AreEqual (size_t (1), listing.entries.size());
        Assert::IsTrue (listing.freeUnits < kFreeOnBlankVolume);
        Assert::IsTrue (payload.bytes == back.bytes);
        Assert::AreEqual ((int) payload.loadAddress, (int) back.loadAddress);
        Assert::AreEqual ((uint32_t) (35 * 13), report.GetUnitCount());
        Assert::IsTrue (report.IsClean());

        {
            Dos33Volume  deleted (afterDelete);

            AssertSucceeded (deleted.Enumerate (listing));
        }

        Assert::AreEqual (size_t (0), listing.entries.size());
        Assert::AreEqual (kFreeOnBlankVolume, listing.freeUnits);
    }


    TEST_METHOD (ThirteenSectorVolume_FilledToTheLastSector_ThenFull)
    {
        Dos33Geometry  geometry = Dos33Geometry::Make (35, 13);
        vector<Byte>   buffer   = MakeBlankVolume (geometry);
        vector<Byte>   next;
        VolumeListing  listing;
        HRESULT        hr       = S_OK;
        int            index    = 0;

        //  Each file is one track/sector list and twelve data sectors.
        while (SUCCEEDED (hr) && index < 100)
        {
            Dos33Volume  volume (buffer);
            FilePayload  payload = MakeBinaryPayload (12 * 256 - 4, 0x2000);

            hr = volume.Write (FilePath::Parse (std::format ("F{}", index)), payload, next);

            if (SUCCEEDED (hr))
            {
                buffer = next;
                index++;
            }
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_DISK_FULL), hr);
        Assert::AreEqual (31, index);

        {
            Dos33Volume            full (buffer);
            VolumeIntegrityReport  report;

            AssertSucceeded (full.Enumerate (listing));
            AssertSucceeded (full.BuildIntegrityReport (report));

            Assert::AreEqual (0u, listing.freeUnits);
            Assert::IsTrue (report.IsClean());
        }
    }


    TEST_METHOD (ThirteenSectorVolume_StartupProgram_IsNotSupported)
    {
        Dos33Geometry  geometry = Dos33Geometry::Make (35, 13);
        vector<Byte>   buffer   = MakeBlankVolume (geometry);
        vector<Byte>   withFile;
        vector<Byte>   result;
        Dos33Volume    blank (buffer);
        FilePayload    payload  = MakeBinaryPayload (100, 0x2000);
        HRESULT        hr       = S_OK;

        AssertSucceeded (blank.Write (FilePath::Parse ("HELLO"), payload, withFile));

        {
            Dos33Volume  written (withFile);

            hr = written.SetStartupProgram (FilePath::Parse ("HELLO"), result);
        }

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED), hr);
    }
};

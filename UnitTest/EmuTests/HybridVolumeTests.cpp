#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/Partitions/HybridVolume.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  HybridVolumeTests
//
//  A 140K disk with a DOS 3.3 volume on tracks 3 to 16 and a ProDOS volume on
//  the rest: both found, a file written to each, and a write failing when a
//  damaged free map would put it over the other volume.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (HybridVolumeTests)
{
public:

    static constexpr Byte  kVolumeNumber   = 254;
    static constexpr int   kDosFirstTrack  = 3;
    static constexpr int   kDosLastTrack   = 16;
    static constexpr int   kTrackCount     = 35;
    static constexpr int   kBlockCount     = 280;
    static constexpr int   kBlocksPerTrack = 8;

    static void SetDosTrackFree (vector<Byte> & buffer, int track, bool isFree)
    {
        Dos33Geometry  geometry;
        int            sector = 0;
        size_t         at     = 0;

        for (sector = 0; sector < geometry.sectorsPerTrack; sector++)
        {
            at = geometry.GetBitmapByteOffset (track, sector);

            buffer[at] = isFree ? (Byte) (buffer[at] | geometry.GetBitmapMask (sector)) : (Byte) (buffer[at] & ~geometry.GetBitmapMask (sector));
        }
    }

    static void SetProDosBlockFree (vector<Byte> & buffer, int block, bool isFree)
    {
        size_t  at   = ProDosSkeleton::GetBitmapByteOffset (buffer, (uint32_t) block);
        Byte    mask = (Byte) (0x80 >> (block % 8));

        buffer[at] = isFree ? (Byte) (buffer[at] | mask) : (Byte) (buffer[at] & ~mask);
    }

    //  DOS has tracks 3 to 16 and its VTOC and catalog on track 17. ProDOS has
    //  the blocks of every other track.
    static vector<Byte> MakeHybrid()
    {
        Dos33Geometry  geometry;
        vector<Byte>   dos    (geometry.GetImageByteSize(), 0);
        vector<Byte>   buffer (geometry.GetImageByteSize(), 0);
        size_t         vtoc   = geometry.GetSectorOffset (Dos33Geometry::kVtocTrack, 0);
        int            track  = 0;
        int            block  = 0;

        AssertSucceeded (Dos33Skeleton::Write (dos, kVolumeNumber));
        AssertSucceeded (ProDosSkeleton::Write (buffer, "HYBRID"));

        std::copy_n (dos.begin() + (ptrdiff_t) vtoc, (size_t) geometry.sectorsPerTrack * Dos33Geometry::kSectorByteSize, buffer.begin() + (ptrdiff_t) vtoc);

        for (track = Dos33Geometry::kVtocTrack + 1; track < kTrackCount; track++)
        {
            SetDosTrackFree (buffer, track, false);
        }

        for (block = kDosFirstTrack * kBlocksPerTrack; block < (Dos33Geometry::kVtocTrack + 1) * kBlocksPerTrack; block++)
        {
            SetProDosBlockFree (buffer, block, false);
        }

        return buffer;
    }

    static FilePayload MakePayload (Byte type, size_t length, Byte seed)
    {
        FilePayload  payload;
        size_t       i = 0;

        payload.type           = type;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        payload.bytes.resize (length);

        for (i = 0; i < length; i++)
        {
            payload.bytes[i] = (Byte) ((i * 5 + i / 256 + seed) | 0x01);
        }

        return payload;
    }


    TEST_METHOD (DetectFilesystems_Hybrid_ReturnsDosThenProDos)
    {
        vector<Byte>        buffer = MakeHybrid();
        vector<VolumeKind>  kinds;

        VolumeImage::DetectFilesystems (buffer, kinds);

        Assert::AreEqual (size_t (2), kinds.size());
        Assert::IsTrue (kinds[0] == VolumeKind::Dos33);
        Assert::IsTrue (kinds[1] == VolumeKind::ProDos);
        Assert::IsTrue (VolumeImage::DetectFilesystem (buffer) == VolumeKind::Dos33);
    }


    TEST_METHOD (DetectFilesystems_DiskOfOneFileSystem_ReturnsIt)
    {
        Dos33Geometry       geometry;
        vector<Byte>        dos    (geometry.GetImageByteSize(), 0);
        vector<Byte>        proDos (geometry.GetImageByteSize(), 0);
        vector<Byte>        zeros  (geometry.GetImageByteSize(), 0);
        vector<VolumeKind>  kinds;

        AssertSucceeded (Dos33Skeleton::Write (dos, kVolumeNumber));
        AssertSucceeded (ProDosSkeleton::Write (proDos, "PLAIN"));

        VolumeImage::DetectFilesystems (dos, kinds);

        Assert::AreEqual (size_t (1), kinds.size());
        Assert::IsTrue (kinds[0] == VolumeKind::Dos33);

        VolumeImage::DetectFilesystems (proDos, kinds);

        Assert::AreEqual (size_t (1), kinds.size());
        Assert::IsTrue (kinds[0] == VolumeKind::ProDos);

        VolumeImage::DetectFilesystems (zeros, kinds);

        Assert::AreEqual (size_t (0), kinds.size());
    }


    TEST_METHOD (Hybrid_BlankVolumes_HaveTheFreeSpaceOfTheirAreas)
    {
        vector<Byte>              buffer = MakeHybrid();
        std::unique_ptr<IVolume>  dos    = VolumeImage::CreateVolume (VolumeKind::Dos33,  buffer);
        std::unique_ptr<IVolume>  proDos = VolumeImage::CreateVolume (VolumeKind::ProDos, buffer);
        VolumeListing             dosListing;
        VolumeListing             proDosListing;

        AssertSucceeded (dos->Enumerate (dosListing));
        AssertSucceeded (proDos->Enumerate (proDosListing));

        Assert::AreEqual ((uint32_t) ((kDosLastTrack - kDosFirstTrack + 1) * 16), dosListing.freeUnits);
        Assert::AreEqual ((uint32_t) (kBlockCount - (Dos33Geometry::kVtocTrack + 1 - kDosFirstTrack) * kBlocksPerTrack - ProDosSkeleton::GetReservedBlockCount (buffer.size())), proDosListing.freeUnits);
    }


    TEST_METHOD (Hybrid_FileWrittenToEachSide_BothReadBack)
    {
        vector<Byte>              buffer        = MakeHybrid();
        vector<Byte>              withDos;
        vector<Byte>              withBoth;
        std::unique_ptr<IVolume>  volume;
        FilePayload               dosPayload    = MakePayload (Dos33Volume::kTypeBinary,  5000, 3);
        FilePayload               proDosPayload = MakePayload (ProDosVolume::kTypeBinary, 5000, 90);
        FilePayload               back;

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, buffer);
        AssertSucceeded (volume->Write (FilePath::Parse ("DOSFILE"), dosPayload, withDos));

        volume = VolumeImage::CreateVolume (VolumeKind::ProDos, withDos);
        AssertSucceeded (volume->Write (FilePath::Parse ("PROFILE"), proDosPayload, withBoth));

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, withBoth);
        AssertSucceeded (volume->Read (FilePath::Parse ("DOSFILE"), back));
        Assert::IsTrue (dosPayload.bytes == back.bytes);

        volume = VolumeImage::CreateVolume (VolumeKind::ProDos, withBoth);
        AssertSucceeded (volume->Read (FilePath::Parse ("PROFILE"), back));
        Assert::IsTrue (proDosPayload.bytes == back.bytes);
    }


    //  The DOS map has ProDOS's tracks free and its tracks in use, and a
    //  ProDOS file is on most of those tracks.
    TEST_METHOD (Hybrid_DosMapWithProDosFileSectorsFree_DosWriteFails)
    {
        vector<Byte>              buffer        = MakeHybrid();
        vector<Byte>              withProDos;
        vector<Byte>              result;
        std::unique_ptr<IVolume>  volume;
        FilePayload               proDosPayload = MakePayload (ProDosVolume::kTypeBinary, 140 * 512, 90);
        FilePayload               dosPayload    = MakePayload (Dos33Volume::kTypeBinary,  15000, 3);
        HRESULT                   hr            = S_OK;
        int                       track         = 0;

        volume = VolumeImage::CreateVolume (VolumeKind::ProDos, buffer);
        AssertSucceeded (volume->Write (FilePath::Parse ("BIG"), proDosPayload, withProDos));

        for (track = kDosFirstTrack; track < kTrackCount; track++)
        {
            if (track != Dos33Geometry::kVtocTrack)
            {
                SetDosTrackFree (withProDos, track, track > kDosLastTrack);
            }
        }

        {
            Dos33Volume  unchecked (withProDos);

            AssertSucceeded (unchecked.Write (FilePath::Parse ("DOSFILE"), dosPayload, result));
        }

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, withProDos);
        hr     = volume->Write (FilePath::Parse ("DOSFILE"), dosPayload, result);

        Assert::AreEqual (E_UNEXPECTED, hr);
        Assert::IsTrue (result.empty());
    }


    //  The ProDOS map has DOS's tracks free and its blocks in use, and a DOS
    //  file is on most of those tracks.
    TEST_METHOD (Hybrid_ProDosMapWithDosFileBlocksFree_ProDosWriteFails)
    {
        vector<Byte>              buffer        = MakeHybrid();
        vector<Byte>              withDos;
        vector<Byte>              result;
        std::unique_ptr<IVolume>  volume;
        FilePayload               dosPayload    = MakePayload (Dos33Volume::kTypeBinary,  200 * 256, 3);
        FilePayload               proDosPayload = MakePayload (ProDosVolume::kTypeBinary, 15000, 90);
        HRESULT                   hr            = S_OK;
        int                       block         = 0;

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, buffer);
        AssertSucceeded (volume->Write (FilePath::Parse ("BIG"), dosPayload, withDos));

        for (block = ProDosSkeleton::GetReservedBlockCount (withDos.size()); block < kBlockCount; block++)
        {
            SetProDosBlockFree (withDos, block, block >= kDosFirstTrack * kBlocksPerTrack && block < (kDosLastTrack + 1) * kBlocksPerTrack);
        }

        {
            ProDosVolume  unchecked (withDos);

            AssertSucceeded (unchecked.Write (FilePath::Parse ("PROFILE"), proDosPayload, result));
        }

        volume = VolumeImage::CreateVolume (VolumeKind::ProDos, withDos);
        hr     = volume->Write (FilePath::Parse ("PROFILE"), proDosPayload, result);

        Assert::AreEqual (E_UNEXPECTED, hr);
        Assert::IsTrue (result.empty());
    }


    //  The ProDOS map has only the blocks of the DOS VTOC and catalog free.
    TEST_METHOD (Hybrid_ProDosMapWithCatalogTrackFree_ProDosWriteFails)
    {
        vector<Byte>              buffer  = MakeHybrid();
        vector<Byte>              result;
        std::unique_ptr<IVolume>  volume;
        FilePayload               payload = MakePayload (ProDosVolume::kTypeBinary, 100, 90);
        HRESULT                   hr      = S_OK;
        int                       block   = 0;

        for (block = ProDosSkeleton::GetReservedBlockCount (buffer.size()); block < kBlockCount; block++)
        {
            SetProDosBlockFree (buffer, block, block / kBlocksPerTrack == Dos33Geometry::kVtocTrack);
        }

        {
            ProDosVolume  unchecked (buffer);

            AssertSucceeded (unchecked.Write (FilePath::Parse ("PROFILE"), payload, result));
        }

        volume = VolumeImage::CreateVolume (VolumeKind::ProDos, buffer);
        hr     = volume->Write (FilePath::Parse ("PROFILE"), payload, result);

        Assert::AreEqual (E_UNEXPECTED, hr);
    }


    TEST_METHOD (Hybrid_DeleteAndRename_Succeed)
    {
        vector<Byte>              buffer  = MakeHybrid();
        vector<Byte>              withFile;
        vector<Byte>              renamed;
        vector<Byte>              deleted;
        std::unique_ptr<IVolume>  volume;
        FilePayload               payload = MakePayload (Dos33Volume::kTypeBinary, 3000, 3);
        VolumeListing             listing;

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, buffer);
        AssertSucceeded (volume->Write (FilePath::Parse ("FIRST"), payload, withFile));

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, withFile);
        AssertSucceeded (volume->Rename (FilePath::Parse ("FIRST"), "SECOND", renamed));

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, renamed);
        AssertSucceeded (volume->Delete (FilePath::Parse ("SECOND"), deleted));

        volume = VolumeImage::CreateVolume (VolumeKind::Dos33, deleted);
        AssertSucceeded (volume->Enumerate (listing));

        Assert::AreEqual (size_t (0), listing.entries.size());
    }
};

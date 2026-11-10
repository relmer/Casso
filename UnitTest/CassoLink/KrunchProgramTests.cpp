#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/TestMachine.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/InProcessByteStream.h"
#include "InMemoryCassoLinkFolder.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  KrunchProgramTests
//
//  The program's krunch of an Apple Pascal volume, on an emulated //c, with
//  the host on the test's thread through a SteppedByteStream. The program
//  plans the moves and makes them itself, and after the request reads
//  nothing from the host but a Cancel between files. PascalVolume::Krunch,
//  the PC's krunch of the image, is the result it is checked against,
//  and the PC draws its block map from the same plan.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (KrunchProgramTests)
{
public:

    static constexpr Byte  kDrive = 1;

    using Rig = ProgramRig;

    //  A file's bytes: `blocks` blocks less 100 bytes, each a function of
    //  the file and its offset, so no two files and no two blocks match.
    static FilePayload  MakeFile (int file, int blocks)
    {
        FilePayload  payload;



        payload.bytes.resize ((size_t) blocks * PascalVolume::kBlockBytes - 100);

        for (size_t j = 0; j < payload.bytes.size(); j++)
        {
            payload.bytes[j] = (Byte) (file * 7 + j * 3 + (j >> 9));
        }

        return payload;
    }

    //  A volume of the files of `sizes`, in blocks, written in order, then
    //  those in `deleted` removed, so the free space is in pieces.
    static std::vector<Byte>  BuildDisk (std::span<const int> sizes, std::span<const int> deleted)
    {
        std::vector<Byte>  image (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  next;



        PascalVolume::WriteBlank ("KRUNCH", 0, image);

        for (size_t i = 0; i < sizes.size(); i++)
        {
            AssertSucceeded (PascalVolume (image).Write (FilePath::Parse (std::format ("F{:02}.DATA", i)), MakeFile ((int) i, sizes[i]), next));
            image = next;
        }

        for (int i : deleted)
        {
            AssertSucceeded (PascalVolume (image).Delete (FilePath::Parse (std::format ("F{:02}.DATA", i)), next));
            image = next;
        }

        return image;
    }

    //  The free space in pieces, between files that move over part of the
    //  blocks they were in, and one of 20 blocks that crosses tracks at both
    //  places.
    static std::vector<Byte>  BuildSmallDisk()
    {
        static constexpr int  kSizes[]   = { 3, 1, 4, 2, 20, 1, 3, 5, 2, 6 };
        static constexpr int  kDeleted[] = { 1, 3, 5, 8 };



        return BuildDisk (kSizes, kDeleted);
    }

    //  Enough files that the directory's last block has entries, and the
    //  entry in slot 59 has its first block in one sector and its next block
    //  in the following one.
    static std::vector<Byte>  BuildLargeDisk()
    {
        static constexpr int  kDeleted[] = { 2, 9, 25, 33, 50, 64 };
        std::vector<int>      sizes;



        for (int i = 0; i < 66; i++)
        {
            sizes.push_back (i == 10 ? 20 : i == 40 ? 17 : 1 + i % 3);
        }

        return BuildDisk (sizes, kDeleted);
    }

    //  Every file on the volume, read whole.
    static std::map<std::string, std::vector<Byte>>  ReadFiles (const std::vector<Byte> & sectors)
    {
        PascalVolume                              volume (sectors);
        VolumeListing                             listing;
        std::map<std::string, std::vector<Byte>>  files;



        AssertSucceeded (volume.Enumerate (listing));

        for (const FileEntry & entry : listing.entries)
        {
            FilePayload  payload;

            AssertSucceeded (volume.Read (FilePath::Parse (entry.name), payload));
            files[entry.name] = payload.bytes;
        }

        return files;
    }

    //  The CRC of the directory the host checked, from the PC's plan.
    static Word  DirectoryCrc (const std::vector<Byte> & disk, std::vector<PascalKrunchMove> & outMoves)
    {
        std::vector<Byte>  directory;



        AssertSucceeded (PascalVolume (disk).PlanKrunch (outMoves, directory));
        return CassoLinkCodec::ComputeCrc (directory);
    }



    //  Every file moves to where the PC's krunch of the image puts it, the
    //  Status has the files moved, and the progress rises to 100.
    TEST_METHOD (Krunch_MatchesThePcsKrunchOfTheImage)
    {
        std::vector<Byte>              disk     = BuildLargeDisk();
        std::vector<Byte>              pcKrunch;
        std::vector<PascalKrunchMove>  moves;
        std::vector<int>               reported;
        CassoLinkStatus                status;
        int                            moved    = 0;
        Word                           crc      = DirectoryCrc (disk, moves);
        Rig                            rig (disk);



        AssertSucceeded (PascalVolume (disk).Krunch (pcKrunch, moved));
        Assert::IsTrue (moved > 50, L"most files move");

        AssertSucceeded (rig.host->Krunch (kDrive, crc, [&] (Byte percent, Byte) { reported.push_back (percent); }, status));

        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::AreEqual<int> ((int) moves.size(), status.done, L"the Status has the files moved");
        Assert::IsTrue (rig.Sectors() == pcKrunch, L"the disk is the PC's krunch of the image");
        Assert::IsTrue (ReadFiles (rig.Sectors()) == ReadFiles (disk), L"every file reads as it did");
        Assert::IsFalse (reported.empty());
        Assert::IsTrue (std::is_sorted (reported.begin(), reported.end()));
        Assert::AreEqual (100, reported.back());
    }



    //  The directory isn't the one the host checked: the Status is DISK
    //  CHANGED, and nothing moves.
    TEST_METHOD (Krunch_WithAnotherDirectoryCrc_MovesNothing)
    {
        std::vector<Byte>              disk = BuildSmallDisk();
        std::vector<PascalKrunchMove>  moves;
        CassoLinkStatus                status;
        Word                           crc  = DirectoryCrc (disk, moves);
        Rig                            rig (disk);



        AssertSucceeded (rig.host->Krunch (kDrive, (Word) (crc ^ 1), nullptr, status));

        Assert::AreEqual<int> (CassoLinkHost::kErrorChanged, status.error);
        Assert::IsTrue (rig.Sectors() == disk, L"nothing moved");
    }



    //  A Cancel after the first progress: the program stops between files,
    //  so the volume is part krunched and every file reads as it did. A
    //  krunch started again finishes it.
    TEST_METHOD (Krunch_Canceled_StopsBetweenFiles_AndEveryFileReadsAsItDid)
    {
        std::vector<Byte>                         disk     = BuildSmallDisk();
        std::map<std::string, std::vector<Byte>>  before   = ReadFiles (disk);
        std::vector<Byte>                         pcKrunch;
        std::vector<Byte>                         after;
        std::vector<PascalKrunchMove>             moves;
        CassoLinkStatus                           status;
        int                                       moved    = 0;
        Word                                      crc      = DirectoryCrc (disk, moves);
        Rig                                       rig (disk);
        HRESULT                                   hr       = S_OK;



        AssertSucceeded (PascalVolume (disk).Krunch (pcKrunch, moved));

        hr = rig.host->Krunch (kDrive, crc, [&] (Byte percent, Byte) { if (percent > 0) { rig.host->Cancel(); } }, status);
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), hr);

        after = rig.Sectors();

        Assert::IsTrue (after != disk,     L"a file moved");
        Assert::IsTrue (after != pcKrunch, L"stopped partway");
        Assert::IsTrue (ReadFiles (after) == before, L"every file reads as it did");

        crc = DirectoryCrc (after, moves);
        AssertSucceeded (rig.host->Krunch (kDrive, crc, nullptr, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::IsTrue (ReadFiles (rig.Sectors()) == before);

        DirectoryCrc (rig.Sectors(), moves);
        Assert::IsTrue (moves.empty(), L"krunched");
    }



    //  The machine stopped at points through a krunch, as by a power cut.
    //  Stopping the CPU doesn't change the disk, so the disk at each point of
    //  one krunch is the disk after a stop there. Drive 1's image is copied at
    //  each point, and each copy is checked after the krunch. The sector being
    //  written at a point can be torn, as on a real drive, so the copy is
    //  salvaged and at most one sector is lost. Unless that sector is the
    //  directory's, every file reads as it did, except at most the one being
    //  moved.
    TEST_METHOD (Krunch_StoppedAtAnyPoint_LeavesEveryOtherFileWhole)
    {
        static constexpr uint64_t  kStopCycles = 50'000;   // between the points copied

        std::vector<Byte>                         disk     = BuildSmallDisk();
        std::map<std::string, std::vector<Byte>>  before   = ReadFiles (disk);
        std::vector<PascalKrunchMove>             moves;
        Word                                      crc      = DirectoryCrc (disk, moves);
        Rig                                       rig (disk);
        CassoLinkStatus                           status;
        std::vector<DiskImage>                    copies;
        std::vector<uint64_t>                     points;
        uint64_t                                  next     = 0;
        int                                       whole    = 0;



        rig.stream->SetStepHook ([&] ()
        {
            if (next != 0 && rig.stream->GetCycles() >= next)
            {
                copies.push_back (*rig.machine.GetDiskStore().GetImage (ProgramRig::kSlot, 0));
                points.push_back (rig.stream->GetCycles());
                next += kStopCycles;
            }
        });

        AssertSucceeded (rig.host->Krunch (kDrive, crc, [&] (Byte, Byte) { next = (next == 0) ? rig.stream->GetCycles() : next; }, status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::IsTrue (copies.size() > 20, std::format (L"{} points copied", copies.size()).c_str());

        for (size_t i = 0; i < copies.size(); i++)
        {
            DenibblizeReport    report;
            std::vector<Byte>   file;
            std::vector<Byte>   sectors;
            SectorDecodeReport  decode;
            VolumeListing       listing;
            int                 changed = 0;
            std::wstring        where   = std::format (L"stopped at {} cycles", points[i]);



            AssertSucceeded (NibblizationLayer::SalvageSectors (copies[i], DiskFormat::Dsk, file, report));
            Assert::IsTrue (report.sectorsLost + report.sectorsRecovered <= 1, (where + std::format (L": {} sectors damaged", report.sectorsLost + report.sectorsRecovered)).c_str());
            AssertSucceeded (VolumeImage::Load (file, "d.dsk", sectors, decode));

            //  A torn directory sector can fail to parse, or parse with a name
            //  no file had.
            HRESULT  listed = PascalVolume (sectors).Enumerate (listing);

            if (FAILED (listed) ||
                std::any_of (listing.entries.begin(), listing.entries.end(), [&] (const FileEntry & e) { return !before.contains (e.name); }))
            {
                Assert::AreEqual (1, report.sectorsLost + report.sectorsRecovered, (where + L": the directory is damaged with no sector damaged").c_str());
                continue;
            }

            for (const FileEntry & entry : listing.entries)
            {
                FilePayload  payload;
                HRESULT      read = PascalVolume (sectors).Read (FilePath::Parse (entry.name), payload);

                changed += (FAILED (read) || payload.bytes != before.at (entry.name)) ? 1 : 0;
            }

            Assert::AreEqual (before.size(), listing.entries.size(), (where + L": every file is listed").c_str());
            Assert::IsTrue (changed <= 1, (where + std::format (L": {} files changed", changed)).c_str());
            whole++;
        }

        Assert::IsTrue (whole > (int) copies.size() / 2, L"most points left a directory to read");
    }



    //  A volume with no free space between its files: nothing moves, and the
    //  Status has no files moved.
    TEST_METHOD (Krunch_OfAKrunchedDisk_MovesNothing)
    {
        static constexpr int  kSizes[] = { 3, 1, 4 };

        std::vector<Byte>              disk = BuildDisk (kSizes, {});
        std::vector<PascalKrunchMove>  moves;
        CassoLinkStatus                status;
        Word                           crc  = DirectoryCrc (disk, moves);
        Rig                            rig (disk);



        Assert::IsTrue (moves.empty(), L"the PC's plan moves nothing");

        AssertSucceeded (rig.host->Krunch (kDrive, crc, nullptr, status));

        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::AreEqual<int> (0, status.done);
        Assert::IsTrue (rig.Sectors() == disk, L"nothing moved");
    }
};

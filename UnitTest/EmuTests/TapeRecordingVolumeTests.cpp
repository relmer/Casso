#include "Pch.h"

#include "../EhmTestHelper.h"
#include "TapeTestEncoder.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Tape/TapeRecordingVolume.h"
#include "Devices/Tape/TapeRecordingWriter.h"
#include "Devices/Tape/WavCodec.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolumeTests
//
//  Recordings opened as folders of the files on them: BASIC programs found
//  by their length records, binaries as single records, each read back as
//  it was written, and a file whose checksum fails listed with that reason.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeRecordingVolumeTests)
{
public:

    static std::vector<Byte> MakeBytes (size_t count, Byte seed)
    {
        std::vector<Byte>  bytes (count);



        for (size_t i = 0; i < count; i++)
        {
            bytes[i] = (Byte) (i * 29 + seed);
        }

        return bytes;
    }


    static std::vector<TapeRecordingFile> MakeFiles()
    {
        TapeRecordingFile  applesoft;
        TapeRecordingFile  integer;
        TapeRecordingFile  binary;



        applesoft.kind     = TapeFileKind::Applesoft;
        applesoft.bytes    = MakeBytes (60, 5);
        integer.kind       = TapeFileKind::IntegerBasic;
        integer.bytes      = MakeBytes (33, 7);
        binary.kind        = TapeFileKind::Binary;
        binary.bytes       = MakeBytes (200, 2);
        binary.loadAddress = 0x2000;

        return { applesoft, integer, binary };
    }


    static std::vector<Byte> MakeWav (const std::vector<TapeRecordingFile> & files)
    {
        TapeAudio          audio;
        std::string        sidecar;
        std::vector<Byte>  wav;



        TapeRecordingWriter::Render (files, audio, sidecar);
        WavCodec::Encode (audio, wav);

        return wav;
    }


    TEST_METHOD (Enumerate_ListsEachFileWithItsType)
    {
        std::vector<Byte>    wav    = MakeWav (MakeFiles());
        TapeRecordingVolume  volume (wav);
        VolumeListing        listing;



        AssertSucceeded (volume.Enumerate (listing));

        Assert::AreEqual ((size_t) 3, listing.entries.size());
        Assert::IsTrue   (listing.damage.empty());
        Assert::AreEqual (std::string ("FILE1"), listing.entries[0].name);
        Assert::AreEqual (std::string ("FILE3"), listing.entries[2].name);
        Assert::AreEqual (TapeRecordingVolume::kTypeApplesoft, listing.entries[0].type);
        Assert::AreEqual (TapeRecordingVolume::kTypeInteger,   listing.entries[1].type);
        Assert::AreEqual (TapeRecordingVolume::kTypeBinary,    listing.entries[2].type);
        Assert::AreEqual (60u,  listing.entries[0].eofBytes);
        Assert::AreEqual (33u,  listing.entries[1].eofBytes);
        Assert::AreEqual (200u, listing.entries[2].eofBytes);
        Assert::AreEqual ((Word) 0x0801, listing.entries[0].auxType);
    }


    TEST_METHOD (Read_ReturnsEachFileAsItWasWritten)
    {
        std::vector<TapeRecordingFile>  files  = MakeFiles();
        std::vector<Byte>               wav    = MakeWav (files);
        TapeRecordingVolume             volume (wav);



        for (size_t i = 0; i < files.size(); i++)
        {
            FilePayload  payload;

            AssertSucceeded (volume.Read (FilePath::Parse (std::format ("FILE{}", i + 1)), payload));
            Assert::IsTrue  (payload.bytes == files[i].bytes);
        }
    }


    //  The first data bit of the second record is flipped, so its checksum
    //  fails.
    TEST_METHOD (AFailedChecksum_IsListedWithTheReasonAndIsNotRead)
    {
        std::vector<double>  halves;
        std::vector<Byte>    first  = MakeBytes (40, 1);
        std::vector<Byte>    second = MakeBytes (40, 9);
        TapeEncodeOptions    options;
        TapeAudio            audio;
        std::vector<Byte>    wav;
        VolumeListing        listing;
        FilePayload          payload;
        size_t               dataAt = 0;



        TapeTestEncoder::AppendRecord (halves, first, options);
        halves.push_back (2'000'000.0);

        dataAt = halves.size() + (size_t) std::lround (options.leaderSeconds * 1.0e6 / TapeTestEncoder::kLeaderHalfUs) + 2;
        TapeTestEncoder::AppendRecord (halves, second, options);

        halves[dataAt]     = (halves[dataAt] == TapeTestEncoder::kOneHalfUs) ? TapeTestEncoder::kZeroHalfUs : TapeTestEncoder::kOneHalfUs;
        halves[dataAt + 1] = halves[dataAt];

        TapeTestEncoder::Render    (halves, options, audio);
        TapeTestEncoder::EncodeWav (audio, options, wav);

        TapeRecordingVolume  volume (wav);

        AssertSucceeded  (volume.Enumerate (listing));
        Assert::AreEqual ((size_t) 2, listing.entries.size());
        Assert::IsTrue   (listing.damage == std::vector<std::string> { "FILE2 fails its checksum" });
        AssertSucceeded  (volume.Read (FilePath::Parse ("FILE1"), payload));
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CRC), volume.Read (FilePath::Parse ("FILE2"), payload));
    }


    TEST_METHOD (ARecording_OpensThroughVolumeImageAsATape)
    {
        std::vector<Byte>         wav     = MakeWav (MakeFiles());
        std::vector<Byte>         sectors;
        SectorDecodeReport        report;
        std::vector<Byte>         changed;
        FilePayload               payload;
        VolumeListing             listing;
        std::unique_ptr<IVolume>  volume;



        AssertSucceeded (VolumeImage::Load (wav, "programs.wav", sectors, report));
        Assert::IsTrue  (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Tape);

        volume = VolumeImage::CreateVolume (VolumeKind::Tape, sectors);

        AssertSucceeded  (volume->Enumerate (listing));
        Assert::AreEqual ((size_t) 3, listing.entries.size());
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT), volume->Write (FilePath::Parse ("NEW"), payload, changed));
        Assert::AreEqual ("Tape recording", VolumeImage::GetSystemName (VolumeKind::Tape));
    }
};

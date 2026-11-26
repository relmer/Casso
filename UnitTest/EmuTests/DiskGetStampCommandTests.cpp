#include "Pch.h"

#include "FakeDiskFileIo.h"

#include "Core/TextEncoding.h"

#include "Devices/Disk/DiskCommandResult.h"
#include "Devices/Disk/DiskGetStampCommand.h"
#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampChunk.h"
#include "Devices/Disk/Stamp/FormatStampText.h"
#include "Machines/Apple2/Common/WozChunkWalker.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskGetStampCommandTests
//
//  `disk get-stamp` on WOZ files in a fake filesystem.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskGetStampCommandTests)
{
public:

    static constexpr size_t  kTrackBits = 51200;
    static constexpr char    kPath[]    = "disk.woz";
    static constexpr size_t  kDskBytes  = 143360;



    static vector<Byte> MakeWoz()
    {
        vector<Byte>  bits ((kTrackBits + 7) / 8, 0xFF);
        vector<Byte>  woz;



        WozLoader::BuildSyntheticV2 (WozLoader::kDiskType525, false, bits, kTrackBits, woz);

        return woz;
    }



    static FormatStamp MakeStamp()
    {
        FormatStamp  stamp;
        StampTrack   track;



        stamp.writer                    = "Casso 1.32.0";
        stamp.written                   = "2026-11-23T00:00:00Z";
        stamp.hasCapture                = true;
        stamp.capture.reads             = 3;
        track.position                  = 70;
        track.encoding                  = StampEncoding::Thirteen;
        track.addressPrologs            = { { 0xD7, 0xAA, 0xB7 }, { 0xDF, 0xAA, 0xF7 } };
        track.dataPrologs               = { { 0xD7, 0xAA, 0xBD } };
        track.sectorsFound              = 13;
        track.sectorsGood               = 12;
        track.volumes                   = { 254 };
        track.hasCapture                = true;
        track.capture.isWeak            = true;
        track.capture.replacedSectors   = { 4 };
        stamp.tracks.push_back (track);

        return stamp;
    }



    static void RunStamp (FakeDiskFileIo & io, bool isJson, DiskCommandResult & result)
    {
        DiskGetStampCommand::Run (io, kPath, isJson, result);
    }



    TEST_METHOD (StampedImage_PrintsATable)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;
        vector<Byte>       woz = MakeWoz();



        FormatStampChunk::SetStampInFile (woz, MakeStamp());
        io.files[kPath] = woz;

        RunStamp (io, false, result);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue   (result.output.starts_with ("Format stamp: capture, Casso 1.32.0, 2026-11-23T00:00:00Z\n"));
        Assert::IsTrue   (result.output.find ("Capture: 3 reads") != std::string::npos);
        Assert::IsTrue   (result.output.find ("17.5   13-sector    D7 AA B7, DF AA F7  D7 AA BD            13       12    254     weak, replaced 4") != std::string::npos, TextEncoding::Utf8ToWide (result.output).c_str());
    }



    TEST_METHOD (StampedImageWithJson_PrintsTheStoredText)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;
        vector<Byte>       woz   = MakeWoz();
        std::string        text;
        StampState         state = StampState::Absent;
        FormatStamp        stamp;



        FormatStampChunk::SetStampInFile (woz, MakeStamp());
        FormatStampChunk::GetStampFromFile (woz, text, state, stamp);
        io.files[kPath] = woz;

        RunStamp (io, true, result);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (text + "\n", result.output);
    }



    TEST_METHOD (ImageWithoutAStamp_ExitsWithNoStamp)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;



        io.files[kPath] = MakeWoz();

        RunStamp (io, false, result);

        Assert::AreEqual (DiskGetStampCommand::kNoStamp, result.exitStatus);
        Assert::AreEqual (std::string ("disk.woz has no format stamp.\n"), result.output);
    }



    TEST_METHOD (UnreadableStamp_ExitsWithUnreadable)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;
        vector<Byte>       woz  = MakeWoz();
        std::string        text = "{ later }";
        size_t             at   = woz.size();



        woz.resize (at + WozChunkWalker::kChunkHeaderSize + text.size());
        memcpy (woz.data() + at, FormatStampChunk::kId, sizeof (FormatStampChunk::kId));
        WozChunkWalker::WriteSize (woz.data() + at + sizeof (FormatStampChunk::kId), (uint32_t) text.size());
        memcpy (woz.data() + at + WozChunkWalker::kChunkHeaderSize, text.data(), text.size());
        io.files[kPath] = woz;

        RunStamp (io, false, result);
        Assert::AreEqual (DiskGetStampCommand::kUnreadable, result.exitStatus);
        Assert::IsTrue   (result.output.empty());

        result = DiskCommandResult();
        RunStamp (io, true, result);
        Assert::AreEqual (DiskGetStampCommand::kUnreadable, result.exitStatus);
        Assert::AreEqual (text + "\n", result.output);
    }



    TEST_METHOD (NotAWoz_Fails)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;



        io.files[kPath] = vector<Byte> (kDskBytes, 0);

        RunStamp (io, false, result);

        Assert::AreNotEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsFalse    (result.diagnostics.empty());
    }



    TEST_METHOD (FormatPosition_ListsQuarterTracks)
    {
        Assert::AreEqual (std::string ("17"),    FormatStampText::FormatPosition (68));
        Assert::AreEqual (std::string ("17.25"), FormatStampText::FormatPosition (69));
        Assert::AreEqual (std::string ("17.5"),  FormatStampText::FormatPosition (70));
        Assert::AreEqual (std::string ("17.75"), FormatStampText::FormatPosition (71));
    }
};

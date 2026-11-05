#include "Pch.h"

#include "EhmTestHelper.h"
#include "TestMachine.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkKrunchTests
//
//  Krunch over the link, against Casso's program on an emulated //c: an
//  Apple Pascal disk with its free space in pieces, in drive 1, krunched by
//  the program on the Apple. The disk afterward is compared byte for byte
//  with the krunch PascalVolume makes of the same image, and every file is
//  as it was after a krunch whose link drops or that is canceled partway.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkKrunchTests)
{
public:

    static constexpr uint64_t  kFrame       = 17030;
    static constexpr uint64_t  kBudget      = 4'000'000'000;
    static constexpr Byte      kFastControl = 0x10;     // 115,200 baud
    static constexpr Word      kRowLabel    = 0x0600;
    static constexpr Word      kRowDrive1   = 0x0780;
    static constexpr Word      kRowBar1     = 0x0428;
    static constexpr Word      kRowError    = 0x06A8;

    //  The one line to the machine, as the session's TCP connection.
    struct Factory : IByteStreamFactory
    {
        std::shared_ptr<InProcessByteStream>  pcEnd;

        struct Handle : IByteStream
        {
            std::shared_ptr<InProcessByteStream>  end;

            explicit Handle (std::shared_ptr<InProcessByteStream> e) : end (std::move (e)) {}

            HRESULT  Write            (const Byte * data, size_t count) override                                   { return end->Write (data, count); }
            HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { return end->Read (buffer, capacity, timeoutMs, outRead); }
            size_t   GetReadableCount() override                                                                  { return end->GetReadableCount(); }
            bool     IsClosed() const override                                                                    { return end->IsClosed(); }
            void     Close() override                                                                             { end->Close(); }
        };

        HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override                                      { outPorts.clear(); return S_OK; }
        HRESULT  OpenComPort  (const std::wstring &, const ComSettings &, std::unique_ptr<IByteStream> &) override { return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND); }
        HRESULT  ListenTcp    (uint16_t, LinkTransport, std::unique_ptr<IByteStream> &) override                  { return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED); }

        HRESULT  ConnectTcp (const std::string &, uint16_t, std::unique_ptr<IByteStream> & outStream) override
        {
            outStream = std::make_unique<Handle> (pcEnd);
            return S_OK;
        }
    };

    //  The operations done, and the highest progress a krunch reported.
    struct Sink : ICassoLinkSessionSink
    {
        std::mutex                                    lock;
        std::vector<std::shared_ptr<LinkOperation>>   done;
        std::atomic<int>                              progress { 0 };

        void  OnStateChanged (LinkState) override {}

        void  OnProgress (const std::shared_ptr<LinkOperation> & operation) override
        {
            if (operation->kind == LinkOperationKind::Krunch)
            {
                progress = (std::max) (progress.load(), operation->progress.load());
            }
        }

        void  OnDone (const std::shared_ptr<LinkOperation> & operation) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            done.push_back (operation);
        }

        bool  IsDone (const std::shared_ptr<LinkOperation> & operation)
        {
            std::lock_guard<std::mutex>  guard (lock);

            return std::find (done.begin(), done.end(), operation) != done.end();
        }
    };

    //  An emulated //c running the shim at 115,200 baud, with `disk` in drive 1.
    static void  StartMachine (TestMachine & machine, const std::vector<Byte> & disk, std::shared_ptr<InProcessByteStream> & machineEnd)
    {
        std::vector<Byte>  shim;
        Word               at = LinkProgram::kShimAddress;

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Dsk, disk));
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));
        Assert::AreEqual (S_OK, LinkProgram::BuildShim (115200, shim));

        for (Byte b : shim)
        {
            machine.GetCpu()->WriteByte (at++, b);
        }

        machine.GetCpu()->SetPC (LinkProgram::kShimAddress);
    }

    //  Drive 1's disk, as DOS-logical sectors.
    static std::vector<Byte>  ReadDrive1 (TestMachine & machine)
    {
        std::vector<Byte>   file;
        std::vector<Byte>   sectors;
        SectorDecodeReport  report;

        AssertSucceeded (machine.GetDiskStore().GetImage (6, 0)->Serialize (file));
        AssertSucceeded (VolumeImage::Load (file, "d.dsk", sectors, report));
        return sectors;
    }

    //  A session on the machine, as Casso Explorer runs one.
    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::unique_ptr<CassoLinkSession>     session;

        explicit Rig (const std::vector<Byte> & disk)
        {
            CassoLinkSettings  settings;

            InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
            StartMachine (machine, disk, machineEnd);

            session = std::make_unique<CassoLinkSession> (factory, folder, fileIo, sink);

            settings.name      = "//c";
            settings.transport = LinkTransport::Emulated;
            settings.address   = "127.0.0.1:6502";
            settings.use40     = true;

            AssertSucceeded (session->Start (settings));
        }

        ~Rig()
        {
            session.reset();
        }

        //  A row of the text page as text, with its trailing spaces cut.
        std::string  Row (Word address)
        {
            std::string  text;

            for (int i = 0; i < 40; i++)
            {
                text += (char) (machine.GetCpu()->ReadByte ((Word) (address + i)) & 0x7F);
            }

            return text.substr (0, text.find_last_not_of (' ') + 1);
        }

        //  Steps the machine until `until` is true, and calls `eachFrame`
        //  after each frame. The budget runs out only on a failure.
        void  Step (const std::function<bool()> & until, const std::function<void()> & eachFrame = nullptr)
        {
            uint64_t  spent = 0;

            while (!until() && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;

                if (eachFrame)
                {
                    eachFrame();
                }
            }

            Assert::IsTrue (until(), L"the condition waited for is met");
        }

        std::shared_ptr<LinkOperation>  Finish (std::shared_ptr<LinkOperation> operation, const std::function<void()> & eachFrame = nullptr)
        {
            Step ([&] () { return sink.IsDone (operation); }, eachFrame);
            return operation;
        }

        std::vector<Byte>  Sectors() { return ReadDrive1 (machine); }
    };

    //  The host alone on the line, for a request the session doesn't send.
    struct HostRig
    {
        TestMachine                           machine { "Apple2c" };
        InMemoryCassoLinkFolder               folder;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::unique_ptr<CassoLinkHost>        host;

        explicit HostRig (const std::vector<Byte> & disk)
        {
            InProcessByteStream::CreatePair (machineEnd, hostEnd);
            StartMachine (machine, disk, machineEnd);

            host = std::make_unique<CassoLinkHost> (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });
            host->SetOperationTimeout (60000);
        }

        //  Steps the machine while the host runs `operation`.
        HRESULT  Run (std::function<HRESULT()> operation)
        {
            auto      result = std::async (std::launch::async, operation);
            uint64_t  spent  = 0;

            while (result.wait_for (std::chrono::seconds (0)) != std::future_status::ready && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
            }

            Assert::IsTrue (spent < kBudget, L"the operation finishes");
            return result.get();
        }
    };

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

    static std::string  Name (int file)
    {
        return std::format ("F{:02}.DATA", file);
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
            AssertSucceeded (PascalVolume (image).Write (FilePath::Parse (Name ((int) i)), MakeFile ((int) i, sizes[i]), next));
            image = next;
        }

        for (int i : deleted)
        {
            AssertSucceeded (PascalVolume (image).Delete (FilePath::Parse (Name (i)), next));
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

    //  Whether every file starts where the file ahead of it ends.
    static bool  IsKrunched (const std::vector<Byte> & sectors)
    {
        std::vector<PascalKrunchMove>  moves;
        std::vector<Byte>              directory;

        AssertSucceeded (PascalVolume (sectors).PlanKrunch (moves, directory));
        return moves.empty();
    }




    //  Most files move, and the krunch's progress is on the drive's row and
    //  its bar.
    TEST_METHOD (Krunch_MovesTheFilesAsAnImageKrunchDoes)
    {
        std::vector<Byte>               disk     = BuildLargeDisk();
        std::vector<Byte>               expected;
        int                             moved    = 0;
        int                             shown    = 0;
        std::string                     label;
        std::shared_ptr<LinkOperation>  krunch;
        std::shared_ptr<LinkOperation>  list;
        Rig                             rig (disk);



        AssertSucceeded (PascalVolume (disk).Krunch (expected, moved));
        Assert::IsTrue (moved > 50, L"most files move");

        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);

        krunch = rig.Finish (rig.session->Krunch (1), [&]
        {
            std::string  bar   = rig.Row (kRowBar1);
            std::string  shows = rig.Row (kRowLabel);
            size_t       at    = bar.find_last_of (' ');

            if (!bar.empty() && bar.back() == '%' && at != std::string::npos)
            {
                shown = (std::max) (shown, std::atoi (bar.c_str() + at + 1));
            }

            label = shows.empty() ? label : shows;
        });

        Logger::WriteMessage (std::format ("Krunch: 0x{:08X} {}", (uint32_t) krunch->result, krunch->message).c_str());
        AssertSucceeded (krunch->result);
        Assert::IsTrue (rig.Sectors() == expected, L"the disk is the image's krunch");
        Assert::AreEqual (std::string ("KRUNCHING D1 (/KRUNCH)"), label);
        Assert::AreEqual (100, shown, L"the bar is at 100%");
        Assert::IsTrue (rig.sink.progress > 0, L"the PC has the progress");

        list = rig.Finish (rig.session->Enumerate (1, FilePath()));

        AssertSucceeded (list->result);
        Assert::AreEqual (60, (int) list->listing.entries.size());
    }



    //  The link is closed after the first part of a move. The program goes on
    //  without it, and the disk is krunched to the end.
    TEST_METHOD (Krunch_GoesOnToItsEnd_WhenTheLinkDrops)
    {
        std::vector<Byte>               disk     = BuildSmallDisk();
        std::vector<Byte>               expected;
        int                             moved    = 0;
        std::shared_ptr<LinkOperation>  krunch;
        Rig                             rig (disk);



        AssertSucceeded (PascalVolume (disk).Krunch (expected, moved));
        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);

        krunch = rig.session->Krunch (1);

        rig.Step ([&] () { return rig.sink.progress > 0; });
        rig.factory.pcEnd->Close();
        rig.Finish (krunch);

        Assert::IsTrue (FAILED (krunch->result), L"the PC's operation ends with the line");

        //  The drive's row is IDLE again once the krunch ends.
        rig.Step ([&] () { return rig.Row (kRowDrive1) == "D1: IDLE"; });

        Assert::AreEqual (std::string(), rig.Row (kRowError), L"no error");
        Assert::IsTrue (rig.Sectors() == expected, L"the disk is krunched to the end");
    }



    //  Cancel after the first part of the 20-block file's move, which
    //  overlaps its old blocks. The program stops once that file's directory
    //  entry is written, so the volume is part krunched and every file reads
    //  as it did. That file's first part takes the krunch to 21%.
    TEST_METHOD (Krunch_Canceled_LeavesEveryFileAsItWas)
    {
        std::vector<Byte>                         disk   = BuildSmallDisk();
        std::map<std::string, std::vector<Byte>>  before = ReadFiles (disk);
        std::vector<Byte>                         after;
        std::shared_ptr<LinkOperation>            krunch;
        Rig                                       rig (disk);



        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);

        krunch = rig.session->Krunch (1);

        rig.Step ([&] () { return rig.sink.progress > 15; });
        rig.session->Cancel();
        rig.Finish (krunch);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CANCELLED), krunch->result);

        after = rig.Sectors();

        Assert::IsFalse (IsKrunched (after), L"stopped partway");
        Assert::IsTrue  (after != disk, L"a file moved");
        Assert::IsTrue  (ReadFiles (after) == before, L"every file reads as it did");

        //  A krunch started again finishes it.
        AssertSucceeded (rig.Finish (rig.session->Krunch (1))->result);
        Assert::IsTrue (IsKrunched (rig.Sectors()));
        Assert::IsTrue (ReadFiles (rig.Sectors()) == before);
    }



    //  A disk with no free space between files has nothing to move, and the
    //  Apple is sent no krunch.
    TEST_METHOD (Krunch_OfAKrunchedDisk_LeavesItAlone)
    {
        static constexpr int  kSizes[] = { 3, 1, 4 };

        std::vector<Byte>               disk   = BuildDisk (kSizes, {});
        std::shared_ptr<LinkOperation>  krunch;
        Rig                             rig (disk);



        krunch = rig.Finish (rig.session->Krunch (1));

        AssertSucceeded (krunch->result);
        Assert::IsTrue (rig.Sectors() == disk);
        Assert::AreEqual (0, rig.sink.progress.load(), L"no krunch on the Apple");
    }



    //  The program checks the directory's CRC against the host's before it
    //  moves anything. With another CRC the Status is DISK CHANGED and the
    //  disk is as it was; with the directory's CRC, each Krunch progress is
    //  no less than the last and the last is 100.
    TEST_METHOD (Krunch_ChecksTheDirectoryAgainstTheHosts)
    {
        std::vector<Byte>              disk      = BuildSmallDisk();
        std::vector<Byte>              expected;
        std::vector<PascalKrunchMove>  moves;
        std::vector<Byte>              directory;
        std::vector<int>               reported;
        CassoLinkStatus                status;
        int                            moved     = 0;
        Word                           crc       = 0;
        HostRig                        rig (disk);



        AssertSucceeded (PascalVolume (disk).Krunch (expected, moved));
        AssertSucceeded (PascalVolume (disk).PlanKrunch (moves, directory));

        crc = CassoLinkCodec::ComputeCrc (directory);

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->SetSpeed (kFastControl); }));
        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->Krunch (1, (Word) (crc ^ 1), nullptr, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorChanged, status.error);
        Assert::IsTrue (ReadDrive1 (rig.machine) == disk, L"nothing moved");

        Assert::AreEqual (S_OK, rig.Run ([&] () { return rig.host->Krunch (1, crc, [&] (Byte percent, Byte) { reported.push_back (percent); }, status); }));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);
        Assert::AreEqual<int> ((int) moves.size(), status.done, L"the Status has the files moved");
        Assert::IsTrue (ReadDrive1 (rig.machine) == expected);
        Assert::IsFalse (reported.empty());
        Assert::IsTrue (std::is_sorted (reported.begin(), reported.end()));
        Assert::AreEqual (100, reported.back());
    }
};

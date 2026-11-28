#include "Pch.h"

#include "EhmTestHelper.h"
#include "EmulatorLink.h"
#include "FakeDiskFileIo.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/FullSend/FullSendStatus.h"
#include "CassoLink/SteppedByteStream.h"
#include "Core/MachineConfig.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Core/PathResolver.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "CassoLink/Session/CassoLinkBlockDevices.h"
#include "Machines/Apple2/Common/SuperSerialCard.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSessionScenarioTests
//
//  The PC's session, as Casso Explorer runs it, on each emulated Apple II
//  with a serial port: the ][, the ][ plus, the //e and the enhanced //e
//  with Apple's Super Serial Card in slot 2, and the //c and //c Plus on
//  their port 2. Each has the two drives it was sold with: a Disk II card's
//  two drives, the //c's internal drive and a Disk IIc, and the //c Plus's
//  internal Apple 3.5 Drive and a second one. IN#2 is typed (and Ctrl-A
//  14B on a //c), and the session's bootstrap types the loader.
//
//  The machine runs on the session's thread, through a SteppedByteStream,
//  only while the session reads the line, so nothing depends on the PC's
//  clock. The bootstrap and the machine's model run with the scenario
//  suite. File operations, a disk read and written, and a Full Send of
//  both drives run on demand.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkSessionScenarioTests)
{
public:

    static constexpr uint64_t  kKeyCycles      = 100'000;
    static constexpr int       kPort           = 2;
    static constexpr DWORD     kBootstrapMs    = 10 * 60 * 1000;
    static constexpr DWORD     kFileMs         = 10 * 60 * 1000;
    static constexpr DWORD     kWholeDiskMs    = 30 * 60 * 1000;
    static constexpr DWORD     kPollMs         = 200;

    static constexpr const char *  kComPort    = "COM9";
    static constexpr const char *  kFolder     = "C:\\Box";
    static constexpr const char *  kD2Image    = "C:\\Box\\d2.dsk";
    static constexpr const char *  kD2Written  = "C:\\Box\\written.dsk";
    static constexpr const char *  kD2Again    = "C:\\Box\\again.dsk";
    static constexpr const char *  kD1Path     = "C:\\Disks\\drive1.po";
    static constexpr const char *  kD2Path     = "C:\\Disks\\drive2.po";
    static constexpr size_t        k800KBytes  = 1600 * 512;

    //  Communications mode at 9,600 baud, and the second bank as it ships.
    static constexpr const char *  kCommunicationsSw1 = "off off off on on on on";
    static constexpr const char *  kShippedSw2        = "on on off on off off off";

    static bool  IsIIc (const std::string & machineId)
    {
        return machineId == "Apple2c" || machineId == "Apple2cPlus";
    }

    static std::filesystem::path  RequireFirmwarePath()
    {
        std::filesystem::path  folder  = PathResolver::GetDataFolder();
        std::filesystem::path  path    = folder / L"Devices" / L"SuperSerialCard" / L"SSC.rom";
        std::error_code        ec;
        std::wstring           message = std::format (
            L"Apple's Super Serial Card firmware is REQUIRED by this case and was not found in the "
            L"data folder {}. To get it, run scripts/FetchRoms.ps1", folder.wstring());



        Assert::AreEqual<uintmax_t> (2048, std::filesystem::file_size (path, ec), message.c_str());

        return path;
    }

    //  The card in slot 2, turned on, with Apple's firmware; a //c has no
    //  card, and its port 2 is left as it ships.
    static void  ConfigureCard (MachineConfig & config, const std::string & machineId)
    {
        if (IsIIc (machineId))
        {
            return;
        }

        std::filesystem::path  firmware = RequireFirmwarePath();

        for (SlotConfig & slot : config.slots)
        {
            if (slot.device == kpszSuperSerialCardDevice)
            {
                slot.slot           = kPort;
                slot.enabled        = true;
                slot.firmwarePath   = firmware.string();
                slot.firmwareStatus = SlotFirmwareStatus::Present;
                slot.switchesSw1    = kCommunicationsSw1;
                slot.switchesSw2    = kShippedSw2;
                slot.ports.clear();
            }
        }
    }

    //  A serial port's streams are the machine's line, through the stream
    //  the session's thread steps the machine with. A pause runs the machine
    //  for its length, so the loader is typed at the session's pace in the
    //  machine's time.
    struct RigStreams : IByteStreamFactory
    {
        std::shared_ptr<IByteStream>  stream;
        TestMachine                 * machine = nullptr;

        struct Shared : IByteStream
        {
            std::shared_ptr<IByteStream>  inner;
            TestMachine                 * machine = nullptr;

            void      Pause (uint64_t microseconds) override                                            { machine->RunCycles (microseconds * machine->GetConfig().clockSpeed / 1'000'000); }

            HRESULT   Write (const Byte * data, size_t count) override                                  { return inner->Write (data, count); }
            HRESULT   Read (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { return inner->Read (buffer, capacity, timeoutMs, outRead); }
            size_t    GetReadableCount() override                                                       { return inner->GetReadableCount(); }
            bool      IsClosed() const override                                                         { return inner->IsClosed(); }
            void      Close() override                                                                  { }
            uint64_t  GetTickMs() override                                                              { return inner->GetTickMs(); }
        };

        HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override { outPorts = { L"COM9" }; return S_OK; }
        HRESULT  ListenTcp    (uint16_t, LinkTransport, std::unique_ptr<IByteStream> &) override { return E_NOTIMPL; }
        HRESULT  ConnectTcp   (const std::string &, uint16_t, std::unique_ptr<IByteStream> &) override { return E_NOTIMPL; }

        HRESULT  OpenComPort (const std::wstring &, const ComSettings &, std::unique_ptr<IByteStream> & outStream) override
        {
            auto  shared = std::make_unique<Shared>();

            shared->inner   = stream;
            shared->machine = machine;
            outStream       = std::move (shared);
            return S_OK;
        }
    };

    static bool  IsIIcPlus (const std::string & machineId)
    {
        return machineId == "Apple2cPlus";
    }

    //  The machine with its two standard drives: a Disk II card's two drives
    //  on a ][, ][ plus, //e or enhanced //e; a //c's internal drive and a
    //  Disk IIc on its disk port; and a //c Plus's internal Apple 3.5 Drive
    //  and a second one on its disk port, with `drive1` and `drive2` in them
    //  through `io`.
    static std::unique_ptr<TestMachine>  MakeMachine (const std::string        & machineId,
                                                      FakeDiskFileIo           & io,
                                                      const std::vector<Byte>  & drive1,
                                                      const std::vector<Byte>  & drive2)
    {
        std::vector<PortConfig>  ports (2);



        if (!IsIIcPlus (machineId))
        {
            return std::make_unique<TestMachine> (machineId, [machineId] (MachineConfig & config)
            {
                ConfigureCard (config, machineId);

                for (PortConfig & port : config.ports)
                {
                    if (port.name == "disk")
                    {
                        port.device = kpszDiskIicDevice;
                    }
                }
            });
        }

        io.files[kD1Path]  = drive1;
        io.stamps[kD1Path] = FileStamp { drive1.size(), 1 };
        io.files[kD2Path]  = drive2;
        io.stamps[kD2Path] = FileStamp { drive2.size(), 1 };

        ports[0].name            = "internal";
        ports[0].device          = kpszApple35DriveDevice;
        ports[0].image           = kD1Path;
        ports[1].name            = "disk";
        ports[1].chain.resize (1);
        ports[1].chain[0].device = kpszApple35DriveDevice;
        ports[1].chain[0].image  = kD2Path;

        return std::make_unique<TestMachine> (machineId, ports, io);
    }

    //  An 800K ProDOS volume, with a file on it when `withFile` is set.
    static std::vector<Byte>  Make800K (const char * volume, bool withFile)
    {
        std::vector<Byte>  image (k800KBytes, 0);
        std::vector<Byte>  written;



        AssertSucceeded (ProDosSkeleton::Write (image, volume));

        if (withFile)
        {
            AssertSucceeded (ProDosVolume (image).Write (FilePath::Parse ("HELLO"), EmulatorLink::MakeBinary (1500, 9), written));
            image = std::move (written);
        }

        return image;
    }

    //  The machine at BASIC's prompt with IN#2 typed, a disk in each drive,
    //  and a session bootstrapped onto it and ready. DOS 3.3 is booted from
    //  its master in drive 1, and drive 2 has the DOS 3.3 fixture; a //c
    //  Plus has an 800K ProDOS volume in each drive and starts at
    //  Applesoft's prompt, and its drives are listed as block devices.
    struct Rig
    {
        std::string                           machineId;
        FakeDiskFileIo                        machineIo;
        std::vector<Byte>                     master;
        std::vector<Byte>                     disk;
        std::unique_ptr<TestMachine>          owned;
        TestMachine                         & machine;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        RigStreams                            streams;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        EmulatorLink::Sink                    sink;
        std::unique_ptr<CassoLinkSession>     session;
        int                                   drive2  = 2;

        explicit Rig (const char * id)
            : machineId (id),
              master    (IsIIcPlus (id) ? Make800K ("DRIVEONE", false) : GuestSession::RequireDos33Master()),
              disk      (IsIIcPlus (id) ? Make800K ("DRIVETWO", true)  : ReadFixture()),
              owned     (MakeMachine (id, machineIo, master, disk)),
              machine   (*owned)
        {
            CassoLinkSettings               settings;
            std::shared_ptr<LinkOperation>  bootstrap;



            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

            if (IsIIcPlus (machineId))
            {
                //  Neither volume is bootable: ROM 05 reports it, and
                //  Control-Reset then goes to Applesoft's prompt.
                machine.PowerCycle();
                GuestSession::StartFromPowerOn (machine);
                Assert::IsTrue (RunUntilScreenShows ("UNABLE TO FIND A BOOTABLE DISK"), Widen ("ROM 05 finds no bootable disk").c_str());

                //  ROM 05 is busy for a moment after the reset, and a key
                //  pressed then isn't read.
                machine.SoftReset();
                machine.RunCycles (3 * machine.GetConfig().clockSpeed);
                Assert::IsTrue (RunUntilScreenShows ("]"), Widen ("Applesoft's prompt after Control-Reset").c_str());
            }
            else
            {
                GuestSession::BootToPrompt (machine, master);
                AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));
            }

            InProcessByteStream::CreatePair (machineEnd, pcEnd);
            AssertSucceeded (machine.AttachSerialStream (kPort, machineEnd));

            Type ("IN#2\r");

            if (IsIIc (machineId))
            {
                Type ("\x01" "14B");
            }

            streams.stream     = std::make_shared<SteppedByteStream> (machine, pcEnd);
            streams.machine    = &machine;
            settings.name      = "Rig";
            settings.transport = LinkTransport::Serial;
            settings.port      = kComPort;

            session = std::make_unique<CassoLinkSession> (streams, folder, fileIo, sink);
            AssertSucceeded (session->StartWithBootstrap (settings, bootstrap));

            Assert::IsTrue (sink.WaitForDone (bootstrap, kBootstrapMs), Widen ("the bootstrap finishes").c_str());
            Assert::IsTrue (SUCCEEDED (bootstrap->result), Widen (std::format ("the bootstrap: {:08X} {}", (uint32_t) bootstrap->result, bootstrap->message)).c_str());
            Assert::IsTrue (sink.WaitForState (LinkState::Ready, kBootstrapMs), Widen ("the link is ready").c_str());

            if (IsIIcPlus (machineId))
            {
                EmulatorLink::Finish (sink, session->ListBlockDevices(), kFileMs);
                Assert::AreEqual<size_t> (2, session->GetBlockDevices().size(), Widen ("both 3.5-inch drives are block devices").c_str());

                drive2 = CassoLinkBlockDevices::kFirstDrive + 1;
            }
        }

        static std::vector<Byte>  ReadFixture()
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;



            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            return std::vector<Byte> (bytes.begin(), bytes.end());
        }

        ~Rig()
        {
            session.reset();
        }

        void  Type (const char * text)
        {
            for (size_t i = 0; text[i] != '\0'; i++)
            {
                AssertSucceeded (KeystrokeInjector::InjectKey (machine, (Byte) text[i]));
                machine.RunCycles (kKeyCycles);
            }
        }

        std::wstring  Widen (const std::string & text) const
        {
            return TextEncoding::NarrowToWide (machineId + ": " + text);
        }

        //  Runs the machine a tenth of a second at a time, for at most 30
        //  seconds, until `needle` is on the text screen.
        bool  RunUntilScreenShows (const std::string & needle)
        {
            for (int tenth = 0; tenth < 300; tenth++)
            {
                for (const std::string & row : TextScreenScraper::Scrape (machine))
                {
                    if (row.find (needle) != std::string::npos)
                    {
                        return true;
                    }
                }

                machine.RunCycles (machine.GetConfig().clockSpeed / 10);
            }

            return false;
        }
    };

    static std::vector<std::string>  GetNames (const VolumeListing & listing)
    {
        return EmulatorLink::GetNames (listing);
    }

    static bool  Contains (const std::vector<std::string> & names, const char * name)
    {
        return std::find (names.begin(), names.end(), std::string (name)) != names.end();
    }



    //  The session's bootstrap, and the model the session reports from the
    //  program's Ready.
    static void  RunBootstrap (const char * machineId, const wchar_t * model)
    {
        Rig  rig (machineId);



        Assert::AreEqual (std::wstring (model), rig.session->GetModelName(), rig.Widen ("the model").c_str());
    }



    //  Drive 2's catalog, a file copied from it, and a binary file copied to
    //  it, read back, renamed and deleted: a DOS 3.3 binary on a 5.25-inch
    //  disk, and a ProDOS one on the //c Plus's 800K disk.
    static void  RunFileOperations (const char * machineId)
    {
        Rig                       rig    (machineId);
        FilePayload               copied = EmulatorLink::MakeBinary (3000, 5);
        std::vector<std::string>  names;



        if (!IsIIcPlus (machineId))
        {
            copied.type = Dos33Volume::kTypeBinary;
        }

        auto  catalog = EmulatorLink::Finish (rig.sink, rig.session->Enumerate (rig.drive2, FilePath()), kFileMs);

        names = GetNames (catalog->listing);
        Assert::IsFalse (names.empty(), rig.Widen ("drive 2's catalog").c_str());

        auto  first = EmulatorLink::Finish (rig.sink, rig.session->ReadFile (rig.drive2, FilePath::Parse (names[0])), kFileMs);

        Assert::IsFalse (first->file.bytes.empty(), rig.Widen ("a file copied from the Apple: " + names[0]).c_str());

        EmulatorLink::Finish (rig.sink, rig.session->WriteFile (rig.drive2, FilePath::Parse ("COPIED"), copied), kFileMs);

        auto  back = EmulatorLink::Finish (rig.sink, rig.session->ReadFile (rig.drive2, FilePath::Parse ("COPIED")), kFileMs);

        Assert::IsTrue (back->file.bytes == copied.bytes, rig.Widen ("the file copied to the Apple reads back").c_str());

        EmulatorLink::Finish (rig.sink, rig.session->Rename (rig.drive2, FilePath::Parse ("COPIED"), "RENAMED"), kFileMs);

        catalog = EmulatorLink::Finish (rig.sink, rig.session->Enumerate (rig.drive2, FilePath()), kFileMs);
        names   = GetNames (catalog->listing);
        Assert::IsTrue (Contains (names, "RENAMED") && !Contains (names, "COPIED"), rig.Widen ("the file is renamed").c_str());

        EmulatorLink::Finish (rig.sink, rig.session->Delete (rig.drive2, FilePath::Parse ("RENAMED")), kFileMs);

        catalog = EmulatorLink::Finish (rig.sink, rig.session->Enumerate (rig.drive2, FilePath()), kFileMs);
        Assert::IsFalse (Contains (GetNames (catalog->listing), "RENAMED"), rig.Widen ("the file is deleted").c_str());
    }



    //  Drive 2 read to an image, an image written to it, and the disk read
    //  again. The //c Plus's 3.5-inch drive is read and written as a block
    //  device.
    static void  RunReadAndWriteADisk (const char * machineId)
    {
        Rig                rig     (machineId);
        bool               blocks  = IsIIcPlus (machineId);
        std::vector<Byte>  toWrite;



        if (blocks)
        {
            toWrite = Make800K ("WRITTEN", true);
        }
        else
        {
            toWrite.assign (NibbleFormat::kImageByteSize, 0);
            AssertSucceeded (Dos33Skeleton::Write (toWrite, 200));
        }

        auto  readDisk = [&rig, blocks] (const char * path)
        {
            return blocks ? rig.session->ReadBlockDevice (rig.drive2, path, false) : rig.session->ReadDisk (rig.drive2, path, false);
        };

        EmulatorLink::Finish (rig.sink, readDisk (kD2Image), kWholeDiskMs);
        Assert::IsTrue (rig.fileIo.files[kD2Image] == rig.disk, rig.Widen ("drive 2's image is its disk").c_str());

        rig.fileIo.files[kD2Written]  = toWrite;
        rig.fileIo.stamps[kD2Written] = FileStamp { toWrite.size(), 1 };

        EmulatorLink::Finish (rig.sink, blocks ? rig.session->WriteBlockDevice (rig.drive2, kD2Written, false) : rig.session->WriteDisk (rig.drive2, kD2Written), kWholeDiskMs);
        EmulatorLink::Finish (rig.sink, readDisk (kD2Again), kWholeDiskMs);
        Assert::IsTrue (rig.fileIo.files[kD2Again] == toWrite, rig.Widen ("drive 2 has the image written to it").c_str());
    }



    //  A Full Send with a disk in each drive: Space at the entry screen sends
    //  drive 1, then drive 2 when its disk is found, and Q at the pause after
    //  ends it. The keys are pressed on the PC. A Full Send reads 5.25-inch
    //  drives only, so the //c Plus, with two 3.5-inch drives, has no case
    //  here until Phase 72 adds 3.5-inch drives to it.
    static void  RunFullSend (const char * machineId)
    {
        Rig                             rig (machineId);
        std::shared_ptr<LinkOperation>  send;
        FullSendSnapshot                snapshot;
        ULONGLONG                       deadline = GetTickCount64() + kWholeDiskMs;
        bool                            master   = false;
        bool                            fixture  = false;



        send = rig.session->FullSend (kFolder, "", "");
        rig.session->PressFullSendKey (CassoLinkCodec::kKeySpace);

        do
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (kPollMs));
            snapshot = rig.session->GetFullSendStatus().GetSnapshot();
        }
        while (snapshot.disks.size() < 2 && GetTickCount64() < deadline && !rig.sink.WaitForDone (send, 0));

        Assert::AreEqual<size_t> (2, snapshot.disks.size(), rig.Widen ("both drives' disks are sent").c_str());

        rig.session->PressFullSendKey (CassoLinkCodec::kKeyQuit);
        Assert::IsTrue (rig.sink.WaitForDone (send, kWholeDiskMs), rig.Widen ("Q ends the Full Send").c_str());
        Assert::IsTrue (send->state == LinkOperationState::Canceled, rig.Widen (send->message).c_str());
        Assert::IsTrue (send->message.find ("2 sent") != std::string::npos, rig.Widen (send->message).c_str());

        for (const auto & [path, bytes] : rig.fileIo.files)
        {
            master  = master  || bytes == rig.master;
            fixture = fixture || bytes == rig.disk;
        }

        Assert::IsTrue (master,  rig.Widen ("drive 1's image is DOS 3.3's master").c_str());
        Assert::IsTrue (fixture, rig.Widen ("drive 2's image is its disk").c_str());
        Assert::IsTrue (rig.fileIo.files.contains (std::string (kFolder) + "\\manifest.fullsend"), rig.Widen ("the manifest").c_str());
    }



    TEST_METHOD (TheSessionsBootstrap_FindsAnApple2)               { RunBootstrap ("Apple2",          L"Apple ]["); }
    TEST_METHOD (TheSessionsBootstrap_FindsAnApple2Plus)             { RunBootstrap ("Apple2Plus",      L"Apple ][+"); }
    TEST_METHOD (TheSessionsBootstrap_FindsAnApple2e)                { RunBootstrap ("Apple2e",         L"Apple //e"); }
    TEST_METHOD (TheSessionsBootstrap_FindsAnApple2eEnhanced)        { RunBootstrap ("Apple2eEnhanced", L"Apple //e enhanced"); }
    TEST_METHOD (TheSessionsBootstrap_FindsAnApple2c)                { RunBootstrap ("Apple2c",         L"Apple //c (ROM 4)"); }
    TEST_METHOD (TheSessionsBootstrap_FindsAnApple2cPlus)            { RunBootstrap ("Apple2cPlus",     L"Apple //c Plus"); }

    TEST_METHOD (OnDemand_FileOperations_OnAnApple2)                 { RunFileOperations ("Apple2"); }
    TEST_METHOD (OnDemand_FileOperations_OnAnApple2Plus)             { RunFileOperations ("Apple2Plus"); }
    TEST_METHOD (OnDemand_FileOperations_OnAnApple2e)                { RunFileOperations ("Apple2e"); }
    TEST_METHOD (OnDemand_FileOperations_OnAnApple2eEnhanced)        { RunFileOperations ("Apple2eEnhanced"); }
    TEST_METHOD (OnDemand_FileOperations_OnAnApple2c)                { RunFileOperations ("Apple2c"); }
    TEST_METHOD (OnDemand_FileOperations_OnAnApple2cPlus)            { RunFileOperations ("Apple2cPlus"); }

    TEST_METHOD (OnDemand_ADiskIsReadAndWritten_OnAnApple2)          { RunReadAndWriteADisk ("Apple2"); }
    TEST_METHOD (OnDemand_ADiskIsReadAndWritten_OnAnApple2Plus)      { RunReadAndWriteADisk ("Apple2Plus"); }
    TEST_METHOD (OnDemand_ADiskIsReadAndWritten_OnAnApple2e)         { RunReadAndWriteADisk ("Apple2e"); }
    TEST_METHOD (OnDemand_ADiskIsReadAndWritten_OnAnApple2eEnhanced) { RunReadAndWriteADisk ("Apple2eEnhanced"); }
    TEST_METHOD (OnDemand_ADiskIsReadAndWritten_OnAnApple2c)         { RunReadAndWriteADisk ("Apple2c"); }
    TEST_METHOD (OnDemand_ADiskIsReadAndWritten_OnAnApple2cPlus)     { RunReadAndWriteADisk ("Apple2cPlus"); }

    TEST_METHOD (OnDemand_AFullSend_SendsBothDrives_OnAnApple2)          { RunFullSend ("Apple2"); }
    TEST_METHOD (OnDemand_AFullSend_SendsBothDrives_OnAnApple2Plus)      { RunFullSend ("Apple2Plus"); }
    TEST_METHOD (OnDemand_AFullSend_SendsBothDrives_OnAnApple2e)         { RunFullSend ("Apple2e"); }
    TEST_METHOD (OnDemand_AFullSend_SendsBothDrives_OnAnApple2eEnhanced) { RunFullSend ("Apple2eEnhanced"); }
    TEST_METHOD (OnDemand_AFullSend_SendsBothDrives_OnAnApple2c)         { RunFullSend ("Apple2c"); }
};

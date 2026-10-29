#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSuperSerialScenarioTests
//
//  The link on a //e with a Super Serial Card, over the in-process bridge to
//  the card's 6551. One case boots the link disk with the
//  card in slot 5, and copies files both ways and reads the whole disk. The
//  other starts from the BASIC prompt with IN#2 and Ctrl-A 14B typed for
//  the card's firmware, and is bootstrapped over the card.
//
//  A third case starts with no firmware involved: the monitor reads the shim
//  from the cassette input as audio, and the shim then
//  talks to the card's 6551 directly.
//
//  The typed bootstrap case requires Apple's firmware for the card, at
//  %LOCALAPPDATA%\Casso\Devices\SuperSerialCard\SSC.rom, which is not
//  committed; it fails without it, as the card's other guest-visible cases do.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkSuperSerialScenarioTests)
{
public:

    static constexpr uint64_t  kFrame  = 17030;
    static constexpr uint64_t  kBudget = 6'000'000'000;

    //  Communications mode at 9,600 baud, and the second bank as it ships.
    static constexpr const char *  kCommunicationsSw1 = "off off off on on on on";
    static constexpr const char *  kShippedSw2        = "on on off on off off off";

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
            void     Close() override                                                                             { }
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

    struct Sink : ICassoLinkSessionSink
    {
        std::mutex                                    lock;
        std::vector<std::shared_ptr<LinkOperation>>   done;

        void  OnStateChanged (LinkState) override {}
        void  OnProgress     (const std::shared_ptr<LinkOperation> &) override {}

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

    //  The card turned on in the given slot, with nothing on its connector:
    //  the test's bridge is attached once the machine is built. With
    //  firmware, Apple's, and the switches for IN# and Ctrl-A commands.
    static void  ConfigureCard (MachineConfig & config, int slotNumber, bool firmware)
    {
        for (SlotConfig & slot : config.slots)
        {
            if (slot.device == kpszSuperSerialCardDevice)
            {
                slot.slot    = slotNumber;
                slot.enabled = true;
                slot.ports.clear();

                if (firmware)
                {
                    slot.firmwarePath   = RequireFirmwarePath().string();
                    slot.firmwareStatus = SlotFirmwareStatus::Present;
                    slot.switchesSw1    = kCommunicationsSw1;
                    slot.switchesSw2    = kShippedSw2;
                }
            }
        }
    }

    static std::filesystem::path  RequireFirmwarePath()
    {
        char *                 root = nullptr;
        size_t                 len  = 0;
        std::filesystem::path  path;
        std::error_code        ec;

        if (_dupenv_s (&root, &len, "LOCALAPPDATA") == 0 && root != nullptr)
        {
            path = std::filesystem::path (root) / "Casso" / "Devices" / "SuperSerialCard" / "SSC.rom";
            free (root);
        }

        Assert::AreEqual<uintmax_t> (2048, std::filesystem::file_size (path, ec),
            L"Apple's Super Serial Card firmware is REQUIRED by this case and was not found at "
            L"%LOCALAPPDATA%\\Casso\\Devices\\SuperSerialCard\\SSC.rom. To get it, turn the card on "
            L"under Settings > Machine and accept the download");

        return path;
    }

    static void  Type (TestMachine & machine, const char * text)
    {
        for (size_t i = 0; text[i] != '\0'; i++)
        {
            machine.GetRefs().iieKeyboard->PressKey ((Byte) text[i]);
            machine.RunCycles (100'000ULL);
        }
    }

    static std::shared_ptr<LinkOperation>  Finish (TestMachine & machine, Sink & sink, std::shared_ptr<LinkOperation> operation)
    {
        uint64_t  spent = 0;

        while (!sink.IsDone (operation) && spent < kBudget)
        {
            machine.RunCycles (kFrame);
            spent += kFrame;
        }

        Assert::IsTrue (sink.IsDone (operation), L"the operation finishes");
        return operation;
    }

    //  Drive 2's disk, as DOS-logical sectors.
    static std::vector<Byte>  Sectors (TestMachine & machine)
    {
        std::vector<Byte>   file;
        std::vector<Byte>   sectors;
        SectorDecodeReport  report;

        AssertSucceeded (machine.GetDiskStore().GetImage (6, 1)->Serialize (file));
        AssertSucceeded (VolumeImage::Load (file, "d.dsk", sectors, report));
        return sectors;
    }

    static std::string  JoinRows (const std::vector<std::string> & rows)
    {
        std::string  text;

        for (const std::string & row : rows)
        {
            text += row + " | ";
        }

        return text;
    }

    static std::vector<Byte>  LoadDos33()
    {
        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;

        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        return std::vector<Byte> (bytes.begin(), bytes.end());
    }




    TEST_METHOD (TheLinkBootDisk_OverACardInSlot5_CopiesFilesBothWays_AndReadsTheDisk)
    {
        TestMachine                           machine ("Apple2e", [] (MachineConfig & config) { ConfigureCard (config, 5, false); });
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::vector<Byte>                     boot;
        std::vector<Byte>                     disk    = LoadDos33();
        LinkShimOptions                       options;
        CassoLinkSettings                     settings;
        std::vector<std::string>              screen;
        VolumeListing                         expected;
        FilePayload                           file;
        FilePayload                           payload;
        std::vector<Byte>                     written;
        std::vector<Byte>                     sectors;
        SectorDecodeReport                    report;

        options.slot = 5;
        AssertSucceeded (LinkProgram::BuildBootDisk (options, boot));

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

        GuestSession::Mount (machine, boot);
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (5, machineEnd));

        machine.RunCycles (20'000'000ULL);

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[2].starts_with ("WAITING FOR THE PC ON:"), TextEncoding::NarrowToWide (screen[0] + " | " + screen[2]).c_str());
        Assert::IsTrue (screen[4].starts_with ("SSC, SLOT 5, 115200"), TextEncoding::NarrowToWide (screen[4]).c_str());

        settings.name      = "//e";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6505";

        AssertSucceeded (Dos33Volume (disk).Enumerate (expected));
        Assert::IsFalse (expected.entries.empty());
        AssertSucceeded (Dos33Volume (disk).Read (FilePath::Parse (expected.entries[0].name), file));

        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x6000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (1500, 0x5A);
        AssertSucceeded (Dos33Volume (disk).Write (FilePath::Parse ("FROMPC"), payload, written));

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            AssertSucceeded (session.Start (settings));

            auto  list = Finish (machine, sink, session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::AreEqual (expected.entries.size(), list->listing.entries.size(), L"drive 2's catalog over the card");

            auto  read = Finish (machine, sink, session.ReadFile (2, FilePath::Parse (expected.entries[0].name)));

            AssertSucceeded (read->result);
            Assert::IsTrue (read->file.bytes == file.bytes, L"a file out over the card");

            auto  write = Finish (machine, sink, session.WriteFile (2, FilePath::Parse ("FROMPC"), payload));

            AssertSucceeded (write->result);
            Assert::IsTrue (Sectors (machine) == written, L"a file in over the card");

            auto  image = Finish (machine, sink, session.ReadDisk (2, "drive2.dsk", false));

            AssertSucceeded (image->result);
            AssertSucceeded (VolumeImage::Load (fileIo.files["drive2.dsk"], "drive2.dsk", sectors, report));
            Assert::IsTrue (sectors == written, L"the whole disk over the card");

            screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
            Assert::IsTrue (screen[2].find ("SSC, SLOT 5, 115200") != std::string::npos, TextEncoding::NarrowToWide (screen[2]).c_str());
        }

        Assert::AreEqual<Byte> (0x50, machine.GetCpu()->ReadByte (0x0006), L"the program's 6551 is slot 5's");
    }



    //  The universal disk finds the card in slot 5 by its firmware's
    //  identification bytes and links over it.
    TEST_METHOD (TheUniversalDisk_FindsACardInSlot5_AndListsADisk)
    {
        TestMachine                           machine ("Apple2e", [] (MachineConfig & config) { ConfigureCard (config, 5, true); });
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::vector<Byte>                     boot;
        std::vector<Byte>                     disk = LoadDos33();
        LinkShimOptions                       options;
        CassoLinkSettings                     settings;
        std::vector<std::string>              screen;
        VolumeListing                         expected;

        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, boot));

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

        GuestSession::Mount (machine, boot);
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (5, machineEnd));

        machine.RunCycles (20'000'000ULL);

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[2].starts_with ("WAITING FOR THE PC ON:"), TextEncoding::NarrowToWide (screen[0] + " | " + screen[2]).c_str());
        Assert::IsTrue (screen[4].starts_with ("SSC, SLOT 5, 115200"), TextEncoding::NarrowToWide (screen[4]).c_str());

        settings.name      = "//e";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6505";

        AssertSucceeded (Dos33Volume (disk).Enumerate (expected));

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            AssertSucceeded (session.Start (settings));

            auto  list = Finish (machine, sink, session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::AreEqual (expected.entries.size(), list->listing.entries.size(), L"drive 2's catalog over the card");
        }

        Assert::AreEqual<Byte> (0x50, machine.GetCpu()->ReadByte (0x0006), L"the program's 6551 is slot 5's");
    }



    //  The card's firmware in slot 5 has the //c's IN# and Ctrl-A 14B, except
    //  that it executes the command at the Return after it; the session then
    //  types the loader in through the card, and the slot finder takes slot 5
    //  from the input hook IN#5 set.
    TEST_METHOD (ABareIIe_IsBootstrappedThroughTheCardsFirmware_AndListsADisk)
    {
        TestMachine                           machine ("Apple2e", [] (MachineConfig & config) { ConfigureCard (config, 5, true); });
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::vector<Byte>                     disk = LoadDos33();
        CassoLinkSettings                     settings;
        std::shared_ptr<LinkOperation>        bootstrap;
        std::vector<std::string>              screen;

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();
        machine.RunCycles (2'000'000ULL);

        //  At the BASIC prompt, as the user finds it.
        machine.GetCpu()->SetPC (0xE000);
        machine.RunCycles (2'000'000ULL);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (5, machineEnd));

        Type (machine, "IN#5\r");
        Type (machine, "\x01" "14B\r");

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        settings.name      = "//e";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            session.SetBootstrapPause (40);
            AssertSucceeded (session.StartWithBootstrap (settings, bootstrap));

            Finish (machine, sink, bootstrap);

            screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
            Assert::IsTrue (SUCCEEDED (bootstrap->result), TextEncoding::NarrowToWide (JoinRows (screen)).c_str());

            auto  list = Finish (machine, sink, session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::IsFalse (list->listing.entries.empty(), L"drive 2's catalog over the bootstrapped link");
        }

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[0].starts_with ("CASSOLINK"), L"the program's status screen");
    }



    TEST_METHOD (ABareIIe_IsBootstrappedThroughTheCassetteInput_AndListsADisk)
    {
        TestMachine                           machine ("Apple2e", [] (MachineConfig & config) { ConfigureCard (config, 5, true); });
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::vector<Byte>                     disk    = LoadDos33();
        std::vector<Byte>                     wav;
        std::string                           command;
        LinkShimOptions                       options;
        NullTapeAudioDecoder                  mp3;
        TapeImage                             image;
        std::string                           error;
        CassoLinkSettings                     settings;
        std::vector<std::string>              screen;
        double                                seconds = 0.0;

        AssertSucceeded (LinkAudioBootstrap::BuildWav (options, wav, command));
        Assert::AreEqual (std::string ("3AA0.3E6CR 3AA0G\r"), command, L"the slot finder ahead of the shim");
        Assert::AreEqual (S_OK, TapeImageLoader::Load (wav, "bootstrap.wav", false, mp3, image, error), TextEncoding::NarrowToWide (error).c_str());

        seconds = (double) image.signal.lengthSamples / image.signal.sampleRate;

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();
        machine.RunCycles (2'000'000ULL);

        //  At the BASIC prompt, as the user finds it.
        machine.GetCpu()->SetPC (0xE000);
        machine.RunCycles (2'000'000ULL);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (5, machineEnd));
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        Type (machine, "CALL -151\r");
        Type (machine, command.c_str());

        machine.GetTapeDeck().Insert (std::move (image));
        machine.GetTapeDeck().Play (*machine.GetCpu()->GetBusCyclePtr());
        machine.RunCycles ((uint64_t) ((seconds + 1.0) * machine.GetConfig().clockSpeed));
        machine.GetTapeDeck().Update (*machine.GetCpu()->GetBusCyclePtr());

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[4].starts_with ("SSC, SLOT 5, 115200"), TextEncoding::NarrowToWide (JoinRows (screen)).c_str());

        settings.name      = "//e";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            AssertSucceeded (session.Start (settings));

            auto  list = Finish (machine, sink, session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::IsFalse (list->listing.entries.empty(), L"drive 2's catalog over the link the cassette started");
        }
    }
};

#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "PacedRun.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkFault.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Tape/ITapeAudioDecoder.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"
#include "Seams/Win32ByteStreamFactory.h"
#include "Seams/Win32HostNetwork.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFaultSessionTests
//
//  Faults injected into whole sessions, as a Debug build of Casso Explorer
//  arms them: over a //e's Super Serial Card, a //e's Uthernet II, the
//  cassette input and a //c's serial bootstrap. Each fault is taken from the
//  process's list as the operation, the bootstrap or the cassette audio
//  starts, and the link recovers or fails with its error.
//
//  The cassette case requires Apple's firmware for the card, at
//  %LOCALAPPDATA%\Casso\Devices\SuperSerialCard\SSC.rom, as the card's other
//  cassette case does.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkFaultSessionTests)
{
public:

    static constexpr uint64_t  kFrame        = 17030;
    static constexpr uint64_t  kBudget       = 6'000'000'000;
    static constexpr uint64_t  kBootCycles   = 20'000'000;
    static constexpr uint16_t  kNetworkPort  = 26503;
    static constexpr DWORD     kNetworkWait  = 120'000;
    static constexpr DWORD     kRetryMs      = 500;

    //  Mid-sector in the reply to the operation's sixth request, and in what
    //  the PC sends.
    static constexpr const char *  kAppleFlip = "apple:flip@1000#5";
    static constexpr const char *  kPcFlip    = "pc:flip@300";

    //  Faults on for the test, and off with none left armed after it, so the
    //  other cases' programs have no Fault command.
    struct Enabled
    {
        Enabled()  { LinkFaults::Enable(); }
        ~Enabled() { LinkFaults::Disable(); }
    };

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
        std::vector<std::string>                      activity;

        void  OnStateChanged (LinkState) override {}
        void  OnProgress     (const std::shared_ptr<LinkOperation> &) override {}

        void  OnActivity (const std::string & text) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            activity.push_back (text);
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

        bool  HasActivity (const char * text)
        {
            std::lock_guard<std::mutex>  guard (lock);

            return std::any_of (activity.begin(), activity.end(), [text] (const std::string & line) { return line.find (text) != std::string::npos; });
        }
    };

    static void  Arm (const char * text)
    {
        std::vector<LinkFault>  faults;



        Assert::AreEqual (S_OK, LinkFault::Parse (text, faults));
        LinkFaults::Arm (faults);
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

    static std::vector<Byte>  LoadDos33()
    {
        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        return std::vector<Byte> (bytes.begin(), bytes.end());
    }

    static FilePayload  Binary (size_t length, Byte seed)
    {
        FilePayload  payload;



        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x6000;
        payload.hasLoadAddress = true;
        payload.bytes.resize (length);

        for (size_t i = 0; i < length; i++)
        {
            payload.bytes[i] = (Byte) (seed + i * 5 + (i >> 8));
        }

        return payload;
    }

    //  The card in the slot, as the card's link disk cases have it, with
    //  Apple's firmware when `firmware` is true.
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
                    slot.switchesSw1    = "off off off on on on on";
                    slot.switchesSw2    = "on on off on off off off";
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

    //  A //e with the card in slot 5 booted from the link disk, and drive 2's
    //  DOS 3.3 disk.
    static void  BootOverACard (TestMachine & machine, Factory & factory, const std::vector<Byte> & disk)
    {
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::vector<Byte>                     boot;
        LinkShimOptions                       options;



        options.slot = 5;
        AssertSucceeded (LinkProgram::BuildBootDisk (options, boot));

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

        GuestSession::Mount (machine, boot);
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (5, machineEnd));

        machine.RunCycles (kBootCycles);
    }

    static CassoLinkSettings  EmulatedSettings (const char * name)
    {
        CassoLinkSettings  settings;



        settings.name      = name;
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6505";
        settings.use40     = true;
        return settings;
    }



    //  Over the card's 6551: a byte of the disk the Apple sends, flipped,
    //  and the track read again.
    TEST_METHOD (OverACard_AnAppleFlipInADiskRead_IsReadAgain)
    {
        Enabled                  enabled;
        TestMachine              machine ("Apple2e", [] (MachineConfig & config) { ConfigureCard (config, 5, false); });
        Factory                  factory;
        InMemoryCassoLinkFolder  folder;
        FakeDiskFileIo           fileIo;
        Sink                     sink;
        std::vector<Byte>        disk    = LoadDos33();
        std::vector<Byte>        sectors;
        SectorDecodeReport       report;



        BootOverACard (machine, factory, disk);

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            AssertSucceeded (session.Start (EmulatedSettings ("//e")));
            AssertSucceeded (Finish (machine, sink, session.Enumerate (2, FilePath()))->result);

            Arm (kAppleFlip);

            AssertSucceeded (Finish (machine, sink, session.ReadDisk (2, "drive2.dsk", false))->result);
            AssertSucceeded (VolumeImage::Load (fileIo.files["drive2.dsk"], "drive2.dsk", sectors, report));
            Assert::IsTrue (sectors == Sectors (machine), L"the whole disk");
        }

        Assert::IsTrue (sink.HasActivity ("Sent a fault to the Apple"));
        Assert::IsTrue (sink.HasActivity ("damaged"), L"read again");
    }



    //  Over the card's 6551: a byte the PC sends while writing a file,
    //  flipped, and sent again.
    TEST_METHOD (OverACard_APcFlipInAWrite_IsSentAgain)
    {
        Enabled                  enabled;
        TestMachine              machine ("Apple2e", [] (MachineConfig & config) { ConfigureCard (config, 5, false); });
        Factory                  factory;
        InMemoryCassoLinkFolder  folder;
        FakeDiskFileIo           fileIo;
        Sink                     sink;
        std::vector<Byte>        disk    = LoadDos33();
        std::vector<Byte>        written;
        FilePayload              payload = Binary (1500, 9);



        BootOverACard (machine, factory, disk);
        AssertSucceeded (Dos33Volume (disk).Write (FilePath::Parse ("FROMPC"), payload, written));

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            AssertSucceeded (session.Start (EmulatedSettings ("//e")));
            AssertSucceeded (Finish (machine, sink, session.Enumerate (2, FilePath()))->result);

            Arm (kPcFlip);

            AssertSucceeded (Finish (machine, sink, session.WriteFile (2, FilePath::Parse ("FROMPC"), payload))->result);
            Assert::IsTrue (Sectors (machine) == written, L"drive 2 has the file");
        }

        Assert::IsTrue (sink.HasActivity ("Injected a fault"));
    }



    //  A byte of the cassette audio's data flipped: the monitor's checksum
    //  fails and it reports ERR, the shim doesn't start, and the slot finder
    //  has the monitor read the tape again. The audio sent again, with
    //  nothing typed, starts it.
    TEST_METHOD (ACassetteFlip_FailsTheMonitorsCheck_AndTheAudioSentAgainLoads)
    {
        Enabled                               enabled;
        TestMachine                           machine ("Apple2e", [] (MachineConfig & config) { ConfigureCard (config, 5, true); });
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        std::vector<std::string>              reports;
        std::vector<std::string>              screen;
        std::string                           shown;



        LinkFaults::SetReporter ([&reports] (const std::string & text) { reports.push_back (text); });
        Arm ("cassette:flip@100");

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();
        machine.RunCycles (2'000'000ULL);

        //  At the BASIC prompt, as the user finds it.
        machine.GetCpu()->SetPC (0xE000);
        machine.RunCycles (2'000'000ULL);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (5, machineEnd));

        Type (machine, "CALL -151\r");
        PlayCassette (machine, true);

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);

        for (const std::string & row : screen)
        {
            shown += row + " | ";
        }

        LinkFaults::SetReporter (nullptr);

        Assert::AreEqual ((size_t) 1, reports.size(), L"the fault was injected");
        Assert::IsTrue (shown.find ("ERR") != std::string::npos, TextEncoding::NarrowToWide (shown).c_str());
        Assert::IsTrue (shown.find ("SSC, SLOT 5") == std::string::npos, L"the shim didn't start");

        PlayCassette (machine, false);

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[4].starts_with ("SSC, SLOT 5, 115200"), TextEncoding::NarrowToWide (screen[4]).c_str());
    }

    //  The cassette audio sent to the machine's tape input, after the
    //  monitor's command when it is to be typed.
    static void  PlayCassette (TestMachine & machine, bool typeCommand)
    {
        std::vector<Byte>     wav;
        std::string           command;
        LinkShimOptions       options;
        NullTapeAudioDecoder  mp3;
        TapeImage             image;
        std::string           error;
        double                seconds = 0.0;



        AssertSucceeded (LinkAudioBootstrap::BuildWav (options, wav, command));
        Assert::AreEqual (S_OK, TapeImageLoader::Load (wav, "bootstrap.wav", false, mp3, image, error), TextEncoding::NarrowToWide (error).c_str());

        seconds = (double) image.signal.lengthSamples / image.signal.sampleRate;

        if (typeCommand)
        {
            Type (machine, command.c_str());
        }

        machine.GetTapeDeck().Insert (std::move (image));
        machine.GetTapeDeck().Play (*machine.GetCpu()->GetBusCyclePtr());
        machine.RunCycles ((uint64_t) ((seconds + 1.0) * machine.GetConfig().clockSpeed));
        machine.GetTapeDeck().Update (*machine.GetCpu()->GetBusCyclePtr());
    }



    //  A byte of the shim the serial bootstrap sends, flipped: the loader's
    //  XOR fails, it returns to the BASIC prompt, and the bootstrap fails. A
    //  link started with no Bootstrap chosen then sends its probe, which the
    //  Apple runs at the prompt, and the Apple is bootstrapped and linked.
    TEST_METHOD (ABootstrapFlip_FailsTheLoadersCheck_AndTheAppleIsBootstrappedAgainFromThePrompt)
    {
        Enabled                               enabled;
        TestMachine                           machine ("Apple2c");
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::vector<std::string>              lines;
        size_t                                typed    = 0;
        HRESULT                               result   = S_OK;
        std::shared_ptr<LinkOperation>        bootstrap;
        CassoLinkSettings                     settings = EmulatedSettings ("//c");



        AssertSucceeded (LinkBootstrap::BuildLines (lines));

        for (const std::string & line : lines)
        {
            typed += line.size();
        }

        //  The shim's first page, after the lines and the page count.
        Arm (std::format ("bootstrap:flip@{}", typed + 1 + 40).c_str());

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();
        machine.RunCycles (2'000'000ULL);

        //  At the BASIC prompt, with the port readied.
        machine.GetCpu()->SetPC (0xE000);
        machine.RunCycles (2'000'000ULL);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

        Type (machine, "IN#2\r");
        Type (machine, "\x01" "14B");

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, LoadDos33()));

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            session.SetBootstrapPause (40);
            AssertSucceeded (session.StartWithBootstrap (settings, bootstrap));

            result = Finish (machine, sink, bootstrap)->result;
            Assert::IsTrue (FAILED (result), L"the loader's XOR failed");
        }

        Assert::IsTrue (sink.HasActivity ("Injected a fault"));

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            session.SetBootstrapPause (40);
            AssertSucceeded (session.Start (settings));
            AssertSucceeded (Finish (machine, sink, session.Enumerate (2, FilePath()))->result);
        }
    }



    //  Over the W5100 and a real loopback socket: a byte of the disk the
    //  Apple sends, flipped, and a byte the PC sends, flipped, each
    //  recovered from.
    TEST_METHOD (OverAnUthernet_FlipsFromEitherEnd_AreRecoveredFrom)
    {
        Enabled                  enabled;
        Win32HostNetwork         network;
        TestMachine              machine ("Apple2eEnhanced", TestMachine::Slots::AsShipped,
                                          [&network] (MachineConfig & config, MachineBuildServices & services)
                                          {
                                              for (SlotConfig & slot : config.slots)
                                              {
                                                  if (slot.device == "uthernet2")
                                                  {
                                                      slot.enabled = true;
                                                  }
                                              }

                                              services.hostNetwork = &network;
                                          });
        Win32ByteStreamFactory   factory;
        InMemoryCassoLinkFolder  folder;
        FakeDiskFileIo           fileIo;
        Sink                     sink;
        std::vector<Byte>        disk    = LoadDos33();
        std::vector<Byte>        boot;
        std::vector<Byte>        sectors;
        std::vector<Byte>        written;
        SectorDecodeReport       report;
        LinkShimOptions          options;
        CassoLinkSettings        settings;
        FilePayload              payload = Binary (1500, 3);



        options.chip            = LinkChip::W5100;
        options.slot            = 3;
        options.network.address = { 127, 0, 0, 1 };
        options.network.port    = kNetworkPort;

        AssertSucceeded (LinkProgram::BuildBootDisk (options, boot));

        GuestSession::Mount (machine, boot);
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));
        machine.RunCycles (kBootCycles);

        settings.name      = "Uthernet";
        settings.transport = LinkTransport::Uthernet;
        settings.address   = std::format ("127.0.0.1:{}", kNetworkPort);
        settings.use40     = true;

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            session.SetRetryInterval (kRetryMs);
            AssertSucceeded (session.Start (settings));
            AssertSucceeded (FinishInRealTime (machine, sink, session.Enumerate (2, FilePath()))->result);

            Arm (kAppleFlip);

            AssertSucceeded (FinishInRealTime (machine, sink, session.ReadDisk (2, "drive2.dsk", false))->result);
            AssertSucceeded (VolumeImage::Load (fileIo.files["drive2.dsk"], "drive2.dsk", sectors, report));
            Assert::IsTrue (sectors == Sectors (machine), L"the whole disk");
            Assert::IsTrue (sink.HasActivity ("damaged"), L"read again");

            AssertSucceeded (Dos33Volume (sectors).Write (FilePath::Parse ("FROMPC"), payload, written));
            Arm (kPcFlip);

            AssertSucceeded (FinishInRealTime (machine, sink, session.WriteFile (2, FilePath::Parse ("FROMPC"), payload))->result);
            Assert::IsTrue (Sectors (machine) == written, L"drive 2 has the file");
        }

        Assert::IsTrue (sink.HasActivity ("Sent a fault to the Apple"));
        Assert::IsTrue (sink.HasActivity ("Injected a fault"));
    }

    //  The guest as fast as the PC allows, in real time only while it waits
    //  on the PC, until the operation finishes.
    static std::shared_ptr<LinkOperation>  FinishInRealTime (TestMachine & machine, Sink & sink, std::shared_ptr<LinkOperation> operation)
    {
        TestMachine  * machines[] = { &machine };



        PacedRun::RunUntil (machines, [&] () { return sink.IsDone (operation); }, kNetworkWait);

        Assert::IsTrue (sink.IsDone (operation), L"the operation finishes");
        return operation;
    }
};

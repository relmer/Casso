#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkConnector.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBootstrapScenarioTests
//
//  A //c with no link program, at the BASIC prompt with IN#2 and Control-A
//  14B typed as the user would: the session types the loader into it over
//  the in-process bridge, the shim loads the program, and the program's
//  status screen comes up and lists a disk.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkBootstrapScenarioTests)
{
public:

    static constexpr uint64_t  kFrame   = 17030;
    static constexpr uint64_t  kBudget  = 6'000'000'000;
    static constexpr uint64_t  kClockHz = 1'020'484;

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

    static void  Type (TestMachine & machine, const char * text)
    {
        for (size_t i = 0; text[i] != '\0'; i++)
        {
            machine.GetRefs().iieKeyboard->PressKey ((Byte) text[i]);
            machine.RunCycles (100'000ULL);
        }
    }

    static std::shared_ptr<LinkOperation>  RunUntilDone (TestMachine & machine, Sink & sink, std::shared_ptr<LinkOperation> operation)
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

    //  Slot 2's screen holes in main memory, $47A to $7FA, one in each 128
    //  bytes of the page; then every screen hole in auxiliary memory, where
    //  the //c has each port's settings from power-on. 80STORE and PAGE2
    //  select auxiliary memory for the text page while it is read.
    static void  ReadPortHoles (TestMachine & machine, std::vector<Byte> & outHoles)
    {
        outHoles.clear();

        for (Word at = 0x047A; at < 0x0800; at += 0x80)
        {
            outHoles.push_back (machine.GetMemoryBus().ReadByte (at));
        }

        machine.GetMemoryBus().WriteByte (0xC001, 0);
        machine.GetMemoryBus().ReadByte (0xC055);

        for (Word slot = 0; slot < 8; slot++)
        {
            for (Word at = 0x0478 + slot; at < 0x0800; at += 0x80)
            {
                outHoles.push_back (machine.GetMemoryBus().ReadByte (at));
            }
        }

        machine.GetMemoryBus().ReadByte (0xC054);
        machine.GetMemoryBus().WriteByte (0xC000, 0);
    }




    TEST_METHOD (ABareApple_IsBootstrapped_AndListsADisk)
    {
        TestMachine                           machine ("Apple2c");
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        FixtureProvider                       fixtures;
        std::vector<uint8_t>                  bytes;
        std::vector<Byte>                     disk;
        CassoLinkSettings                     settings;
        std::shared_ptr<LinkOperation>        bootstrap;
        uint64_t                              spent = 0;
        std::vector<std::string>              screen;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        disk.assign (bytes.begin(), bytes.end());

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();
        machine.RunCycles (2'000'000ULL);

        //  At the BASIC prompt, as the user finds it, with the port readied.
        machine.GetCpu()->SetPC (0xE000);
        machine.RunCycles (2'000'000ULL);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

        Type (machine, "IN#2\r");
        Type (machine, "\x01" "14B");

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        settings.name      = "//c";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            session.SetBootstrapPause (40);
            settings.use40 = true;
            AssertSucceeded (session.StartWithBootstrap (settings, bootstrap));

            while (!sink.IsDone (bootstrap) && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
            }

            Assert::IsTrue (sink.IsDone (bootstrap), L"the bootstrap finishes");
            AssertSucceeded (bootstrap->result);

            auto  list = session.Enumerate (2, FilePath());

            spent = 0;

            while (!sink.IsDone (list) && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
            }

            Assert::IsTrue (sink.IsDone (list));
            AssertSucceeded (list->result);
            Assert::IsFalse (list->listing.entries.empty(), L"drive 2's catalog over the bootstrapped link");
        }

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[0].starts_with ("CASSOLINK"), L"the program's status screen");
    }



    //  The //c's port 2 settings are in its screen holes, the 8 bytes after
    //  each 120 of the text page, and each port's settings from power-on are
    //  in the holes in auxiliary memory. After a bootstrap, a link in 80
    //  columns and Escape, they match their values before the bootstrap, so
    //  the port and its Ctrl-A work.
    TEST_METHOD (AfterEscape_ThePortsScreenHoles_MatchTheirValuesBeforeTheBootstrap)
    {
        TestMachine                           machine ("Apple2c");
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        FixtureProvider                       fixtures;
        std::vector<uint8_t>                  bytes;
        std::vector<Byte>                     disk;
        CassoLinkSettings                     settings;
        std::shared_ptr<LinkOperation>        bootstrap;
        uint64_t                              spent  = 0;
        std::vector<Byte>                     before;
        std::vector<Byte>                     after;
        std::vector<std::string>              screen;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        disk.assign (bytes.begin(), bytes.end());

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();
        machine.RunCycles (2'000'000ULL);

        machine.GetCpu()->SetPC (0xE000);
        machine.RunCycles (2'000'000ULL);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

        Type (machine, "IN#2\r");
        Type (machine, "\x01" "14B");

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        ReadPortHoles (machine, before);
        Assert::IsTrue (std::any_of (before.begin(), before.end(), [] (Byte b) { return b != 0xA0; }), L"IN#2 set the port's holes");

        settings.name      = "//c";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";
        settings.use40     = false;

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            session.SetBootstrapPause (40);
            AssertSucceeded (session.StartWithBootstrap (settings, bootstrap));

            while (!sink.IsDone (bootstrap) && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
            }

            AssertSucceeded (bootstrap->result);

            auto  list = session.Enumerate (2, FilePath());

            spent = 0;

            while (!sink.IsDone (list) && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
            }

            AssertSucceeded (list->result);

            screen = TextScreenScraper::Scrape (machine);
            Assert::AreEqual<size_t> (TextScreenScraper::kCols80, screen[0].size(), L"the program's screen in 80 columns");
            Assert::AreNotEqual (std::string::npos, screen[0].find ("CASSOLINK"), L"the program's status screen");

            machine.GetRefs().iieKeyboard->PressKey (0x1B);
            machine.RunCycles (2 * kClockHz);
        }

        screen = TextScreenScraper::Scrape (machine);
        Assert::AreEqual (std::string::npos, screen[0].find ("CASSOLINK"), L"Escape ended CassoLink");

        ReadPortHoles (machine, after);

        for (size_t i = 0; i < before.size(); i++)
        {
            std::wstring  where = (i < 8) ? std::format (L"main memory's ${:03X}", 0x47A + i * 0x80)
                                          : std::format (L"auxiliary memory's ${:03X}", 0x478 + (i - 8) / 8 + ((i - 8) % 8) * 0x80);

            Assert::AreEqual<int> (before[i], after[i], (L"the screen hole at " + where).c_str());
        }
    }



    //  DOS 3.3 booted on the //c, IN#2 and Ctrl-A 14B, and a link started with
    //  no Bootstrap chosen. DOS sets KSW to its input routine. The prompt
    //  probe's $A5 arrives and the Apple is bootstrapped, and RWTS at $BD00
    //  to $BFFF is intact afterward. After both drives' catalogs over the
    //  link in 80 columns and Escape, DOS's CATALOG reads drive 1 with no I/O
    //  error.

    TEST_METHOD (UnderDos_ThePromptProbe_BootstrapsTheApple_AndDosCatalogsAfterEscape)
    {
        TestMachine                           machine ("Apple2c");
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        FixtureProvider                       fixtures;
        std::vector<uint8_t>                  bytes;
        std::vector<Byte>                     disk;
        std::vector<Byte>                     dos;
        CassoLinkSettings                     settings;
        std::vector<std::string>              screen;
        std::string                           text;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        disk.assign (bytes.begin(), bytes.end());

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "master.dsk", DiskFormat::Dsk, GuestSession::RequireDos33Master()));
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));
        machine.RunCycles (15 * kClockHz);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

        Type (machine, "IN#2\r");
        Type (machine, "\x01" "14B");

        for (Word at = 0xBD00; at <= 0xBFFF; at++)
        {
            dos.push_back (machine.GetMemoryBus().ReadByte (at));
        }

        settings.name      = "//c";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";
        settings.use40     = false;

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            session.SetBootstrapPause (40);
            AssertSucceeded (session.Start (settings));

            AssertSucceeded (RunUntilDone (machine, sink, session.Enumerate (1, FilePath()))->result);
            AssertSucceeded (RunUntilDone (machine, sink, session.Enumerate (2, FilePath()))->result);

            machine.GetRefs().iieKeyboard->PressKey (0x1B);
            machine.RunCycles (2 * kClockHz);
        }

        for (size_t i = 0; i < dos.size(); i++)
        {
            Word  at = (Word) (0xBD00 + i);

            Assert::AreEqual<int> (dos[i], machine.GetMemoryBus().ReadByte (at), std::format (L"RWTS at ${:04X}", at).c_str());
        }

        Type (machine, "CATALOG\r");
        machine.RunCycles (5 * kClockHz);

        screen = TextScreenScraper::Scrape (machine);

        for (const std::string & row : screen)
        {
            text += row + "|";
        }

        Assert::AreEqual (std::string::npos, text.find ("ERROR"), TextEncoding::NarrowToWide (text).c_str());
        Assert::AreNotEqual (std::string::npos, text.find ("HELLO"), TextEncoding::NarrowToWide (text).c_str());
    }



    //  A link started with no Bootstrap chosen, on an Apple at the BASIC
    //  prompt with IN#2 and Ctrl-A 14B typed: the connection doesn't get a reply,
    //  the prompt probe's $A5 comes back, and the Apple is bootstrapped.
    TEST_METHOD (AnAppleWaitingAtThePrompt_IsFoundByTheProbe_AndBootstrapped)
    {
        TestMachine                           machine ("Apple2c");
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        FixtureProvider                       fixtures;
        std::vector<uint8_t>                  bytes;
        std::vector<Byte>                     disk;
        CassoLinkSettings                     settings;
        uint64_t                              spent = 0;
        std::vector<std::string>              screen;



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        disk.assign (bytes.begin(), bytes.end());

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
        machine.PowerCycle();
        machine.RunCycles (2'000'000ULL);

        machine.GetCpu()->SetPC (0xE000);
        machine.RunCycles (2'000'000ULL);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

        Type (machine, "IN#2\r");
        Type (machine, "\x01" "14B");

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        settings.name      = "//c";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            session.SetBootstrapPause (40);
            settings.use40 = true;
            AssertSucceeded (session.Start (settings));

            auto  list = session.Enumerate (2, FilePath());

            while (!sink.IsDone (list) && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
            }

            Assert::IsTrue (sink.IsDone (list));
            AssertSucceeded (list->result);
        }

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[0].starts_with ("CASSOLINK"), L"the program's status screen");
    }



    //  The disk Save link to disk makes boots into the shim at the link's
    //  speed; the shim has no program, so the session loads it.
    TEST_METHOD (TheLinkBootDisk_Boots_AndTheProgramIsLoaded)
    {
        TestMachine                           machine ("Apple2c");
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::vector<Byte>                     boot;
        CassoLinkSettings                     settings;
        uint64_t                              spent = 0;
        std::vector<std::string>              screen;



        AssertSucceeded (LinkProgram::BuildBootDisk (CassoLinkSettings::kDefaultBaud, boot));

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

        GuestSession::Mount (machine, boot);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

        machine.RunCycles (20'000'000ULL);

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[2].starts_with ("WAITING FOR THE PC ON:"), TextEncoding::NarrowToWide (screen[0] + " | " + screen[1] + " | " + screen[2]).c_str());
        Assert::IsTrue (screen[4].starts_with ("SERIAL PORT 2, 115200"), TextEncoding::NarrowToWide (screen[4]).c_str());

        settings.name      = "//c";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            settings.use40 = true;

            AssertSucceeded (session.Start (settings));

            auto  list = session.Enumerate (1, FilePath());

            while (!sink.IsDone (list) && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
            }

            Assert::IsTrue (sink.IsDone (list));
            Assert::AreEqual (LinkProgram::kProgramVersion, (Word) (machine.GetCpu()->ReadByte (0x0803) | (machine.GetCpu()->ReadByte (0x0804) << 8)), L"the program was loaded");
        }

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[0].starts_with ("CASSOLINK"));
    }



    //  The universal disk lists both of the //c's ports and waits on both;
    //  the PC on port 1 has the shim started there.
    TEST_METHOD (TheUniversalDisk_ListsBothPorts_AndLinksOverPort1)
    {
        TestMachine                           machine ("Apple2c");
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::vector<Byte>                     boot;
        LinkShimOptions                       options;
        CassoLinkSettings                     settings;
        uint64_t                              spent = 0;
        std::vector<std::string>              screen;



        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, boot));

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

        GuestSession::Mount (machine, boot);

        InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (1, machineEnd));

        machine.RunCycles (20'000'000ULL);

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::IsTrue (screen[2].starts_with ("WAITING FOR THE PC ON:"), TextEncoding::NarrowToWide (screen[0] + " | " + screen[2]).c_str());
        Assert::IsTrue (screen[4].starts_with ("SERIAL PORT 1, 115200"), TextEncoding::NarrowToWide (screen[4]).c_str());
        Assert::IsTrue (screen[5].starts_with ("SERIAL PORT 2, 115200"), TextEncoding::NarrowToWide (screen[5]).c_str());

        settings.name      = "//c";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6501";

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            settings.use40 = true;

            AssertSucceeded (session.Start (settings));

            auto  list = session.Enumerate (1, FilePath());

            while (!sink.IsDone (list) && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;
            }

            Assert::IsTrue (sink.IsDone (list));
            Assert::AreEqual (LinkProgram::kProgramVersion, (Word) (machine.GetCpu()->ReadByte (0x0803) | (machine.GetCpu()->ReadByte (0x0804) << 8)), L"the program was loaded");
        }

        Assert::AreEqual<Byte> (0x10, machine.GetCpu()->ReadByte (0x0006), L"the program's 6551 is port 1's");
    }



    //  A Wi-Fi modem between the //c and the PC: it reads the dial command,
    //  replies CONNECT as a modem does while `answer` is true, and then passes
    //  bytes both ways.
    struct Modem
    {
        std::shared_ptr<InProcessByteStream>  serial;
        std::shared_ptr<InProcessByteStream>  network;
        std::mutex                            lock;
        std::string                           dialed;
        std::atomic<bool>                     answer { true };
        std::jthread                          thread;

        void  Run (std::stop_token stop)
        {
            Byte         chunk[256];
            size_t       got     = 0;
            std::string  command;
            bool         online  = false;
            HRESULT      read    = S_OK;

            while (!stop.stop_requested())
            {
                read = serial->Read (chunk, sizeof (chunk), 5, got);

                if (SUCCEEDED (read) && got > 0)
                {
                    if (online)
                    {
                        network->Write (chunk, got);
                    }
                    else
                    {
                        command.append (chunk, chunk + got);
                    }
                }

                size_t  end = command.find ('\r');

                if (!online && answer && end != std::string::npos)
                {
                    {
                        std::lock_guard<std::mutex>  guard (lock);

                        dialed = command.substr (0, end);
                    }

                    serial->Write (reinterpret_cast<const Byte *> ("CONNECT\r\n"), 9);
                    online = true;
                }

                read = online ? network->Read (chunk, sizeof (chunk), 0, got) : E_PENDING;

                if (SUCCEEDED (read) && got > 0)
                {
                    serial->Write (chunk, got);
                }
            }
        }
    };

    struct ModemFactory : Factory
    {
        HRESULT  ListenTcp (uint16_t, LinkTransport transport, std::unique_ptr<IByteStream> & outStream) override
        {
            Assert::IsTrue (transport == LinkTransport::WifiModem);
            outStream = std::make_unique<Handle> (pcEnd);
            return S_OK;
        }
    };




    TEST_METHOD (AWifiModemLinksDisk_DialsThePc_AndConnects)
    {
        TestMachine                           machine ("Apple2c");
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Modem                                 modem;
        ModemFactory                          factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        FixtureProvider                       fixtures;
        std::vector<uint8_t>                  bytes;
        std::vector<Byte>                     disk;
        std::vector<Byte>                     boot;
        std::vector<std::string>              screen;
        CassoLinkSettings                     settings;
        uint64_t                              spent = 0;



        AssertSucceeded (LinkProgram::BuildBootDisk (CassoLinkSettings::kDefaultBaud, boot, "ATDT casso.local:6400"));
        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        disk.assign (bytes.begin(), bytes.end());

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

        GuestSession::Mount (machine, boot);
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        InProcessByteStream::CreatePair (machineEnd, modem.serial);
        InProcessByteStream::CreatePair (modem.network, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));
        modem.answer = false;
        modem.thread = std::jthread ([&modem] (std::stop_token stop) { modem.Run (stop); });

        //  The shim draws the target in capitals while the modem dials.
        machine.RunCycles (20'000'000ULL);

        screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
        Assert::AreEqual (std::string ("DIALING CASSO.LOCAL:6400"), screen[2].substr (0, screen[2].find_last_not_of (' ') + 1));
        modem.answer = true;

        settings.name      = "Modem";
        settings.transport = LinkTransport::WifiModem;
        settings.address   = "0.0.0.0:6400";

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            auto  started = std::chrono::steady_clock::now();

            settings.use40 = true;

            AssertSucceeded (session.Start (settings));

            auto  list = session.Enumerate (2, FilePath());

            //  At the //c's own speed: the Apple's timeouts count cycles and the
            //  modem's hops take wall-clock time, as on a real line.
            while (!sink.IsDone (list) && spent < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;

                while (std::chrono::steady_clock::now() - started < std::chrono::microseconds (spent * 1'000'000 / kClockHz))
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (1));
                }
            }

            Assert::IsTrue (sink.IsDone (list), std::format (L"state {}, dialed {}, PC {:04X}", (int) session.GetState(), TextEncoding::NarrowToWide (modem.dialed), machine.GetCpu()->GetPC()).c_str());
            AssertSucceeded (list->result);
            Assert::IsFalse (list->listing.entries.empty(), L"drive 2's catalog through the modem");

            screen = TextScreenScraper::Scrape40 (machine.GetMemoryBus(), 0x0400);
            Assert::IsTrue (screen[1].starts_with ("WI-FI, PORT 2, 115200"), TextEncoding::NarrowToWide (screen[1]).c_str());
            Assert::IsTrue (screen[2].starts_with ("DIALED:     CASSO.LOCAL:6400"), TextEncoding::NarrowToWide (screen[2]).c_str());
        }

        modem.thread.request_stop();
        modem.thread.join();

        Assert::AreEqual (std::string ("ATDT casso.local:6400"), modem.dialed);
    }
};

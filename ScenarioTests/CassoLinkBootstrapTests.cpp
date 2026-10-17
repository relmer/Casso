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
//  status screen comes up and lists a disk (FR-092, US15/AC1).
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



    //  The disk Save link to disk makes boots into the shim at the link's
    //  speed; the shim has no program, so the session loads it (FR-091,
    //  US15/AC3 and AC4).
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
        Assert::IsTrue (screen[2].starts_with ("WAITING FOR THE PC ON PORT 2 AT 115200"), TextEncoding::NarrowToWide (screen[0] + " | " + screen[1] + " | " + screen[2]).c_str());

        settings.name      = "//c";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

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



    //  A Wi-Fi modem between the //c and the PC: it reads the dial command,
    //  replies CONNECT as a modem does, and then passes bytes both ways.
    struct Modem
    {
        std::shared_ptr<InProcessByteStream>  serial;
        std::shared_ptr<InProcessByteStream>  network;
        std::mutex                            lock;
        std::string                           dialed;
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

                        size_t  end = command.find ('\r');

                        if (end != std::string::npos)
                        {
                            {
                                std::lock_guard<std::mutex>  guard (lock);

                                dialed = command.substr (0, end);
                            }

                            serial->Write (reinterpret_cast<const Byte *> ("CONNECT\r\n"), 9);
                            online = true;
                        }
                    }
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
        CassoLinkSettings                     settings;
        uint64_t                              spent = 0;



        AssertSucceeded (LinkProgram::BuildBootDisk (CassoLinkSettings::kDefaultBaud, boot, "ATDT CASSO.LOCAL:6400"));
        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        disk.assign (bytes.begin(), bytes.end());

        machine.SetVideoTiming (std::make_unique<VideoTiming>());
        machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());

        GuestSession::Mount (machine, boot);
        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, disk));

        InProcessByteStream::CreatePair (machineEnd, modem.serial);
        InProcessByteStream::CreatePair (modem.network, factory.pcEnd);
        Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));
        modem.thread = std::jthread ([&modem] (std::stop_token stop) { modem.Run (stop); });

        settings.name      = "Modem";
        settings.transport = LinkTransport::WifiModem;
        settings.address   = "0.0.0.0:6400";

        {
            CassoLinkSession  session (factory, folder, fileIo, sink);

            auto  started = std::chrono::steady_clock::now();

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
        }

        modem.thread.request_stop();
        modem.thread.join();

        Assert::AreEqual (std::string ("ATDT CASSO.LOCAL:6400"), modem.dialed);
    }
};
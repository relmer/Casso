#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Seams/Win32ByteStreamFactory.h"
#include "Seams/Win32HostNetwork.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkUthernetScenarioTests
//
//  The link over an Uthernet II: an enhanced //e with the card in slot 3 on
//  the PC's own network stack boots the W5100 link disk, whose shim listens
//  on 127.0.0.1. A CassoLinkSession with the Uthernet transport connects to
//  it over a real loopback socket, loads the W5100 program, and lists, reads
//  and writes the DOS 3.3 disk in drive 2.
//
//  The port is not 6502, so the case cannot collide with another session's link on
//  this PC. The guest runs at 1.023 MHz of wall time, as its timeouts count
//  cycles and the socket's traffic takes real time.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkUthernetScenarioTests)
{
public:

    static constexpr uint64_t  kCyclesPerMs = 1023;
    static constexpr uint64_t  kBootCycles  = 20'000'000;
    static constexpr uint16_t  kPort        = 26502;
    static constexpr int       kSlot        = 3;
    static constexpr DWORD     kWaitMs      = 120'000;
    static constexpr DWORD     kRetryMs     = 500;
    static constexpr Word      kRow2        = 0x0500;
    static constexpr Word      kRow3        = 0x0580;
    static constexpr int       kField       = 12;

    struct Sink : ICassoLinkSessionSink
    {
        std::mutex                                    lock;
        std::vector<std::shared_ptr<LinkOperation>>   done;

        std::atomic<bool>                             loading { false };
        std::atomic<bool>                             dropped { false };

        //  A line that drops while the program loads, as when the shim resets
        //  the chip.
        void  OnStateChanged (LinkState state) override
        {
            dropped = dropped || (loading && state == LinkState::Disconnected);
            loading = (loading || state == LinkState::LoadingProgram) && state != LinkState::Ready;
        }

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

    struct Rig
    {
        Win32HostNetwork         host;
        TestMachine              machine;
        Win32ByteStreamFactory   factory;
        InMemoryCassoLinkFolder  folder;
        FakeDiskFileIo           fileIo;
        Sink                     sink;

        Rig()
            : machine ("Apple2eEnhanced", TestMachine::Slots::AsShipped,
                       [this] (MachineConfig & config, MachineBuildServices & services)
                       {
                           for (SlotConfig & slot : config.slots)
                           {
                               if (slot.device == "uthernet2")
                               {
                                   slot.enabled = true;
                               }
                           }

                           services.hostNetwork = &host;
                       })
        {
            Assert::IsNotNull (machine.GetRefs().uthernet, L"the card must be installed");
        }

        //  Runs at 1.023 MHz of wall time until `done` or the time runs out.
        void  RunUntil (std::function<bool()> done, DWORD ms)
        {
            ULONGLONG  start = GetTickCount64();
            uint64_t   ran   = 0;
            uint64_t   due   = 0;

            while (!done() && GetTickCount64() - start < ms)
            {
                due = (GetTickCount64() - start) * kCyclesPerMs;

                if (ran < due)
                {
                    ran += machine.RunCycles (due - ran);
                }
                else
                {
                    Sleep (1);
                }
            }
        }

        std::shared_ptr<LinkOperation>  Finish (std::shared_ptr<LinkOperation> operation)
        {
            RunUntil ([&] () { return sink.IsDone (operation); }, kWaitMs);

            Assert::IsTrue (sink.IsDone (operation), L"the operation finishes");
            return operation;
        }

        //  A row of the text page, from a column, with its trailing spaces cut.
        std::string  Row (Word address, int column = 0)
        {
            std::string  text;

            for (int i = column; i < 40; i++)
            {
                text += (char) (machine.GetCpu()->ReadByte ((Word) (address + i)) & 0x7F);
            }

            return text.substr (0, text.find_last_not_of (' ') + 1);
        }

        //  Drive 2's disk, as DOS-logical sectors.
        std::vector<Byte>  Sectors()
        {
            std::vector<Byte>   file;
            std::vector<Byte>   sectors;
            SectorDecodeReport  report;

            AssertSucceeded (machine.GetDiskStore().GetImage (6, 1)->Serialize (file));
            AssertSucceeded (VolumeImage::Load (file, "d.dsk", sectors, report));
            return sectors;
        }
    };

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




    TEST_METHOD (TheLinkDisk_Listens_AndTheSessionListsReadsAndWrites)
    {
        Rig                    rig;
        FixtureProvider        fixtures;
        std::vector<uint8_t>   bytes;
        std::vector<Byte>      dos;
        std::vector<Byte>      boot;
        std::vector<Byte>      written;
        LinkShimOptions        options;
        CassoLinkSettings      settings;
        VolumeListing          catalog;
        FilePayload            payload = Binary (1500, 7);

        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        dos.assign (bytes.begin(), bytes.end());
        AssertSucceeded (Dos33Volume (dos).Enumerate (catalog));
        Assert::IsFalse (catalog.entries.empty());

        options.chip            = LinkChip::W5100;
        options.slot            = kSlot;
        options.network.address = { 127, 0, 0, 1 };
        options.network.port    = kPort;

        AssertSucceeded (LinkProgram::BuildBootDisk (options, boot));

        GuestSession::Mount (rig.machine, boot);
        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, dos));
        rig.machine.RunCycles (kBootCycles);

        Assert::AreEqual (std::string ("WAITING FOR PC AT 127.0.0.1:26502"), rig.Row (kRow2), L"the shim listens");

        settings.name      = "Uthernet";
        settings.transport = LinkTransport::Uthernet;
        settings.address   = std::format ("127.0.0.1:{}", kPort);

        {
            CassoLinkSession  session (rig.factory, rig.folder, rig.fileIo, rig.sink);

            session.SetRetryInterval (kRetryMs);
            AssertSucceeded (session.Start (settings));

            auto  list = rig.Finish (session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::AreEqual (catalog.entries.size(), list->listing.entries.size(), L"drive 2's catalog over the network");
            Assert::AreEqual (catalog.entries[0].name, list->listing.entries[0].name);
            Assert::AreEqual (LinkProgram::kProgramVersion, (Word) (rig.machine.GetCpu()->ReadByte (0x0803) | (rig.machine.GetCpu()->ReadByte (0x0804) << 8)), L"the program was loaded");
            Assert::AreEqual (std::string ("SLOT 3, PORT 26502"), rig.Row (kRow2, kField));
            Assert::AreEqual (std::string ("CONNECTED"),          rig.Row (kRow3, kField));

            AssertSucceeded (Dos33Volume (dos).Write (FilePath::Parse ("FROMPC"), payload, written));

            auto  write = rig.Finish (session.WriteFile (2, FilePath::Parse ("FROMPC"), payload));

            AssertSucceeded (write->result);
            Assert::IsTrue (rig.Sectors() == written, L"drive 2 has the written file");

            auto  read = rig.Finish (session.ReadFile (2, FilePath::Parse ("FROMPC")));

            AssertSucceeded (read->result);
            Assert::IsTrue (read->file.bytes == payload.bytes, L"the file read back");
        }

        //  The PC left; the program listens again, and a new session connects
        //  to the program as it is, with no load.
        {
            CassoLinkSession  session (rig.factory, rig.folder, rig.fileIo, rig.sink);

            session.SetRetryInterval (kRetryMs);
            AssertSucceeded (session.Start (settings));

            auto  list = rig.Finish (session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::AreEqual (catalog.entries.size() + 1, list->listing.entries.size(), L"the catalog, with the file written");
        }

        //  A program of another version: Load program returns it to the shim
        //  over the same connection, and the shim loads the current program
        //  on it.
        {
            CassoLinkSession  session (rig.factory, rig.folder, rig.fileIo, rig.sink);
            Word              ready   = FindReady (rig.machine);
            Byte              version = rig.machine.GetCpu()->ReadByte ((Word) (ready + 5));

            rig.machine.GetCpu()->WriteByte ((Word) (ready + 5),  (Byte) (version - 1));
            rig.machine.GetCpu()->WriteByte ((Word) (ready + 14), (Byte) (rig.machine.GetCpu()->ReadByte ((Word) (ready + 14)) ^ version ^ (Byte) (version - 1)));

            session.SetRetryInterval (kRetryMs);
            AssertSucceeded (session.Start (settings));

            auto  list = rig.Finish (session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::AreEqual<int> (version, rig.machine.GetCpu()->ReadByte ((Word) (ready + 5)), L"the current program, loaded again");
            Assert::IsFalse (rig.sink.dropped, L"the connection stays open while the program loads");
        }
    }

    //  The program's Ready packet in memory: its envelope, with nine bytes.
    static Word  FindReady (TestMachine & machine)
    {
        const Byte  header[] = { 0xC1, 0x09, 0x00, CassoLinkCodec::kReady };
        Word        found    = 0;

        for (Word at = LinkProgram::kProgramAddress; at < LinkProgram::kW5100ZeroPageSave && found == 0; at++)
        {
            bool  match = true;

            for (size_t i = 0; i < sizeof (header); i++)
            {
                match = match && machine.GetCpu()->ReadByte ((Word) (at + i)) == header[i];
            }

            found = match ? at : 0;
        }

        Assert::AreNotEqual ((Word) 0, found, L"the program's Ready packet");
        return found;
    }
};

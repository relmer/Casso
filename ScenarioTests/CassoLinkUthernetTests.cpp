#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Devices/Network/VirtualSubnet.h"
#include "Seams/Win32ByteStreamFactory.h"
#include "Seams/Win32HostNetwork.h"
#include "Seams/Win32LinkAnnouncements.h"

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
    static constexpr Word      kRow0        = 0x0400;
    static constexpr Word      kRow1        = 0x0480;
    static constexpr Word      kRow4        = 0x0600;
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
            settings.use40 = true;
            AssertSucceeded (session.Start (settings));

            auto  list = rig.Finish (session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::AreEqual (catalog.entries.size(), list->listing.entries.size(), L"drive 2's catalog over the network");
            Assert::AreEqual (catalog.entries[0].name, list->listing.entries[0].name);
            Assert::AreEqual (LinkProgram::kProgramVersion, (Word) (rig.machine.GetCpu()->ReadByte (0x0803) | (rig.machine.GetCpu()->ReadByte (0x0804) << 8)), L"the program was loaded");
            Assert::AreEqual (std::string ("CASSOLINK: CONNECTED"),  rig.Row (kRow0));
            Assert::AreEqual (std::string ("UTHERNET II, SLOT 3, 26502"), rig.Row (kRow1));
            Assert::AreEqual (std::format ("ADDRESS:    127.0.0.1:{}", kPort), rig.Row (kRow2));

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
            settings.use40 = true;
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
            rig.machine.GetCpu()->WriteByte ((Word) (ready + 16), (Byte) (rig.machine.GetCpu()->ReadByte ((Word) (ready + 16)) ^ version ^ (Byte) (version - 1)));

            session.SetRetryInterval (kRetryMs);
            settings.use40 = true;
            AssertSucceeded (session.Start (settings));

            auto  list = rig.Finish (session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::AreEqual<int> (version, rig.machine.GetCpu()->ReadByte ((Word) (ready + 5)), L"the current program, loaded again");
            Assert::IsFalse (rig.sink.dropped, L"the connection stays open while the program loads");
        }
    }



    //  A Casso link at the card's address, the wrong kind of link for it:
    //  the shim's Ready indicates the W5100, so the program sent is the W5100
    //  build, and not the 6551 build for the link's transport.
    TEST_METHOD (ACassoLinkAtTheCardsAddress_LoadsTheW5100Program)
    {
        constexpr uint16_t     kOtherPort = kPort + 1;
        Rig                    rig;
        FixtureProvider        fixtures;
        std::vector<uint8_t>   bytes;
        std::vector<Byte>      dos;
        std::vector<Byte>      boot;
        LinkShimOptions        options;
        CassoLinkSettings      settings;
        VolumeListing          catalog;

        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        dos.assign (bytes.begin(), bytes.end());
        AssertSucceeded (Dos33Volume (dos).Enumerate (catalog));

        options.chip            = LinkChip::W5100;
        options.slot            = kSlot;
        options.network.address = { 127, 0, 0, 1 };
        options.network.port    = kOtherPort;

        AssertSucceeded (LinkProgram::BuildBootDisk (options, boot));

        GuestSession::Mount (rig.machine, boot);
        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, dos));
        rig.machine.RunCycles (kBootCycles);

        settings.name      = "Casso";
        settings.transport = LinkTransport::Emulated;
        settings.address   = std::format ("127.0.0.1:{}", kOtherPort);

        {
            CassoLinkSession  session (rig.factory, rig.folder, rig.fileIo, rig.sink);

            session.SetRetryInterval (kRetryMs);
            settings.use40 = true;
            AssertSucceeded (session.Start (settings));

            auto  list = rig.Finish (session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::AreEqual (catalog.entries.size(), list->listing.entries.size(), L"drive 2's catalog through a Casso link");
            Assert::IsTrue (rig.Row (kRow1).starts_with (std::format ("UTHERNET II, SLOT 3, {}", kOtherPort)), L"the W5100 program's connection row");
        }
    }

    //  The universal disk finds the card in slot 3, takes the address the
    //  virtual DHCP server offers over the address it was built with,
    //  broadcasts an announcement, and links.
    TEST_METHOD (TheUniversalDisk_GetsAnAddressByDhcp_Announces_AndLinks)
    {
        constexpr uint16_t     kUniversalPort = kPort + 2;
        Rig                    rig;
        FixtureProvider        fixtures;
        std::vector<uint8_t>   bytes;
        std::vector<Byte>      dos;
        std::vector<Byte>      boot;
        LinkShimOptions        options;
        CassoLinkSettings      settings;
        VolumeListing          catalog;
        Win32LinkAnnouncements listener (LinkProgram::kAnnouncePort);
        std::string            row;
        std::array<Byte, 6>    cardMac = {};
        LinkAnnouncement       ours;
        std::string            dhcp    = std::format ("UTHERNET II, SLOT 3, {}.{}.{}.{}:{}",
                                                     VirtualSubnet::kGuestIp >> 24, (VirtualSubnet::kGuestIp >> 16) & 0xFF,
                                                     (VirtualSubnet::kGuestIp >> 8) & 0xFF, VirtualSubnet::kGuestIp & 0xFF, kUniversalPort);

        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        dos.assign (bytes.begin(), bytes.end());
        AssertSucceeded (Dos33Volume (dos).Enumerate (catalog));

        options.network.address = { 192, 168, 77, 77 };
        options.network.mac     = { 0x00, 0x08, 0xDC, 0x11, 0x22, 0x33 };
        options.network.port    = kUniversalPort;

        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, boot));

        GuestSession::Mount (rig.machine, boot);
        AssertSucceeded (rig.machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, dos));

        AssertSucceeded (listener.SetListening (true));

        rig.RunUntil ([&] () { row = rig.Row (kRow4); return row.find (" AT ") != std::string::npos; }, kWaitMs);
        Assert::AreEqual (dhcp, row, L"the address from DHCP, not the one built in");

        //  Announcements from other Apples on the LAN arrive on the same
        //  port, so the one to check is matched by this card's MAC address,
        //  read from the program's memory.
        for (size_t at = 0; at < cardMac.size(); at++)
        {
            cardMac[at] = rig.machine.GetCpu()->ReadByte ((Word) (LinkProgram::kUniversalW5100 + LinkProgram::kNetMacOffset + at));
        }

        Assert::IsTrue (std::equal (options.network.mac.begin(), options.network.mac.begin() + 3, cardMac.begin()),
                        L"the MAC address's first three bytes as built, the rest salted on the Apple");

        auto  findOurs = [&] ()
        {
            for (const LinkAnnouncement & announcement : listener.GetAnnounced())
            {
                if (announcement.mac == cardMac)
                {
                    ours = announcement;
                    return true;
                }
            }

            return false;
        };

        rig.RunUntil (findOurs, kWaitMs);
        Assert::IsTrue (findOurs(), L"an announcement from this card's MAC address");
        Assert::AreEqual (std::format ("127.0.0.1:{}", kUniversalPort), ours.GetAddressText(),
                          L"from this PC, so at the address the card's socket is on");
        AssertSucceeded (listener.SetListening (false));

        settings.name      = "Uthernet";
        settings.transport = LinkTransport::Uthernet;
        settings.address   = std::format ("127.0.0.1:{}", kUniversalPort);

        {
            CassoLinkSession  session (rig.factory, rig.folder, rig.fileIo, rig.sink);

            session.SetRetryInterval (kRetryMs);
            settings.use40 = true;
            AssertSucceeded (session.Start (settings));

            auto  list = rig.Finish (session.Enumerate (2, FilePath()));

            AssertSucceeded (list->result);
            Assert::AreEqual (catalog.entries.size(), list->listing.entries.size(), L"drive 2's catalog over the card the disk found");
        }
    }

    //  The disk's MAC address is salted with the Apple's own memory, so two
    //  Apples booted from copies of one disk have two addresses.
    TEST_METHOD (TheUniversalDisksMac_DiffersBetweenTwoApples)
    {
        std::array<std::array<Byte, 6>, 2>  macs = {};
        LinkShimOptions                     options;
        std::vector<Byte>                   boot;

        options.network.address = { 192, 168, 77, 77 };
        options.network.mac     = { 0x00, 0x08, 0xDC, 0x11, 0x22, 0x33 };
        options.network.port    = kPort + 3;

        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, boot));

        for (size_t apple = 0; apple < macs.size(); apple++)
        {
            Rig          rig;
            std::string  row;

            GuestSession::Mount (rig.machine, boot);
            rig.machine.GetMemoryBus().WriteByte (0x5000, (Byte) (0x11 * (apple + 1)));

            rig.RunUntil ([&] () { row = rig.Row (kRow4); return row.find (" AT ") != std::string::npos; }, kWaitMs);

            for (size_t at = 0; at < macs[apple].size(); at++)
            {
                macs[apple][at] = rig.machine.GetCpu()->ReadByte ((Word) (LinkProgram::kUniversalW5100 + LinkProgram::kNetMacOffset + at));
            }

            Assert::IsTrue (macs[apple][0] == 0x00 && macs[apple][1] == 0x08 && macs[apple][2] == 0xDC, L"the first three bytes as built");
        }

        Assert::IsTrue (macs[0] != macs[1], L"two Apples whose memory differs have two addresses");
    }

    //  Two Apples, each with an Uthernet II, linked to one PC at once. Each
    //  session lists, writes and reads the disk of its Apple while the other
    //  session does the same, and then both Apples run a Full Send to one
    //  folder. Its manifest has a row for each of the four disks, each row with
    //  a different number.
    TEST_METHOD (TwoApples_LinkAtOnce_ForFiles_AndForFullSendsToOneFolder)
    {
        constexpr std::array<uint16_t, 2>  kPorts      = { kPort + 5, kPort + 6 };
        constexpr Word                     kRowCounts  = 0x0680;
        constexpr Word                     kRowPrompt  = 0x07A8;
        constexpr DWORD                    kFullSendMs = 600'000;

        Rig                                first;
        Rig                                second;
        std::array<Rig *, 2>               rigs        = { &first, &second };
        FakeDiskFileIo                     shared;
        FixtureProvider                    fixtures;
        std::vector<uint8_t>               bytes;
        std::vector<Byte>                  dos;
        std::array<std::vector<Byte>, 4>   disks;
        std::set<std::string>              images;
        std::set<int>                      sequences;
        FullSendManifest                   manifest;

        std::array<std::unique_ptr<CassoLinkSession>, 2>  sessions;
        std::array<std::shared_ptr<LinkOperation>, 2>     sends;
        std::array<FilePayload, 2>                        payloads = { Binary (1500, 7), Binary (900, 9) };



        AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
        dos.assign (bytes.begin(), bytes.end());

        //  Both machines run at 1.023 MHz of wall time, a step of each in turn.
        auto  runBoth = [&] (std::function<bool()> done, DWORD ms)
        {
            ULONGLONG                  start = GetTickCount64();
            std::array<uint64_t, 2>    ran   = { 0, 0 };
            uint64_t                   due   = 0;

            while (!done() && GetTickCount64() - start < ms)
            {
                due = (GetTickCount64() - start) * kCyclesPerMs;

                for (size_t apple = 0; apple < rigs.size(); apple++)
                {
                    if (ran[apple] < due)
                    {
                        ran[apple] += rigs[apple]->machine.RunCycles (due - ran[apple]);
                    }
                }

                Sleep (1);
            }

            Assert::IsTrue (done());
        };

        auto  isPrompting = [] (Rig & rig)
        {
            constexpr std::array<Word, 8>  kRows = { 0x07A8, 0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750 };

            return std::any_of (kRows.begin(), kRows.end(), [&] (Word row) { return rig.Row (row).starts_with ("PUT IN A NEW DISK"); });
        };

        for (size_t apple = 0; apple < rigs.size(); apple++)
        {
            LinkShimOptions    options;
            CassoLinkSettings  settings;
            std::vector<Byte>  boot;

            options.chip            = LinkChip::W5100;
            options.slot            = kSlot;
            options.network.address = { 127, 0, 0, 1 };
            options.network.port    = kPorts[apple];

            AssertSucceeded (LinkProgram::BuildBootDisk (options, boot));

            GuestSession::Mount (rigs[apple]->machine, boot);
            AssertSucceeded (rigs[apple]->machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Dsk, dos));
            rigs[apple]->machine.RunCycles (kBootCycles);

            settings.name      = std::format ("Apple {}", apple + 1);
            settings.transport = LinkTransport::Uthernet;
            settings.address   = std::format ("127.0.0.1:{}", kPorts[apple]);
            settings.use40     = true;

            sessions[apple] = std::make_unique<CassoLinkSession> (rigs[apple]->factory, rigs[apple]->folder, shared, rigs[apple]->sink);
            sessions[apple]->SetRetryInterval (kRetryMs);
            AssertSucceeded (sessions[apple]->Start (settings));
        }

        //  Files, on both links at once.
        {
            std::array<std::shared_ptr<LinkOperation>, 2>  lists;
            std::array<std::shared_ptr<LinkOperation>, 2>  writes;
            std::array<std::shared_ptr<LinkOperation>, 2>  reads;

            for (size_t apple = 0; apple < rigs.size(); apple++)
            {
                lists[apple]  = sessions[apple]->Enumerate (2, FilePath());
                writes[apple] = sessions[apple]->WriteFile (2, FilePath::Parse (apple == 0 ? "FROMA" : "FROMB"), payloads[apple]);
                reads[apple]  = sessions[apple]->ReadFile (2, FilePath::Parse (apple == 0 ? "FROMA" : "FROMB"));
            }

            runBoth ([&] () { return first.sink.IsDone (reads[0]) && second.sink.IsDone (reads[1]); }, kWaitMs);

            for (size_t apple = 0; apple < rigs.size(); apple++)
            {
                AssertSucceeded (lists[apple]->result);
                AssertSucceeded (writes[apple]->result);
                AssertSucceeded (reads[apple]->result);
                Assert::IsTrue (reads[apple]->file.bytes == payloads[apple].bytes, L"each Apple's file, read back from that Apple's disk");
            }
        }

        //  Full Sends to one folder, at once. Drive 1's link disk is replaced
        //  with another disk, and drive 2 has the file just written.
        for (size_t apple = 0; apple < rigs.size(); apple++)
        {
            FilePayload  payload = Binary (300, (Byte) (apple + 1));

            AssertSucceeded (Dos33Volume (dos).Write (FilePath::Parse (apple == 0 ? "ALPHA" : "BRAVO"), payload, disks[apple * 2]));
            rigs[apple]->machine.GetDiskStore().Eject (6, 0);
            AssertSucceeded (rigs[apple]->machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Dsk, disks[apple * 2]));
            disks[apple * 2 + 1] = rigs[apple]->Sectors();

            sends[apple] = sessions[apple]->FullSend ("C:\\Box", "", "");
        }

        runBoth ([&] () { return first.Row (kRowPrompt).starts_with ("V: VERIFY READS") && second.Row (kRowPrompt).starts_with ("V: VERIFY READS"); }, kWaitMs);

        for (Rig * rig : rigs)
        {
            rig->machine.GetRefs().iieKeyboard->PressKey (' ');
        }

        runBoth ([&] () { return first.Row (kRowCounts).starts_with ("SENT 2") && second.Row (kRowCounts).starts_with ("SENT 2") && isPrompting (first) && isPrompting (second); }, kFullSendMs);

        for (Rig * rig : rigs)
        {
            rig->machine.GetRefs().iieKeyboard->PressKey ('Q');
        }

        runBoth ([&] () { return first.sink.IsDone (sends[0]) && second.sink.IsDone (sends[1]); }, kWaitMs);

        AssertSucceeded (manifest.Parse (shared.files["C:\\Box\\manifest.fullsend"]));
        Assert::AreEqual ((size_t) 4, manifest.GetRows().size(), L"each Apple's two disks, with no row overwritten by the other session");

        for (const FullSendRow & row : manifest.GetRows())
        {
            std::vector<Byte>   sectors;
            SectorDecodeReport  report;

            sequences.insert (row.sequence);
            AssertSucceeded (VolumeImage::Load (shared.files["C:\\Box\\" + row.file], row.file, sectors, report));
            images.insert (std::string (sectors.begin(), sectors.end()));
        }

        Assert::IsTrue (sequences == std::set<int> ({ 1, 2, 3, 4 }), L"a number for each disk");

        for (const std::vector<Byte> & disk : disks)
        {
            Assert::IsTrue (images.contains (std::string (disk.begin(), disk.end())), L"every disk's image, as it was in its drive");
        }
    }

    //  The program's Ready packet in memory: its envelope, with eleven bytes.
    static Word  FindReady (TestMachine & machine)
    {
        const Byte  header[] = { 0xC1, 0x0B, 0x00, CassoLinkCodec::kReady };
        Word        found    = 0;

        for (Word at = LinkProgram::kProgramAddress; at < LinkProgram::kZeroPageSave && found == 0; at++)
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

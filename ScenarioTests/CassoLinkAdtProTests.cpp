#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"
#include "CassoLink/CassoLinkCaptures.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/CassoLinkSession.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "Seams/IByteStreamFactory.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAdtProTests
//
//  ADTPro's own client, unmodified, against Casso's host. The client disk
//  boots in an emulated //c whose port 2 is joined to CassoLinkHost over the
//  serial bridge; the client is driven through its own menus, as a user
//  would; and the disks it sends and receives must be byte for byte the ones
//  ADTPro's own server made in the same exchanges (the captures in
//  UnitTest/Fixtures/CassoLink/).
//
//  The machine runs as fast as it can. The client's timeouts are counted in
//  its own cycles and the host replies within a millisecond, far inside them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkAdtProTests)
{
public:

    //  The line for a session: each connection is a new handle, and the
    //  bytes from the Apple while no handle is open are discarded, as on a
    //  COM port that is closed.
    struct ReopeningFactory : IByteStreamFactory
    {
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::atomic<int>                      connects { 0 };

        struct Handle : IByteStream
        {
            std::shared_ptr<InProcessByteStream>  end;
            std::atomic<bool>                     closed { false };

            explicit Handle (std::shared_ptr<InProcessByteStream> e) : end (std::move (e)) {}

            HRESULT  Write            (const Byte * data, size_t count) override                                   { return closed ? E_HANDLE : end->Write (data, count); }
            HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { outRead = 0; return closed ? E_HANDLE : end->Read (buffer, capacity, timeoutMs, outRead); }
            size_t   GetReadableCount() override                                                                  { return closed ? 0 : end->GetReadableCount(); }
            bool     IsClosed() const override                                                                    { return closed; }
            void     Close() override                                                                             { closed = true; }
        };

        HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override                                      { outPorts.clear(); return S_OK; }
        HRESULT  OpenComPort  (const std::wstring &, const ComSettings &, std::unique_ptr<IByteStream> &) override { return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND); }
        HRESULT  ListenTcp    (uint16_t, LinkTransport, std::unique_ptr<IByteStream> &) override                  { return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED); }

        HRESULT  ConnectTcp (const std::string &, uint16_t, std::unique_ptr<IByteStream> & outStream) override
        {
            Byte     discard[256];
            size_t   got = 0;
            HRESULT  hr  = S_OK;

            while (pcEnd->GetReadableCount() > 0 && SUCCEEDED (hr))
            {
                hr = pcEnd->Read (discard, sizeof (discard), 0, got);
            }

            connects++;
            outStream = std::make_unique<Handle> (pcEnd);
            return S_OK;
        }
    };

    //  Counts the connection attempts that found no Apple.
    struct QuietSink : ICassoLinkSessionSink
    {
        std::atomic<int>  misses { 0 };

        void  OnStateChanged (LinkState state) override { misses += (state == LinkState::Disconnected) ? 1 : 0; }
        void  OnProgress     (const std::shared_ptr<LinkOperation> &) override {}
        void  OnDone         (const std::shared_ptr<LinkOperation> &) override {}
    };

    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        InMemoryCassoLinkFolder               folder;
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  hostEnd;
        std::unique_ptr<CassoLinkHost>        host;
        std::jthread                          serving;

        //  Without the host, the line is left for a test's own session.
        explicit Rig (bool serve = true)
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  client;

            AssertSucceeded (fixtures.OpenFixture ("CassoLink/ADTPRO-2.1.0.DSK", client));

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            machine.PowerCycle();

            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Dsk, std::vector<Byte> (client.begin(), client.end())));

            InProcessByteStream::CreatePair (machineEnd, hostEnd);
            Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

            if (!serve)
            {
                return;
            }

            host    = std::make_unique<CassoLinkHost> (hostEnd, folder, [] () { return std::array<Byte, 4> {}; });
            serving = std::jthread ([this] (std::stop_token stop)
            {
                HRESULT  hr = S_OK;

                while (!stop.stop_requested() && SUCCEEDED (hr))
                {
                    hr = host->ServeOne (20);
                }
            });
        }

        ~Rig()
        {
            if (serving.joinable())
            {
                serving.request_stop();
                serving.join();
            }
        }

        std::string  Screen()
        {
            std::string  text;

            for (const std::string & row : TextScreenScraper::Scrape (machine)) { text += row + "\n"; }
            return text;
        }

        bool  RunUntil (const std::string & text, uint64_t budget)
        {
            uint64_t  spent = 0;

            while (Screen().find (text) == std::string::npos && spent < budget)
            {
                machine.RunCycles (17030);
                spent += 17030;
            }

            return spent < budget;
        }

        void  Type (const std::string & keys)
        {
            for (char key : keys)
            {
                machine.GetRefs().keyboard->PressKey ((Byte) key);
                machine.RunCycles (150000);
            }
        }

        //  A screen row, without the spaces after its text.
        std::string  Row (int row)
        {
            std::string  text = TextScreenScraper::Scrape (machine)[(size_t) row];

            text.erase (text.find_last_not_of (' ') + 1);
            return text;
        }

        //  The chooser's highlighted row. The client inverts it by clearing
        //  bit 6 as well as bit 7, which the scraper reads 0x40 higher for
        //  anything but a letter.
        std::string  Highlighted (int row)
        {
            std::string  text = Row (row);

            for (char & c : text)
            {
                c = (c >= '`') ? (char) (c - 0x40) : c;
            }

            text.erase (text.find_last_not_of (' ') + 1);
            return text;
        }

        //  Files the listing pages through, one block each.
        void  AddFiles (const std::string & prefix, int count)
        {
            for (int i = 0; i < count; i++)
            {
                folder.files[std::format ("{}{:02}.DSK", prefix, i)] = std::vector<Byte> (512, 0);
            }
        }

        //  The client's configuration screen, with Enable nibbles set to Yes:
        //  the fifth parameter, changed with the arrow keys.
        void  EnableNibbles()
        {
            Start ('G');
            Assert::IsTrue (RunUntil ("ENABLE NIBBLES", 10'000'000), L"the configuration screen");
            Type ("\x0A\x0A\x0A\x0A");

            for (int press = 0; press < 2 && Row (7).find ("YES") == std::string::npos; press++)
            {
                Type ("\x15");
            }

            Assert::IsTrue (Row (7).find ("YES") != std::string::npos, L"nibbles are enabled");
            Type ("\r");
            Assert::IsTrue (RunUntil ("ECEIVE", 10'000'000), L"and the menu is back");
        }

        //  Choose from the main menu once it is up. The last item chosen is
        //  highlighted, which the scraper misreads, so the wait is on a word
        //  that reads the same either way.
        void  Choose (char command)
        {
            Assert::IsTrue (RunUntil ("ECEIVE", 40'000'000), TextEncoding::NarrowToWide ("the main menu, not:\n" + Screen()).c_str());
            Type (std::string (1, command));
        }

        //  Boot, choose the serial connection, and start the command.
        void  Start (char command)
        {
            Assert::IsTrue (RunUntil ("CONNECTION TYPE", 40'000'000), L"ADTPro's client boots");
            Type ("S");
            Assert::IsTrue (RunUntil ("(S)END", 10'000'000), L"and takes the serial connection");
            Type (std::string (1, command));
        }

        //  The volume list, then slot 6 drive 1.
        void  PickDriveOne()
        {
            Assert::IsTrue (RunUntil ("ADTPRO.2.1.0", 40'000'000), L"drive 1 is in the volume list");
            Type ("\x0A\x0D");
        }
    };


    TEST_METHOD (ASentDiskIsTheImageAdtProsServerWrote)
    {
        Rig                   rig;
        FixtureProvider       fixtures;
        std::vector<uint8_t>  source;

        AssertSucceeded (fixtures.OpenFixture ("CassoLink/ADTPRO-2.1.0.DSK", source));

        rig.Start ('S');
        rig.Type ("CASSOCAP.DSK\r");
        rig.PickDriveOne();

        Assert::IsTrue (rig.RunUntil ("COMPLETE", 400'000'000), L"the send completes");
        rig.machine.RunCycles (2'000'000);

        Assert::IsTrue (rig.folder.files["CASSOCAP.DSK"] == std::vector<Byte> (source.begin(), source.end()),
                        L"the image is the disk, as ADTPro's own server wrote it");
    }


    TEST_METHOD (AReceivedDiskIsTheImageTheServerHeld)
    {
        Rig                rig;
        std::vector<Byte>  written;

        CassoLinkHost::ToFileOrder ("VDRIVE-2.1.0.DSK", CassoLinkCaptures::RebuildImage ("adtpro-receive-disk.txt", '<', 280), rig.folder.files["VDRIVE-2.1.0.DSK"]);

        rig.Start ('R');
        rig.Type ("VDRIVE-2.1.0.DSK\r");
        rig.PickDriveOne();

        Assert::IsTrue (rig.RunUntil ("COMPLETE", 600'000'000), L"the receive completes");

        AssertSucceeded (rig.machine.GetDiskStore().GetImage (6, 0)->Serialize (written));
        Assert::IsTrue (written == rig.folder.files["VDRIVE-2.1.0.DSK"], L"drive 1 now has the image, sector for sector");
    }


    //  DIR as the client draws it: the header, a blank row and the dashes,
    //  a full page of files on rows 3 to 20, and Z for the next page, which
    //  the client requests from the host.
    TEST_METHOD (DirectoryPages_AreDrawnAndPagedByTheClient)
    {
        Rig  rig;

        rig.folder.path = "C:\\DISKS\\";
        rig.AddFiles ("A", 20);

        rig.Start ('D');
        rig.Type ("\r");
        Assert::IsTrue (rig.RunUntil ("SELECT WITH RETURN", 40'000'000), L"the listing arrives");

        Assert::AreEqual (std::string ("DIRECTORY OF: C:\\DISKS\\"), rig.Row (0));
        Assert::AreEqual (std::string(),                              rig.Row (1));
        Assert::AreEqual (std::string (40, '-'),                      rig.Row (2));
        Assert::AreEqual (std::string ("A00.DSK"),                    rig.Highlighted (3));
        Assert::AreEqual (std::string ("A17.DSK"),                    rig.Row (20));

        rig.Type ("Z");
        Assert::IsTrue (rig.RunUntil ("A19.DSK", 40'000'000), L"Z brings the second page");

        Assert::AreEqual (std::string ("A18.DSK"), rig.Highlighted (3));
        Assert::AreEqual (std::string ("A19.DSK"), rig.Row (4));
        Assert::AreEqual (std::string(),           rig.Row (5));
    }


    TEST_METHOD (AFilespec_NarrowsTheListing)
    {
        Rig  rig;

        rig.folder.files["A.DSK"] = std::vector<Byte> (512, 0);
        rig.folder.files["B.PO"]  = std::vector<Byte> (512, 0);
        rig.folder.files["C.PO"]  = std::vector<Byte> (512, 0);

        rig.Start ('D');
        rig.Type ("*.PO\r");
        Assert::IsTrue (rig.RunUntil ("SELECT WITH RETURN", 40'000'000), L"the listing arrives");

        Assert::AreEqual (std::string ("B.PO"), rig.Highlighted (3));
        Assert::AreEqual (std::string ("C.PO"), rig.Row (4));
        Assert::IsTrue (rig.Screen().find ("A.DSK") == std::string::npos);
    }


    //  Receive with no name opens the chooser; Return on a name sends its
    //  page and row, which the host resolves from the listing it sent.
    TEST_METHOD (TheReceiveChooser_FetchesTheHighlightedFile)
    {
        Rig                rig;
        std::vector<Byte>  written;

        rig.AddFiles ("A", 19);
        CassoLinkHost::ToFileOrder ("VDRIVE-2.1.0.DSK", CassoLinkCaptures::RebuildImage ("adtpro-receive-disk.txt", '<', 280), rig.folder.files["VDRIVE-2.1.0.DSK"]);

        rig.Start ('R');
        rig.Type ("\r");
        Assert::IsTrue (rig.RunUntil ("SELECT WITH RETURN", 40'000'000), L"the chooser");

        rig.Type ("Z");
        Assert::IsTrue (rig.RunUntil ("VDRIVE-2.1.0.DSK", 40'000'000), L"the second page");
        Assert::AreEqual (std::string ("VDRIVE-2.1.0.DSK"), rig.Row (4));

        rig.Type ("\x0A\r");
        rig.PickDriveOne();

        Assert::IsTrue (rig.RunUntil ("COMPLETE", 600'000'000), L"the receive completes");

        AssertSucceeded (rig.machine.GetDiskStore().GetImage (6, 0)->Serialize (written));
        Assert::IsTrue (written == rig.folder.files["VDRIVE-2.1.0.DSK"], L"the file on page 2, row 4, now in drive 1");
    }


    //  Each track of the .nib is one revolution of drive 1: 16 address
    //  fields, every one for that track.
    TEST_METHOD (ANibbleSend_WritesEachTrackOfTheDisk)
    {
        Rig  rig;

        rig.EnableNibbles();
        rig.Choose ('S');
        rig.Type ("GAME\r");
        rig.PickDriveOne();
        Assert::IsTrue (rig.RunUntil ("IBBLE?", 40'000'000), L"the client offers a nibble send");
        rig.Type ("N");

        Assert::IsTrue (rig.RunUntil ("COMPLETE", 2'000'000'000), L"the nibble send completes");
        rig.machine.RunCycles (2'000'000);

        const std::vector<Byte> &  nib = rig.folder.files["GAME.nib"];

        Assert::AreEqual (CassoLinkHost::kNibbleTracks * CassoLinkHost::kNibbleTrackSize, nib.size());

        for (size_t track = 0; track < CassoLinkHost::kNibbleTracks; track++)
        {
            const Byte *  bytes  = nib.data() + track * CassoLinkHost::kNibbleTrackSize;
            int           fields = 0;

            for (size_t i = 0; i + 8 < CassoLinkHost::kNibbleTrackSize; i++)
            {
                bool  field = bytes[i] == 0xD5 && bytes[i + 1] == 0xAA && bytes[i + 2] == 0x96;

                if (field)
                {
                    Assert::AreEqual<size_t> (track, (size_t) (((bytes[i + 5] << 1) | 1) & bytes[i + 6]), L"an address field for this track");
                    fields++;
                }
            }

            Assert::AreEqual (16, fields, std::format (L"one revolution of track {}", track).c_str());
        }
    }


    //  Each disk of a batch takes the prefix and the next free number.
    TEST_METHOD (ABatchSend_NumbersEachDiskFromThePrefix)
    {
        Rig                   rig;
        FixtureProvider       fixtures;
        std::vector<uint8_t>  source;

        AssertSucceeded (fixtures.OpenFixture ("CassoLink/ADTPRO-2.1.0.DSK", source));
        rig.folder.files["DISK0001.dsk"] = std::vector<Byte> (512, 0);

        rig.Start ('B');
        rig.Type ("DISK\r");
        rig.PickDriveOne();
        Assert::IsTrue (rig.RunUntil ("PRESS A KEY", 40'000'000), L"the client waits for the first disk");
        rig.Type (" ");

        Assert::IsTrue (rig.RunUntil ("COMPLETE", 400'000'000), L"the first disk is sent");
        Assert::IsTrue (rig.RunUntil ("PRESS A KEY", 40'000'000), L"and the client waits for the next");
        rig.Type ("\x1B");
        rig.machine.RunCycles (2'000'000);

        Assert::IsTrue (rig.folder.files["DISK0002.dsk"] == std::vector<Byte> (source.begin(), source.end()), L"DISK0001 is taken, so DISK0002");
    }


    //  A failed send ends in the client's Home, and the client's next request
    //  is served. The client also accepts a reply to Home, so this does not
    //  separate a reply from none; the unit test Home_HasNoReply does.
    TEST_METHOD (Home_AfterAFailedSend_LeavesTheNextRequestServed)
    {
        Rig  rig;

        rig.folder.files["A.DSK"] = std::vector<Byte> (512, 0);
        rig.folder.open = false;

        rig.Start ('S');
        rig.Type ("X.DSK\r");
        rig.PickDriveOne();
        Assert::IsTrue (rig.RunUntil ("PRESS A KEY", 40'000'000), L"the send fails with no folder");

        //  The client's PAUSE clears the keyboard strobe before it waits, so
        //  the key goes in once it is waiting.
        rig.folder.open = true;
        rig.machine.RunCycles (1'000'000);
        rig.Type (" ");

        rig.Choose ('D');
        rig.Type ("\r");
        Assert::IsTrue (rig.RunUntil ("SELECT WITH RETURN", 40'000'000), L"the next listing arrives");
        Assert::AreEqual (std::string ("A.DSK"), rig.Highlighted (3));
    }


    //  With no Casso program on the line, the session makes attempt after
    //  attempt while ADTPro's client boots; the line stays open across the
    //  attempts, and the client's request between them is served.
    TEST_METHOD (ASessionWithNoAppleAnswering_KeepsTheLineOpen_AndServesAdtPro)
    {
        Rig                 rig (false);
        ReopeningFactory    factory;
        FakeDiskFileIo      fileIo;
        QuietSink           sink;
        CassoLinkSettings   settings;

        factory.pcEnd = rig.hostEnd;
        rig.folder.files["A.DSK"] = std::vector<Byte> (512, 0);

        CassoLinkSession  session (factory, rig.folder, fileIo, sink);

        settings.name      = "//c";
        settings.transport = LinkTransport::Emulated;
        settings.address   = "127.0.0.1:6502";

        session.SetConnectTimeout (200);
        session.SetRetryInterval (200);
        AssertSucceeded (session.Start (settings));

        rig.Start ('D');

        auto  until = std::chrono::steady_clock::now() + std::chrono::seconds (10);

        while (sink.misses < 3 && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
        }

        Assert::IsTrue (sink.misses >= 3, L"more than one attempt");

        rig.Type ("\r");
        Assert::IsTrue (rig.RunUntil ("SELECT WITH RETURN", 400'000'000), L"the listing arrives");
        Assert::AreEqual (std::string ("A.DSK"), rig.Highlighted (3));
        Assert::AreEqual (1, factory.connects.load(), L"the line was opened once");
    }
};

#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkFullSendScenarioTests
//
//  A Full Send against Casso's program on an emulated //c, over the
//  in-process bridge, with disks swapped by the test as the Apple's screen
//  reports each transfer. Each image is compared with its source.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkFullSendScenarioTests)
{
public:

    using Image = std::vector<Byte>;

    static constexpr uint64_t  kFrame       = 17030;
    static constexpr uint64_t  kBudget      = 8'000'000'000;
    static constexpr uint64_t  kCyclesPerS  = 1'020'484;
    static constexpr Word      kRowTitle    = 0x0600;
    static constexpr Word      kRowCounts   = 0x0680;
    static constexpr Word      kRowDivider  = 0x0700;
    static constexpr Word      kRowDrive1   = 0x0780;
    static constexpr Word      kRowBar1     = 0x0428;
    static constexpr Word      kRowDrive2   = 0x0528;
    static constexpr Word      kRowPrompt   = 0x07A8;
    static constexpr Word      kRowPrompt2  = 0x0450;
    static constexpr int       kSlot        = 6;

    //  The target is 30; a read to image alone takes 27 here.
    static constexpr uint64_t  kSecondsADisk = 31;

    struct Factory : IByteStreamFactory
    {
        std::shared_ptr<InProcessByteStream>  pcEnd;

        struct Handle : IByteStream
        {
            std::shared_ptr<InProcessByteStream>  end;

            explicit Handle (std::shared_ptr<InProcessByteStream> e) : end (std::move (e)) {}

            HRESULT  Write            (const Byte * data, size_t count) override                                   { return end->Write (data, count); }
            HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) override { return end->Read (buffer, capacity, timeoutMs, outRead); }
            size_t   GetReadableCount() override                                                                   { return end->GetReadableCount(); }
            bool     IsClosed() const override                                                                     { return end->IsClosed(); }
            void     Close() override                                                                              { end->Close(); }
        };

        HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override                                      { outPorts.clear(); return S_OK; }
        HRESULT  OpenComPort  (const std::wstring &, const ComSettings &, std::unique_ptr<IByteStream> &) override { return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND); }
        HRESULT  ListenTcp    (uint16_t, LinkTransport, std::unique_ptr<IByteStream> &) override                   { return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED); }

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
        std::vector<std::shared_ptr<LinkOperation>>   queued;
        std::vector<std::string>                      activity;

        bool  TryGetFullSendTarget (std::string & outFolder, std::string & outExtension) override
        {
            outFolder = "C:\\Box";
            outExtension.clear();

            return true;
        }

        void  OnOperationQueued (const std::shared_ptr<LinkOperation> & operation) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            queued.push_back (operation);
        }

        void  OnStateChanged (LinkState) override {}
        void  OnProgress     (const std::shared_ptr<LinkOperation> &) override {}

        void  OnDone (const std::shared_ptr<LinkOperation> & operation) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            done.push_back (operation);
        }

        void  OnActivity (const std::string & text) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            activity.push_back (text);
        }

        bool  IsDone (const std::shared_ptr<LinkOperation> & operation)
        {
            std::lock_guard<std::mutex>  guard (lock);

            return std::find (done.begin(), done.end(), operation) != done.end();
        }
    };

    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        std::shared_ptr<InProcessByteStream>  machineEnd;
        Factory                               factory;
        InMemoryCassoLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::unique_ptr<CassoLinkSession>     session;
        Image                                 dosFile;
        Image                                 prodosFile;
        std::shared_ptr<LinkOperation>        send;
        uint64_t                              spent = 0;

        explicit Rig (DWORD shimBaud = 9600, bool use40 = true)
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;
            Image                 shim;
            Word                  at = LinkProgram::kShimAddress;
            CassoLinkSettings     settings;

            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            dosFile.assign (bytes.begin(), bytes.end());
            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/prodos.po", bytes));
            prodosFile.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            machine.PowerCycle();

            Mount (1, DiskFormat::Dsk, dosFile);
            Mount (2, DiskFormat::Po,  prodosFile);

            InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
            Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));
            Assert::AreEqual (S_OK, LinkProgram::BuildShim (shimBaud, shim));

            for (Byte b : shim)
            {
                machine.GetCpu()->WriteByte (at++, b);
            }

            machine.GetCpu()->SetPC (LinkProgram::kShimAddress);

            session = std::make_unique<CassoLinkSession> (factory, folder, fileIo, sink);

            settings.name      = "//c";
            settings.transport = LinkTransport::Emulated;
            settings.address   = "127.0.0.1:6502";

            settings.use40 = use40;

            AssertSucceeded (session->Start (settings));
        }

        ~Rig()
        {
            session.reset();
        }

        void  Mount (int drive, DiskFormat format, const Image & file)
        {
            machine.GetDiskStore().Eject (kSlot, drive - 1);
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (kSlot, drive - 1, "", format, file));
        }

        std::string  Row (Word address)
        {
            std::string  text;

            for (int i = 0; i < 40; i++)
            {
                text += (char) (machine.GetCpu()->ReadByte ((Word) (address + i)) & 0x7F);
            }

            return text.substr (0, text.find_last_not_of (' ') + 1);
        }

        //  The pause's prompt is on the history's rows, after the notes on a
        //  repeat.
        bool  IsPrompting()
        {
            constexpr std::array<Word, 8>  kRows = { 0x07A8, 0x0450, 0x04D0, 0x0550, 0x05D0, 0x0650, 0x06D0, 0x0750 };

            return std::any_of (kRows.begin(), kRows.end(), [&] (Word row) { return Row (row).starts_with ("PUT IN A NEW DISK"); });
        }

        //  Space on the entry screen, which begins the session.
        void  Begin()
        {
            RunUntil ([&] () { return Row (kRowPrompt).starts_with ("V: VERIFY READS"); }, L"the entry screen");
            Press (' ');
            RunUntil ([&] () { return Row (kRowPrompt).empty(); }, L"Space begins the session");
        }

        //  Steps the machine until the condition is true, with realTime at the //c's
        //  speed, so that each pause on the PC costs what it would on a real //c.
        template <typename Condition>
        void  RunUntil (Condition condition, const wchar_t * what, bool realTime = false)
        {
            auto  started = std::chrono::steady_clock::now();
            auto  from    = spent;

            while (!condition() && spent - from < kBudget)
            {
                machine.RunCycles (kFrame);
                spent += kFrame;

                while (realTime && std::chrono::steady_clock::now() - started < std::chrono::microseconds ((spent - from) * 1'000'000 / kCyclesPerS))
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (1));
                }
            }

            Assert::IsTrue (condition(), what);
        }

        void  Press (char key)
        {
            machine.GetRefs().iieKeyboard->PressKey ((Byte) key);
        }

        void  Finish()
        {
            RunUntil ([&] () { return sink.IsDone (send); }, L"the session ends");
        }

        std::vector<FullSendRow>  Rows()
        {
            FullSendManifest  manifest;

            AssertSucceeded (manifest.Parse (fileIo.files["C:\\Box\\manifest.fullsend"]));

            return manifest.GetRows();
        }
    };

    //  A disk's sectors from an image file, in DOS logical order.
    static Image  Sectors (const Image & file, const std::string & name)
    {
        Image               sectors;
        SectorDecodeReport  report;

        AssertSucceeded (VolumeImage::Load (file, name, sectors, report));

        return sectors;
    }

    //  The DOS 3.3 fixture with another file on it, so its catalog differs.
    static Image  WithFile (const Image & dos, const char * name)
    {
        FilePayload  payload;
        Image        next;

        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x6000;
        payload.hasLoadAddress = true;
        payload.bytes.assign (300, (Byte) name[0]);

        AssertSucceeded (Dos33Volume (dos).Write (FilePath::Parse (name), payload, next));

        return next;
    }




    //  Each format, for a DOS and a ProDOS disk: read into an image, written over
    //  a scratch disk in the other drive, and read back the same as the source.
    TEST_METHOD (WholeDisks_InEachFormat_ReadAndWriteBackTheSource)
    {
        Rig    rig (115200);
        Image  scratch (NibblizationLayer::kImageByteSize, 0xE5);

        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");

        for (const auto & [format, source] : { std::pair<DiskFormat, const Image *> { DiskFormat::Dsk, &rig.dosFile }, std::pair<DiskFormat, const Image *> { DiskFormat::Po, &rig.prodosFile } })
        {
            Image  expected = Sectors (*source, format == DiskFormat::Dsk ? "source.dsk" : "source.po");

            rig.Mount (1, format, *source);

            for (const char * name : { "out.dsk", "out.do", "out.po", "out.woz", "out.nib", "out.nb2" })
            {
                std::wstring  what (name, name + strlen (name));

                rig.Mount (2, DiskFormat::Dsk, scratch);

                auto  read = rig.session->ReadDisk (1, name, false);

                rig.RunUntil ([&] () { return rig.sink.IsDone (read); }, what.c_str());
                AssertSucceeded (read->result);
                Assert::IsTrue (Sectors (rig.fileIo.files[name], name) == expected, (what + L" has the source's sectors").c_str());

                auto  write = rig.session->WriteDisk (2, name);

                rig.RunUntil ([&] () { return rig.sink.IsDone (write); }, what.c_str());
                AssertSucceeded (write->result);

                auto  back = rig.session->ReadDisk (2, "back.dsk", false);

                rig.RunUntil ([&] () { return rig.sink.IsDone (back); }, what.c_str());
                AssertSucceeded (back->result);
                Assert::IsTrue (Sectors (rig.fileIo.files["back.dsk"], "back.dsk") == expected, (what + L" wrote the source's sectors").c_str());
            }
        }
    }




    //  V on the entry screen: every track is read three times, with no sector
    //  flagged on a good disk, so the Apple's CRCs match the PC's.
    TEST_METHOD (Verify_OnTheEntryScreen_ReadsAGoodDiskClean)
    {
        Rig  rig;

        rig.machine.GetDiskStore().Eject (kSlot, 1);
        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");
        rig.send = rig.session->FullSend ("C:\\Box", "", "");

        rig.RunUntil ([&] () { return rig.Row (kRowPrompt).starts_with ("V: VERIFY READS"); }, L"the entry screen");
        rig.Press ('v');
        rig.RunUntil ([&] () { return (rig.machine.GetCpu()->ReadByte ((Word) (kRowTitle + 13)) & 0xC0) == 0 && rig.Row (kRowPrompt).starts_with ("V: VERIFY READS"); }, L"VERIFY: ON is inverse, and the prompt is back");
        rig.Press (' ');
        rig.RunUntil ([&] () { return rig.Row (kRowCounts).starts_with ("SENT 1"); }, L"the disk is sent");
        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"the session pauses");
        rig.Press ('Q');
        rig.Finish();

        std::vector<FullSendRow>  rows = rig.Rows();

        Assert::AreEqual (std::string (FullSendManifest::kVerifyFull), rows[0].verification);
        Assert::AreEqual (0, rows[0].recovered, L"the Apple's CRCs match");
        Assert::AreEqual (0, rows[0].unreliable);
        Assert::IsTrue (Sectors (rig.fileIo.files["C:\\Box\\" + rows[0].file], rows[0].file) == Sectors (rig.dosFile, "d.dsk"));
    }



    //  P on the entry screen: a prefix typed on the Apple, with a letter erased,
    //  begins the image's name.
    TEST_METHOD (Prefix_TypedOnTheEntryScreen_BeginsTheImagesName)
    {
        static constexpr Word  s_kRowPrefix = 0x04D0;

        Rig  rig;

        rig.machine.GetDiskStore().Eject (kSlot, 1);
        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");
        rig.send = rig.session->FullSend ("C:\\Box", "", "");

        rig.RunUntil ([&] () { return rig.Row (s_kRowPrefix) == "P: FILE NAME PREFIX: NONE"; }, L"the entry screen");
        rig.Press ('p');
        rig.RunUntil ([&] () { return rig.Row (kRowPrompt2) == "PREFIX: _"; }, L"the prefix line");

        for (char key : std::string ("BOXX"))
        {
            rig.Press (key);
            rig.RunUntil ([&] () { return rig.Row (kRowPrompt2).ends_with (std::string (1, key) + "_"); }, L"the letter on the line");
        }

        rig.Press ('\x08');
        rig.RunUntil ([&] () { return rig.Row (kRowPrompt2) == "PREFIX: BOX_"; }, L"Left arrow erases a letter");
        rig.Press ('-');
        rig.RunUntil ([&] () { return rig.Row (kRowPrompt2) == "PREFIX: BOX-_"; }, L"the hyphen");
        rig.Press ('\r');
        rig.RunUntil ([&] () { return rig.Row (s_kRowPrefix) == "P: FILE NAME PREFIX: BOX-"; }, L"Return takes the prefix");
        rig.Press (' ');
        rig.RunUntil ([&] () { return rig.Row (kRowCounts).starts_with ("SENT 1"); }, L"the disk is sent");
        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"the session pauses");
        rig.Press ('Q');
        rig.Finish();

        Assert::IsTrue (rig.Rows()[0].file.starts_with ("BOX-0001"), TextEncoding::NarrowToWide (rig.Rows()[0].file).c_str());
        Assert::AreEqual (std::string ("BOX-"), rig.send->imagePrefix);
    }



    //  A connection that doesn't ask for 40 columns gets 80 on a machine with them:
    //  odd columns are in main memory, and a bar has two cells a track.
    TEST_METHOD (ThePc_SelectsEightyColumns_OnAMachineWithThem)
    {
        Rig  rig (115200, false);

        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");
        rig.machine.RunCycles (kCyclesPerS);

        Assert::AreEqual<int> ('A' | 0x80, rig.machine.GetCpu()->ReadByte (0x0400), L"column 1 of CASSOLINK");
        Assert::AreEqual<int> ('S' | 0x80, rig.machine.GetCpu()->ReadByte (0x0401), L"column 3");
        Assert::AreEqual<int> ('N' | 0x80, rig.machine.GetCpu()->ReadByte (0x0406), L"column 13, in CONNECTED");
        Assert::AreEqual<int> ('2' | 0x80, rig.machine.GetCpu()->ReadByte (0x0426), L"column 77, in the connection at the right edge");
        Assert::AreEqual<int> ('0' | 0x80, rig.machine.GetCpu()->ReadByte (0x0427), L"column 79, the speed's last digit");

        rig.send = rig.session->FullSend ("C:\\Box", "", "");
        rig.machine.RunCycles (kCyclesPerS);

        Assert::IsFalse (rig.sink.IsDone (rig.send), L"the session waits on its entry screen");

        rig.session->Cancel();
        rig.Finish();
    }



    //  Three disks with a swap during a transfer, at the //c's speed: no key and
    //  no PC interaction between them, each in about 30 seconds, and each image
    //  the same as its source.
    TEST_METHOD (SwapsDuringTransfers_RunWithNoKey_InAbout30SecondsADisk)
    {
        Rig         rig (115200);
        Image       third   = WithFile (Sectors (rig.dosFile, "d.dsk"), "THIRD");
        uint64_t    begun   = 0;
        bool        swapped = false;

        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");
        rig.send = rig.session->FullSend ("C:\\Box", "", "");
        rig.Begin();

        begun = rig.spent;
        rig.RunUntil ([&] ()
        {
            if (!swapped && rig.Row (kRowDrive2).starts_with ("D2: SENDING"))
            {
                rig.Mount (1, DiskFormat::Dsk, third);
                swapped = true;
            }

            return rig.Row (kRowCounts).starts_with ("SENT 3");
        }, L"the third disk is sent", true);

        uint64_t  perDisk = (rig.spent - begun) / 3;

        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"the session pauses");

        Logger::WriteMessage (std::format ("Full Send: {:.1f} seconds a disk", (double) perDisk / kCyclesPerS).c_str());

        rig.Press ('Q');
        rig.Finish();

        Assert::IsTrue (swapped);
        Assert::IsTrue (perDisk < kSecondsADisk * kCyclesPerS, L"a disk in the time allowed");

        std::vector<FullSendRow>  rows = rig.Rows();

        Assert::AreEqual ((size_t) 3, rows.size());
        Assert::AreEqual (1, rows[0].drive);
        Assert::AreEqual (2, rows[1].drive);
        Assert::AreEqual (1, rows[2].drive);

        Assert::IsTrue (Sectors (rig.fileIo.files["C:\\Box\\" + rows[0].file], rows[0].file) == Sectors (rig.dosFile, "d.dsk"));
        Assert::IsTrue (Sectors (rig.fileIo.files["C:\\Box\\" + rows[1].file], rows[1].file) == Sectors (rig.prodosFile, "d.po"));
        Assert::IsTrue (Sectors (rig.fileIo.files["C:\\Box\\" + rows[2].file], rows[2].file) == third);
        Assert::IsTrue (rows[1].file.ends_with (".po"));
        Assert::IsTrue (rows[0].file.ends_with (".dsk"));
    }




    //  No drive spins during the pause, and an
    //  empty drive is reported as empty.
    TEST_METHOD (ThePause_StopsTheMotor_AndAnEmptyDriveIsReported)
    {
        Rig  rig;

        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");
        rig.machine.GetDiskStore().Eject (kSlot, 1);
        rig.send = rig.session->FullSend ("C:\\Box", "", "");
        rig.Begin();

        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"a pause");
        Assert::IsTrue (rig.Row (kRowDrive2).starts_with ("D2: EMPTY OR NO RECOGNIZED SECTORS"), TextEncoding::NarrowToWide (rig.Row (kRowDrive2)).c_str());

        rig.machine.RunCycles (2ULL * kCyclesPerS);
        Assert::IsFalse (rig.machine.GetRefs().diskController->IsMotorOn(), L"no drive spins during the pause");

        rig.session->Cancel();
        rig.Finish();

        Assert::IsTrue (rig.send->state == LinkOperationState::Canceled);
        rig.RunUntil ([&] () { return rig.Row (kRowCounts).empty() && rig.Row (kRowDrive1) == "D1: IDLE"; }, L"the screen is cleared");
    }




    //  Copy-protected originals, as WOZ images of them. None is taken for a
    //  disk swapped during its read: Hard Hat Mack's catalog track has no
    //  sector that reads, and it is sent as likely protected, and The Print
    //  Shop's first program has a name of control characters, which is no
    //  title. A disk with no standard sector on track 0 has nothing to read.
    TEST_METHOD (CopyProtectedWozImages_AreNotTakenForASwap)
    {
        struct Expected
        {
            const char *  image;
            size_t        rows;
            const char *  status;
        };

        static constexpr Expected  s_kExpected[] =
        {
            { "Karateka.woz",                                     0, ""                                },
            { "Choplifter.woz",                                   0, ""                                },
            { "LodeRunner.woz",                                   0, ""                                },
            { "RockysBoots.woz",                                  0, ""                                },
            { "Where in the World is Carmen Sandiego side A.woz", 0, ""                                },
            { "HardHatMack.woz",                                  1, FullSendManifest::kStatusProtected },
            { "AppleStellarInvaders.woz",                         1, FullSendManifest::kStatusDamaged   },
            { "The Print Shop Color side A.woz",                  1, FullSendManifest::kStatusUnclear   },
            { "Where in the World is Carmen Sandiego side B.woz", 1, FullSendManifest::kStatusSent      },
        };

        for (const Expected & expected : s_kExpected)
        {
            Rig           rig;
            std::wstring  what = TextEncoding::NarrowToWide (expected.image);
            Image         woz  = GuestSession::FindGameImage (what.c_str());



            //  The images are in the Casso Disks folder, out of the repo.
            if (woz.empty())
            {
                Logger::WriteMessage ((L"SKIPPED: not in the Casso Disks folder: " + what).c_str());
                continue;
            }

            rig.machine.GetDiskStore().Eject (kSlot, 1);
            rig.Mount (1, DiskFormat::Woz, woz);
            rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");
            rig.send = rig.session->FullSend ("C:\\Box", "", "");
            rig.Begin();
            rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"the session pauses");

            for (const std::string & line : rig.sink.activity)
            {
                Assert::IsTrue (line.find ("changed while it was read") == std::string::npos, what.c_str());
            }

            if (expected.rows == 0)
            {
                Assert::IsFalse (rig.fileIo.files.contains ("C:\\Box\\manifest.fullsend"), what.c_str());
            }
            else
            {
                std::vector<FullSendRow>  rows = rig.Rows();

                Assert::AreEqual (expected.rows, rows.size(), what.c_str());
                Assert::AreEqual (std::string (expected.status), rows[0].status, what.c_str());
                Assert::IsTrue   (rows[0].file.find ('_') == std::string::npos, TextEncoding::NarrowToWide (rows[0].file).c_str());
                Assert::IsTrue   (rig.Row (kRowBar1).ends_with ("100%"), L"the bar ends full");
            }

            rig.Press ('Q');
            rig.Finish();
        }
    }
};

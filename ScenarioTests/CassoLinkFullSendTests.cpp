#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/FullSend/FullSendManifest.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
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
//  A full send against Casso's program on an emulated //c, over the
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
    static constexpr Word      kRowLabel    = 0x0780;
    static constexpr Word      kRowDrive1   = 0x0628;
    static constexpr Word      kRowDrive2   = 0x06A8;
    static constexpr Word      kRowImage    = 0x0728;
    static constexpr Word      kRowCounts   = 0x07A8;
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

        explicit Rig (DWORD shimBaud = 9600)
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

        bool  IsPrompting() { return Row (kRowLabel).starts_with ("PUT IN A NEW DISK"); }

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

            AssertSucceeded (manifest.Parse (fileIo.files["C:\\Box\\full-send.csv"]));

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

    //  The disk as a nibble image with the data field of one sector damaged:
    //  its prologue's last nibble changed, so no read of it can succeed.
    static Image  WithUnreadableSector (const Image & sectors)
    {
        static constexpr Byte  kPrologue[] = { 0xD5, 0xAA, 0xAD };
        constexpr int          kTrack      = 5;
        constexpr int          kOnTrack    = 3;
        Image                  nib;
        auto                   from        = nib.begin();

        AssertSucceeded (BlankDiskBuilder::WrapInContainer (DiskFormat::Nib, NibbleImageCodec::kNibTrackSize, false, 0, sectors, nib));

        from = nib.begin() + (ptrdiff_t) (kTrack * NibbleImageCodec::kNibTrackSize);

        for (int i = 0; i <= kOnTrack; i++)
        {
            from = std::search (from + (i == 0 ? 0 : 1), nib.end(), std::begin (kPrologue), std::end (kPrologue));
            Assert::IsTrue (from != nib.end());
        }

        *(from + 2) = 0xFF;

        return nib;
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
        rig.send = rig.session->FullSend ("C:\\Box", "");

        begun = rig.spent;
        rig.RunUntil ([&] ()
        {
            if (!swapped && rig.Row (kRowDrive2).starts_with ("D2 SENDING"))
            {
                rig.Mount (1, DiskFormat::Dsk, third);
                swapped = true;
            }

            return rig.Row (kRowCounts).starts_with ("SENT 3");
        }, L"the third disk is sent", true);

        uint64_t  perDisk = (rig.spent - begun) / 3;

        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"the session pauses");

        Logger::WriteMessage (std::format ("Full send: {:.1f} seconds a disk", (double) perDisk / kCyclesPerS).c_str());

        rig.Press (0x1B);
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




    //  A duplicate and a disk with an unreadable sector: both listed, no second
    //  image, and the session goes on past each.
    TEST_METHOD (ADuplicateAndABadSector_AreListed_AndTheSessionContinues)
    {
        Rig    rig;
        Image  dos    = Sectors (rig.dosFile, "d.dsk");
        Image  bad    = WithFile (dos, "BAD");
        int    pauses = 0;

        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");

        //  Drive 2 has a copy of drive 1's disk: same fingerprint, so a pause.
        rig.Mount (2, DiskFormat::Dsk, rig.dosFile);
        rig.send = rig.session->FullSend ("C:\\Box", "");

        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"a pause, as drive 2's disk repeats drive 1's");
        pauses++;

        Assert::IsTrue (rig.Row (kRowDrive2).starts_with ("D2 ALREADY SENT"), TextEncoding::NarrowToWide (rig.Row (kRowDrive2)).c_str());

        //  Space sends the repeat, and its hash makes it a duplicate.
        rig.Press (' ');
        rig.RunUntil ([&] () { return !rig.IsPrompting(); }, L"Space ends the pause");
        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"the next pause");
        pauses++;

        Assert::IsTrue (rig.Row (kRowCounts).find ("DUPLICATES 1") != std::string::npos, TextEncoding::NarrowToWide (rig.Row (kRowCounts)).c_str());

        //  The next disk has an unreadable sector.
        rig.Mount (1, DiskFormat::Nib, WithUnreadableSector (bad));
        rig.Press (' ');
        rig.RunUntil ([&] () { return !rig.IsPrompting(); }, L"Space ends the pause");
        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"the session goes on past the bad sector");

        Assert::IsTrue (rig.Row (kRowCounts).find ("BAD 1") != std::string::npos, TextEncoding::NarrowToWide (rig.Row (kRowCounts)).c_str());

        rig.Press (0x1B);
        rig.Finish();

        std::vector<FullSendRow>  rows = rig.Rows();

        Assert::AreEqual (2, pauses);
        Assert::AreEqual ((size_t) 3, rows.size());
        Assert::AreEqual (std::string ("sent"), rows[0].status);
        Assert::AreEqual (std::string ("duplicate"), rows[1].status);
        Assert::AreEqual (rows[0].file, rows[1].duplicateOf);
        Assert::IsTrue (rows[1].file.empty());
        Assert::AreEqual (std::string ("possibly-protected"), rows[2].status);
        Assert::AreEqual (1, rows[2].unreadable);

        //  The two images, the manifest and the unreadable list, and no duplicate.
        Assert::AreEqual ((size_t) 4, rig.fileIo.files.size());
        Assert::IsTrue (rig.fileIo.files.contains ("C:\\Box\\" + rows[2].file + ".unreadable.txt"));
    }




    //  No drive spins during the pause, and an
    //  empty drive is reported as empty.
    TEST_METHOD (ThePause_StopsTheMotor_AndAnEmptyDriveIsReported)
    {
        Rig  rig;

        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");
        rig.machine.GetDiskStore().Eject (kSlot, 1);
        rig.send = rig.session->FullSend ("C:\\Box", "");

        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"a pause");
        Assert::IsTrue (rig.Row (kRowDrive2).starts_with ("D2 NO DISK"), TextEncoding::NarrowToWide (rig.Row (kRowDrive2)).c_str());

        rig.machine.RunCycles (2ULL * kCyclesPerS);
        Assert::IsFalse (rig.machine.GetRefs().diskController->IsMotorOn(), L"no drive spins during the pause");

        rig.session->Cancel();
        rig.Finish();

        Assert::IsTrue (rig.send->state == LinkOperationState::Canceled);
        rig.RunUntil ([&] () { return rig.Row (kRowDrive1).empty() && rig.Row (kRowImage).empty(); }, L"the screen is cleared");
    }




    //  Escape during a transfer: no partial image, and CassoLink goes on.
    TEST_METHOD (EscapeDuringATransfer_LeavesNoImage_AndTheLinkRunning)
    {
        Rig  rig;

        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");
        rig.send = rig.session->FullSend ("C:\\Box", "");

        rig.RunUntil ([&] () { return rig.Row (kRowDrive1).starts_with ("D1 SENDING"); }, L"the transfer starts");
        rig.machine.RunCycles (kCyclesPerS / 2);
        rig.Press (0x1B);
        rig.Finish();

        Assert::IsTrue (rig.send->state == LinkOperationState::Canceled);
        Assert::IsTrue (rig.fileIo.files.empty(), L"no image, no manifest");

        auto  list = rig.session->Enumerate (1, FilePath());

        rig.RunUntil ([&] () { return rig.sink.IsDone (list); }, L"the link goes on");
        AssertSucceeded (list->result);
    }




    //  A disk swapped during its transfer: no file for it, and the session
    //  goes on with the disk now in the drive.
    TEST_METHOD (ADiskSwappedDuringItsTransfer_LeavesNoFile_AndTheSessionContinues)
    {
        Rig    rig;
        Image  other = WithFile (Sectors (rig.dosFile, "d.dsk"), "OTHER");
        bool   swapped = false;

        rig.RunUntil ([&] () { return rig.session->GetState() == LinkState::Ready; }, L"the link connects");
        rig.machine.GetDiskStore().Eject (kSlot, 1);
        rig.send = rig.session->FullSend ("C:\\Box", "");

        rig.RunUntil ([&] () { return rig.Row (kRowDrive1).starts_with ("D1 SENDING"); }, L"the transfer starts");
        rig.machine.RunCycles (kCyclesPerS);
        rig.Mount (1, DiskFormat::Dsk, other);
        swapped = true;

        rig.RunUntil ([&] () { return rig.IsPrompting(); }, L"the session pauses");
        rig.Press (0x1B);
        rig.Finish();

        std::vector<FullSendRow>  rows = rig.Rows();

        Assert::IsTrue (swapped);
        Assert::AreEqual ((size_t) 1, rows.size(), L"only the disk now in the drive has a row");
        Assert::IsTrue (Sectors (rig.fileIo.files["C:\\Box\\" + rows[0].file], rows[0].file) == other);
    }
};

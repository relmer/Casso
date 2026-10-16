#include "Pch.h"

#include "EhmTestHelper.h"
#include "FixtureProvider.h"
#include "TestMachine.h"
#include "AppleLink/InMemoryAppleLinkFolder.h"
#include "EmuTests/FakeDiskFileIo.h"
#include "AppleLink/AppleLinkSession.h"
#include "AppleLink/LinkProgram.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Seams/IByteStreamFactory.h"
#include "Seams/InProcessByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSessionScenarioTests
//
//  AppleLinkSession against Casso's own program on an emulated //c, over
//  the in-process bridge: a DOS 3.3 image in drive 1 and a ProDOS image in
//  drive 2. Both catalogs are listed, a file is copied out of each disk and
//  one onto each, each disk is read to an image, and an image is written
//  back; every file and image is compared byte for byte with its source.
//
//  The session runs on its own thread, as in Casso Explorer, while the test
//  steps the machine.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleLinkSessionScenarioTests)
{
public:

    static constexpr uint64_t  kFrame  = 17030;
    static constexpr uint64_t  kBudget = 4'000'000'000;

    //  The one line to the machine, as the session's TCP connection.
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
            bool     IsClosed() const override                                                            { return end->IsClosed(); }
            void     Close() override                                                                  { end->Close(); }
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

    struct Sink : IAppleLinkSessionSink
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
        InMemoryAppleLinkFolder               folder;
        FakeDiskFileIo                        fileIo;
        Sink                                  sink;
        std::unique_ptr<AppleLinkSession>     session;
        std::vector<Byte>                     dosFile;
        std::vector<Byte>                     prodosFile;

        Rig()
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;
            std::vector<Byte>     shim;
            std::string           errors;
            Word                  at = LinkProgram::kShimAddress;
            AppleLinkSettings     settings;

            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            dosFile.assign (bytes.begin(), bytes.end());
            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/prodos.po", bytes));
            prodosFile.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            machine.PowerCycle();

            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Dsk, dosFile));
            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 1, "", DiskFormat::Po, prodosFile));
            machine.GetRefs().diskController->SetExternalDisk (0, machine.GetDiskStore().GetImage (6, 0));
            machine.GetRefs().diskController->SetExternalDisk (1, machine.GetDiskStore().GetImage (6, 1));

            InProcessByteStream::CreatePair (machineEnd, factory.pcEnd);
            Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

            Assert::AreEqual (S_OK, LinkProgram::AssembleShim (shim, errors));

            for (Byte b : shim)
            {
                machine.GetCpu()->WriteByte (at++, b);
            }

            machine.GetCpu()->SetPC (LinkProgram::kShimAddress);

            settings.name      = "//c";
            settings.transport = LinkTransport::Emulated;
            settings.address   = "127.0.0.1:6502";

            session = std::make_unique<AppleLinkSession> (factory, folder, fileIo, sink);
            AssertSucceeded (session->Start (settings));
        }

        ~Rig()
        {
            session.reset();
        }

        //  Steps the machine until the operation is done, and returns it.
        std::shared_ptr<LinkOperation>  Finish (std::shared_ptr<LinkOperation> operation)
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

        //  A drive's disk, as DOS-logical sectors.
        std::vector<Byte>  Sectors (int drive)
        {
            std::vector<Byte>   file;
            std::vector<Byte>   sectors;
            SectorDecodeReport  report;

            AssertSucceeded (machine.GetDiskStore().GetImage (6, drive - 1)->Serialize (file));
            AssertSucceeded (VolumeImage::Load (file, drive == 1 ? "d.dsk" : "d.po", sectors, report));
            return sectors;
        }
    };

    static std::vector<std::string>  Names (const VolumeListing & listing)
    {
        std::vector<std::string>  names;

        for (const auto & entry : listing.entries)
        {
            names.push_back (entry.name);
        }

        return names;
    }

    static std::unique_ptr<IVolume>  Open (const std::vector<Byte> & sectors)
    {
        if (VolumeImage::DetectFilesystem (sectors) == VolumeKind::Dos33)
        {
            return std::make_unique<Dos33Volume> (sectors);
        }

        return std::make_unique<ProDosVolume> (sectors);
    }

    static FilePayload  Binary (Byte type, size_t length, Byte seed)
    {
        FilePayload  payload;

        payload.type           = type;
        payload.loadAddress    = 0x6000;
        payload.hasLoadAddress = true;
        payload.bytes.resize (length);

        for (size_t i = 0; i < length; i++)
        {
            payload.bytes[i] = (Byte) (seed + i * 3 + (i >> 8));
        }

        return payload;
    }




    TEST_METHOD (ListCopyReadAndWriteBothDisks)
    {
        Rig  rig;

        for (int drive : { 1, 2 })
        {
            std::vector<Byte>         before  = rig.Sectors (drive);
            std::unique_ptr<IVolume>  volume  = Open (before);
            VolumeListing             expected;
            FilePayload               file;
            FilePayload               payload = Binary (drive == 1 ? Dos33Volume::kTypeBinary : ProDosVolume::kTypeBinary, 1800, (Byte) drive);
            std::vector<Byte>         written;

            AssertSucceeded (volume->Enumerate (expected));
            Assert::IsFalse (expected.entries.empty());

            //  The catalog.
            auto  list = rig.Finish (rig.session->Enumerate (drive, FilePath()));

            AssertSucceeded (list->result);
            Assert::IsTrue (Names (list->listing) == Names (expected), std::format (L"drive {}'s catalog", drive).c_str());

            //  A file out.
            AssertSucceeded (volume->Read (FilePath::Parse (expected.entries[0].name), file));

            auto  read = rig.Finish (rig.session->ReadFile (drive, FilePath::Parse (expected.entries[0].name)));

            AssertSucceeded (read->result);
            Assert::IsTrue (read->file.bytes == file.bytes, std::format (L"drive {}'s file", drive).c_str());

            //  A file in.
            AssertSucceeded (volume->Write (FilePath::Parse ("FROMPC"), payload, written));

            auto  write = rig.Finish (rig.session->WriteFile (drive, FilePath::Parse ("FROMPC"), payload));

            AssertSucceeded (write->result);
            Assert::IsTrue (rig.Sectors (drive) == written, std::format (L"drive {} has the written file", drive).c_str());

            //  The whole disk out.
            std::string  name = drive == 1 ? "drive1.dsk" : "drive2.po";
            auto                disk    = rig.Finish (rig.session->ReadDisk (drive, name, false));
            std::vector<Byte>   sectors;
            SectorDecodeReport  report;

            AssertSucceeded (disk->result);
            AssertSucceeded (VolumeImage::Load (rig.fileIo.files[name], name, sectors, report));
            Assert::IsTrue (sectors == written, std::format (L"drive {}'s image", drive).c_str());
        }

        //  An image onto drive 1: the DOS 3.3 disk with another file.
        {
            std::vector<Byte>  image = rig.Sectors (1);
            std::vector<Byte>  next;

            AssertSucceeded (Dos33Volume (image).Write (FilePath::Parse ("IMAGED"), Binary (Dos33Volume::kTypeBinary, 700, 9), next));
            rig.fileIo.files["back.dsk"] = next;

            AssertSucceeded (rig.Finish (rig.session->WriteDisk (1, "back.dsk"))->result);
            Assert::IsTrue (rig.Sectors (1) == next, L"drive 1 is the image");
        }
    }
};

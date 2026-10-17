#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeApple.h"
#include "InMemoryAppleLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "AppleLink/AppleLinkSession.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLinkSessionTests
//
//  The session against FakeApple, over in-process lines: opening, loading
//  the program, the operations on a DOS 3.3 disk in drive 1 and a ProDOS
//  disk in drive 2, one at a time, Cancel, a disk changed under its
//  listing, and a line that drops.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleLinkSessionTests)
{
public:

    struct Sink : IAppleLinkSessionSink
    {
        std::mutex                                    lock;
        std::condition_variable                       changed;
        std::vector<LinkState>                        states;
        std::vector<std::shared_ptr<LinkOperation>>   done;

        void  OnStateChanged (LinkState state) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            states.push_back (state);
            changed.notify_all();
        }

        void  OnProgress (const std::shared_ptr<LinkOperation> &) override
        {
        }

        void  OnDone (const std::shared_ptr<LinkOperation> & operation) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            done.push_back (operation);
            changed.notify_all();
        }

        bool  WaitForState (LinkState state, DWORD timeoutMs = 10000)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (timeoutMs), [&] () { return !states.empty() && states.back() == state; });
        }

        bool  WaitForDone (const std::shared_ptr<LinkOperation> & operation, DWORD timeoutMs = 20000)
        {
            std::unique_lock<std::mutex>  guard (lock);

            return changed.wait_for (guard, std::chrono::milliseconds (timeoutMs), [&] () { return std::find (done.begin(), done.end(), operation) != done.end(); });
        }
    };

    struct Rig
    {
        FakeApple                          apple;
        FakeAppleFactory                   factory { apple };
        InMemoryAppleLinkFolder            folder;
        FakeDiskFileIo                     fileIo;
        Sink                               sink;
        std::unique_ptr<AppleLinkSession>  session;

        Rig()
        {
            apple.drives[0] = BuildDos33();
            apple.drives[1] = BuildProDos();
            session         = std::make_unique<AppleLinkSession> (factory, folder, fileIo, sink);
        }

        ~Rig()
        {
            session.reset();
        }

        void  Start (DWORD connectTimeoutMs = 10000)
        {
            AppleLinkSettings  settings;

            settings.name      = "Test";
            settings.transport = LinkTransport::Emulated;
            settings.address   = "127.0.0.1:6502";

            session->SetConnectTimeout (connectTimeoutMs);
            AssertSucceeded (session->Start (settings));
        }

        std::shared_ptr<LinkOperation>  Finish (std::shared_ptr<LinkOperation> operation)
        {
            Assert::IsTrue (sink.WaitForDone (operation), L"the operation finishes");
            return operation;
        }

        std::vector<Byte>  Drive (int drive)
        {
            std::lock_guard<std::mutex>  guard (apple.lock);

            return apple.drives[(size_t) drive - 1];
        }
    };

    static FilePayload  Binary (Byte type, size_t length, Byte seed)
    {
        FilePayload  payload;

        payload.type           = type;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;
        payload.bytes.resize (length);

        for (size_t i = 0; i < length; i++)
        {
            payload.bytes[i] = (Byte) (seed + i * 5 + (i >> 8));
        }

        return payload;
    }

    static std::vector<Byte>  BuildDos33()
    {
        std::vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  next;

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));
        AssertSucceeded (Dos33Volume (buffer).Write (FilePath::Parse ("HELLO"), Binary (Dos33Volume::kTypeBinary, 2000, 1), next));

        return next;
    }

    static std::vector<Byte>  BuildProDos()
    {
        std::vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  next;

        AssertSucceeded (ProDosSkeleton::Write (buffer, "LINKED"));
        AssertSucceeded (ProDosVolume (buffer).Write (FilePath::Parse ("HELLO"), Binary (ProDosVolume::kTypeBinary, 2000, 2), next));

        return next;
    }

    static std::vector<std::string>  Names (const VolumeListing & listing)
    {
        std::vector<std::string>  names;

        for (const auto & entry : listing.entries)
        {
            names.push_back (entry.name);
        }

        return names;
    }




    TEST_METHOD (Start_LoadsTheProgramIntoTheShim_ThenIsReady)
    {
        Rig                rig;
        std::vector<Byte>  program;
        std::string        errors;

        rig.apple.startAsShim  = true;
        rig.apple.nakFirstPage = true;
        rig.Start();

        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready));
        Assert::AreEqual<int> (0x03, rig.session->GetDriveMap());

        AssertSucceeded (LinkProgram::AssembleProgram (program, errors));

        std::lock_guard<std::mutex>  guard (rig.apple.lock);

        Assert::IsTrue (std::equal (program.begin(), program.end(), rig.apple.loadedProgram.begin()), L"the program, page by page");
    }




    TEST_METHOD (AnAppleWhoseReadyCameEarly_IsPromptedForAnother)
    {
        Rig  rig;

        rig.apple.startAsShim    = true;
        rig.apple.readySentEarly = true;
        rig.Start();

        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready), L"Idle drew the shim's Ready");
    }




    TEST_METHOD (Enumerate_ListsBothDrives)
    {
        Rig            rig;
        VolumeListing  dos;
        VolumeListing  prodos;

        AssertSucceeded (Dos33Volume (rig.apple.drives[0]).Enumerate (dos));
        AssertSucceeded (ProDosVolume (rig.apple.drives[1]).Enumerate (prodos));

        rig.Start();

        auto  first  = rig.Finish (rig.session->Enumerate (1, FilePath()));
        auto  second = rig.Finish (rig.session->Enumerate (2, FilePath()));

        AssertSucceeded (first->result);
        AssertSucceeded (second->result);
        Assert::IsTrue (Names (first->listing) == Names (dos));
        Assert::IsTrue (Names (second->listing) == Names (prodos));
        Assert::AreEqual (prodos.volumeName, second->listing.volumeName);
    }




    TEST_METHOD (WriteFile_ThenReadFile_OnBothDrives)
    {
        Rig          rig;
        FilePayload  dosFile    = Binary (Dos33Volume::kTypeBinary, 1500, 7);
        FilePayload  prodosFile = Binary (ProDosVolume::kTypeBinary, 1500, 8);

        rig.Start();

        for (int drive : { 1, 2 })
        {
            std::vector<Byte>  before   = rig.Drive (drive);
            std::vector<Byte>  expected;
            FilePayload      & payload  = drive == 1 ? dosFile : prodosFile;

            if (drive == 1)
            {
                AssertSucceeded (Dos33Volume (before).Write (FilePath::Parse ("NEWFILE"), payload, expected));
            }
            else
            {
                AssertSucceeded (ProDosVolume (before).Write (FilePath::Parse ("NEWFILE"), payload, expected));
            }

            AssertSucceeded (rig.Finish (rig.session->Enumerate (drive, FilePath()))->result);
            AssertSucceeded (rig.Finish (rig.session->WriteFile (drive, FilePath::Parse ("NEWFILE"), payload))->result);
            Assert::IsTrue (rig.Drive (drive) == expected, L"the drive has the volume the write makes");

            auto  read = rig.Finish (rig.session->ReadFile (drive, FilePath::Parse ("NEWFILE")));

            AssertSucceeded (read->result);
            Assert::IsTrue (read->file.bytes == payload.bytes);
        }
    }




    TEST_METHOD (DeleteRenameAndMakeDirectory_OnProDos)
    {
        Rig                rig;
        std::vector<Byte>  expected = rig.apple.drives[1];
        std::vector<Byte>  next;

        AssertSucceeded (ProDosVolume (expected).CreateDirectory (FilePath::Parse ("STUFF"), next));
        expected = next;
        AssertSucceeded (ProDosVolume (expected).Rename (FilePath::Parse ("HELLO"), "GREETING", next));
        expected = next;
        AssertSucceeded (ProDosVolume (expected).Delete (FilePath::Parse ("GREETING"), next));
        expected = next;

        rig.Start();

        AssertSucceeded (rig.Finish (rig.session->MakeDirectory (2, FilePath::Parse ("STUFF")))->result);
        AssertSucceeded (rig.Finish (rig.session->Rename (2, FilePath::Parse ("HELLO"), "GREETING"))->result);
        AssertSucceeded (rig.Finish (rig.session->Delete (2, FilePath::Parse ("GREETING")))->result);

        Assert::IsTrue (rig.Drive (2) == expected);
    }




    TEST_METHOD (AWrite_ToADiskChangedSinceItsListing_Fails)
    {
        Rig                rig;
        std::vector<Byte>  changed;

        rig.Start();

        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);

        AssertSucceeded (Dos33Volume (rig.apple.drives[0]).Write (FilePath::Parse ("ELSEWHERE"), Binary (Dos33Volume::kTypeBinary, 300, 4), changed));

        {
            std::lock_guard<std::mutex>  guard (rig.apple.lock);

            rig.apple.drives[0] = changed;
        }

        auto  write = rig.Finish (rig.session->WriteFile (1, FilePath::Parse ("NEWFILE"), Binary (Dos33Volume::kTypeBinary, 300, 5)));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED), write->result);
        Assert::IsFalse (write->message.empty());
        Assert::IsTrue (rig.Drive (1) == changed, L"nothing written");
    }




    TEST_METHOD (AWriteProtectedDisk_FailsTheWrite)
    {
        Rig  rig;

        rig.apple.writeProtected[0] = true;
        rig.Start();

        auto  write = rig.Finish (rig.session->WriteFile (1, FilePath::Parse ("NEWFILE"), Binary (Dos33Volume::kTypeBinary, 300, 5)));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT), write->result);
        Assert::IsTrue (write->state == LinkOperationState::Failed);
    }




    TEST_METHOD (Operations_RunOneAtATime_InOrder)
    {
        Rig  rig;

        rig.Start();

        auto  a = rig.session->Enumerate (1, FilePath());
        auto  b = rig.session->ReadFile (1, FilePath::Parse ("HELLO"));
        auto  c = rig.session->Enumerate (2, FilePath());

        rig.Finish (c);

        std::lock_guard<std::mutex>  guard (rig.sink.lock);

        Assert::AreEqual (size_t (3), rig.sink.done.size());
        Assert::IsTrue (rig.sink.done[0] == a && rig.sink.done[1] == b && rig.sink.done[2] == c);
    }




    TEST_METHOD (Cancel_StopsTheRunningOperation_AndTheQueuedOneRuns)
    {
        Rig  rig;

        rig.Start();
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready));

        rig.apple.sectorDelayMs = 30;

        auto  read = rig.session->ReadFile (1, FilePath::Parse ("HELLO"));
        auto  list = rig.session->Enumerate (2, FilePath());

        while (rig.apple.sectorsRead < 3)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        rig.session->Cancel();
        rig.apple.sectorDelayMs = 0;

        rig.Finish (read);
        rig.Finish (list);

        Assert::IsTrue (read->state == LinkOperationState::Canceled);
        Assert::IsTrue (list->state == LinkOperationState::Done);
        Assert::AreEqual (1, rig.apple.cancels.load());
    }




    TEST_METHOD (ALostLine_Disconnects_AndTheNextRequestReconnects)
    {
        Rig  rig;

        rig.Start();
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready));

        rig.apple.Detach();
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Disconnected));

        auto  list = rig.Finish (rig.session->Enumerate (1, FilePath()));

        AssertSucceeded (list->result);
        Assert::AreEqual (2, rig.factory.connections.load());
    }




    TEST_METHOD (ReadDisk_WritesTheImage_InEachFormat)
    {
        Rig  rig;

        rig.Start();

        for (const char * name : { "out.dsk", "out.po", "out.woz", "out.nib" })
        {
            std::vector<Byte>   sectors;
            SectorDecodeReport  report;
            auto                read = rig.Finish (rig.session->ReadDisk (2, name, false));

            AssertSucceeded (read->result);
            AssertSucceeded (VolumeImage::Load (rig.fileIo.files[name], name, sectors, report));
            Assert::IsTrue (sectors == rig.Drive (2), std::format (L"{} has the disk", std::wstring (name, name + strlen (name))).c_str());
        }

        Assert::IsTrue (rig.fileIo.files["out.dsk"] == rig.Drive (2), L"a .dsk is the sectors in DOS 3.3 order");
    }




    TEST_METHOD (ReadDisk_WithUnreadableSectors_FailsOrWritesThemBlank)
    {
        Rig                rig;
        std::vector<Byte>  expected;

        rig.apple.badSectors = { { 3, 5 }, { 20, 0 } };
        rig.Start();

        auto  strict = rig.Finish (rig.session->ReadDisk (1, "strict.dsk", false));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_CRC), strict->result);
        Assert::AreEqual (size_t (2), strict->unreadable.size());
        Assert::IsFalse (strict->message.empty());
        Assert::IsTrue (rig.fileIo.files.find ("strict.dsk") == rig.fileIo.files.end(), L"no file from a failed read");

        auto  blank = rig.Finish (rig.session->ReadDisk (1, "blank.dsk", true));

        AssertSucceeded (blank->result);

        expected = rig.Drive (1);

        for (const auto & [track, physical] : rig.apple.badSectors)
        {
            auto  at = expected.begin() + (track * 16 + AppleLinkSession::GetLogicalSector (physical)) * 256;

            std::fill (at, at + 256, (Byte) 0);
            Assert::IsTrue (std::find (blank->unreadable.begin(), blank->unreadable.end(), std::pair<int, int> { track, AppleLinkSession::GetLogicalSector (physical) }) != blank->unreadable.end());
        }

        Assert::IsTrue (rig.fileIo.files["blank.dsk"] == expected, L"the unreadable sectors as zeros, the rest as read");
    }




    TEST_METHOD (WriteDisk_PutsTheImageOnTheDisk)
    {
        Rig                rig;
        std::vector<Byte>  image  = BuildProDos();
        std::vector<Byte>  file;
        std::vector<Byte>  next;

        AssertSucceeded (ProDosVolume (image).Write (FilePath::Parse ("EXTRA"), Binary (ProDosVolume::kTypeBinary, 900, 6), next));
        image = next;
        VolumeImage::DosLogicalToProDosFile (image, file);
        rig.fileIo.files["in.po"] = file;

        rig.Start();

        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);
        AssertSucceeded (rig.Finish (rig.session->WriteDisk (1, "in.po"))->result);

        Assert::IsTrue (rig.Drive (1) == image);

        auto  list = rig.Finish (rig.session->Enumerate (1, FilePath()));

        AssertSucceeded (list->result);
        Assert::AreEqual (std::string ("LINKED"), list->listing.volumeName, L"the drive is listed afresh");
    }




    TEST_METHOD (WriteDisk_AnImageWithNonstandardTracks_WritesNothing)
    {
        Rig                rig;
        std::vector<Byte>  nib;
        std::vector<Byte>  before = rig.apple.drives[0];

        AssertSucceeded (BlankDiskBuilder::WrapInContainer (DiskFormat::Nib, NibbleImageCodec::kNibTrackSize, false, 0, BuildDos33(), nib));
        std::fill (nib.begin() + 5 * NibbleImageCodec::kNibTrackSize, nib.begin() + 6 * NibbleImageCodec::kNibTrackSize, (Byte) 0);
        rig.fileIo.files["odd.nib"] = nib;

        rig.Start();

        auto  write = rig.Finish (rig.session->WriteDisk (1, "odd.nib"));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), write->result);
        Assert::IsTrue (write->message.find ("5") != std::string::npos, L"the track is listed");
        Assert::AreEqual (0, rig.apple.sectorsWritten.load());
        Assert::IsTrue (rig.Drive (1) == before);
    }




    TEST_METHOD (ReadDisk_Canceled_WritesNoFile)
    {
        Rig  rig;

        rig.Start();
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready));

        rig.apple.sectorDelayMs = 2;

        auto  read = rig.session->ReadDisk (1, "canceled.dsk", false);

        while (rig.apple.sectorsRead < 40)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        rig.session->Cancel();
        rig.Finish (read);

        Assert::IsTrue (read->state == LinkOperationState::Canceled);
        Assert::IsTrue (rig.fileIo.files.find ("canceled.dsk") == rig.fileIo.files.end());
    }




    TEST_METHOD (NoAppleOnTheLine_FailsWhatIsQueued)
    {
        Rig  rig;

        rig.factory.answer = false;
        rig.Start (500);

        auto  list = rig.Finish (rig.session->Enumerate (1, FilePath()));

        Assert::IsTrue (list->state == LinkOperationState::Failed);
        Assert::IsFalse (list->message.empty());
    }



    TEST_METHOD (EachOperation_SendsItsLabelFirst)
    {
        Rig                       rig;
        std::vector<std::string>  labels;

        rig.Start();
        rig.Finish (rig.session->Enumerate (1, FilePath()));
        rig.Finish (rig.session->WriteFile (1, FilePath::Parse ("NOTES"), Binary (Dos33Volume::kTypeBinary, 300, 7)));

        {
            std::lock_guard<std::mutex>  guard (rig.apple.lock);

            labels = rig.apple.labels;
        }

        Assert::AreEqual ((size_t) 2, labels.size());
        Assert::AreEqual (std::string ("READING THE CATALOG OF D1"), labels[0]);
        Assert::AreEqual (std::string ("COPYING NOTES TO D1 (VOL 254)"), labels[1]);
    }




    TEST_METHOD (DescribeOperation_GivesTheProgramsLabel)
    {
        LinkOperation  operation;
        VolumeListing  listing;

        operation.kind    = LinkOperationKind::Rename;
        operation.drive   = 2;
        operation.path    = FilePath::Parse ("GAMES/OLD");
        operation.newName = "NEW";
        Assert::AreEqual (std::string ("RENAMING GAMES/OLD TO NEW"), AppleLinkSession::DescribeOperation (operation));

        operation.kind = LinkOperationKind::ReadDisk;
        Assert::AreEqual (std::string ("SENDING D2 TO PC"), AppleLinkSession::DescribeOperation (operation));

        listing.hasVolumeName = true;
        listing.volumeName    = "GAMES";
        Assert::AreEqual (std::string ("SENDING D2 (/GAMES) TO PC"), AppleLinkSession::DescribeOperation (operation, listing));

        listing.hasVolumeName   = false;
        listing.hasVolumeNumber = true;
        listing.volumeNumber    = 254;
        operation.kind          = LinkOperationKind::WriteDisk;
        Assert::AreEqual (std::string ("WRITING AN IMAGE TO D2 (VOL 254)"), AppleLinkSession::DescribeOperation (operation, listing));
    }



    TEST_METHOD (ALinkStartedBeforeItsApple_ConnectsOnItsOwn_WhenTheAppleAnswers)
    {
        Rig  rig;

        rig.factory.answer = false;
        rig.session->SetRetryInterval (100);
        rig.Start (300);

        Assert::IsTrue (rig.sink.WaitForState (LinkState::Disconnected), L"no Apple yet");

        rig.factory.answer = true;
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready), L"connected with no request made");
    }
};
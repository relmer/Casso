#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeApple.h"
#include "InMemoryCassoLinkFolder.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "Machines/Apple2/Common/BlankDiskBuilder.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkSessionTests
//
//  The session against FakeApple, over in-process lines: opening, loading
//  the program, the operations on a DOS 3.3 disk in drive 1 and a ProDOS
//  disk in drive 2, one at a time, Cancel, a disk changed under its
//  listing, and a line that drops.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkSessionTests)
{
public:

    struct Sink : ICassoLinkSessionSink
    {
        std::mutex                                    lock;
        std::condition_variable                       changed;
        std::vector<LinkState>                        states;
        std::vector<std::shared_ptr<LinkOperation>>   done;
        std::vector<std::string>                      activities;
        std::vector<std::shared_ptr<LinkOperation>>   queued;
        std::string                                   fullSendFolder;

        bool  TryGetFullSendTarget (std::string & outFolder, std::string & outExtension) override
        {
            outFolder    = fullSendFolder;
            outExtension = ".do";

            return !fullSendFolder.empty();
        }

        void  OnOperationQueued (const std::shared_ptr<LinkOperation> & operation) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            queued.push_back (operation);
        }

        void  OnActivity (const std::string & text) override
        {
            std::lock_guard<std::mutex>  guard (lock);

            activities.push_back (text);
        }

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
        InMemoryCassoLinkFolder            folder;
        FakeDiskFileIo                     fileIo;
        Sink                               sink;
        std::unique_ptr<CassoLinkSession>  session;

        Rig()
        {
            apple.drives[0] = BuildDos33();
            apple.drives[1] = BuildProDos();
            session         = std::make_unique<CassoLinkSession> (factory, folder, fileIo, sink);
        }

        ~Rig()
        {
            session.reset();
        }

        void  Start (DWORD connectTimeoutMs = 10000)
        {
            CassoLinkSettings  settings;

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




    //  A boot disk still starting when the host first prompts it, as a
    //  universal disk asking for an address is, misses that prompt and sends
    //  no Ready of its own. The line is kept open between attempts, and each
    //  attempt prompts it again; once was all it got.
    TEST_METHOD (AnAppleThatMissedThePrompt_IsPromptedAgainOnTheNextAttempt)
    {
        Rig  rig;

        rig.apple.startAsShim    = true;
        rig.apple.readySentEarly = true;
        rig.apple.promptsMissed  = 1;
        rig.apple.readyEveryMs   = 60000;
        rig.session->SetRetryInterval (100);
        rig.Start (500);

        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready, 20000), L"the next attempt's prompt drew the Ready");
    }




    //  A session stopped on purpose is disconnected, not an Apple that
    //  stopped answering.
    TEST_METHOD (Stop_IsReportedAsDisconnected)
    {
        Rig  rig;

        rig.Start();
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready));

        rig.session->Stop();

        std::lock_guard<std::mutex>  guard (rig.sink.lock);

        Assert::IsFalse (rig.sink.activities.empty());
        Assert::AreEqual (std::string ("Disconnected"), rig.sink.activities.back());
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




    //  After a request lost on the line, the Apple is idle and sends Ready:
    //  the host sends the request again rather than waiting on those for good.
    TEST_METHOD (ALostRequest_IsSentAgain)
    {
        Rig  rig;

        rig.apple.readyEveryMs = 200;
        rig.Start();

        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);

        rig.apple.dropRequests = 1;

        auto  list = rig.Finish (rig.session->Enumerate (2, FilePath()));

        AssertSucceeded (list->result);
        Assert::AreEqual (1, rig.apple.dropped.load(), L"one request was dropped and sent again");
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




    //  The link writes the same sectors a local change would.
    TEST_METHOD (SetAttributes_OnProDos)
    {
        Rig                rig;
        std::vector<Byte>  expected = rig.apple.drives[1];
        std::vector<Byte>  next;
        FileAttributes     attributes;

        attributes.type       = ProDosVolume::kTypeBinary;
        attributes.auxType    = 0x0803;
        attributes.access     = FilePayload::kAccessLocked;
        attributes.hasType    = true;
        attributes.hasAuxType = true;
        attributes.hasAccess  = true;

        AssertSucceeded (ProDosVolume (expected).SetAttributes (FilePath::Parse ("HELLO"), attributes, next));
        expected = next;

        rig.Start();

        AssertSucceeded (rig.Finish (rig.session->SetAttributes (2, FilePath::Parse ("HELLO"), attributes))->result);

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




    TEST_METHOD (APascalDisk_ListsReadsAndWrites)
    {
        Rig                rig;
        std::vector<Byte>  disk (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  expected;
        FilePayload        payload = Binary (PascalVolume::kKindData, 1500, 7);
        const char         name[]  = "LINKED";

        disk[ProDosSkeleton::GetBlockByteOffset (2, 2)]  = 6;
        disk[ProDosSkeleton::GetBlockByteOffset (2, 6)]  = (Byte) strlen (name);
        disk[ProDosSkeleton::GetBlockByteOffset (2, 14)] = 24;
        disk[ProDosSkeleton::GetBlockByteOffset (2, 15)] = 1;

        for (size_t i = 0; i < strlen (name); i++)
        {
            disk[ProDosSkeleton::GetBlockByteOffset (2, 7 + i)] = (Byte) name[i];
        }

        rig.apple.drives[0] = disk;
        rig.Start();

        auto  list = rig.Finish (rig.session->Enumerate (1, FilePath()));

        AssertSucceeded (list->result);
        Assert::IsTrue (list->volumeKind == VolumeKind::Pascal);
        Assert::AreEqual (std::string ("LINKED"), list->listing.volumeName);

        AssertSucceeded (PascalVolume (disk).Write (FilePath::Parse ("NEW.DATA"), payload, expected));
        AssertSucceeded (rig.Finish (rig.session->WriteFile (1, FilePath::Parse ("NEW.DATA"), payload))->result);
        Assert::IsTrue (rig.Drive (1) == expected);

        auto  read = rig.Finish (rig.session->ReadFile (1, FilePath::Parse ("NEW.DATA")));

        AssertSucceeded (read->result);
        Assert::IsTrue (read->file.bytes == payload.bytes);

        AssertSucceeded (rig.Finish (rig.session->Rename (1, FilePath::Parse ("NEW.DATA"), "OLD.DATA"))->result);
        AssertSucceeded (rig.Finish (rig.session->Delete (1, FilePath::Parse ("OLD.DATA")))->result);
        Assert::AreEqual ((size_t) 0, rig.Finish (rig.session->Enumerate (1, FilePath()))->listing.entries.size());
    }




    TEST_METHOD (ADamagedCpmDisk_NeedsAChoice_UntilAnotherDiskIsInTheDrive)
    {
        Rig                rig;
        std::vector<Byte>  disk (NibblizationLayer::kImageByteSize, CpmVolume::kFree);
        std::vector<Byte>  expected;
        FilePayload        payload = Binary (0, 2000, 3);

        //  One slot that isn't an entry, so detection doesn't find it empty.
        disk[CpmVolume::GetSectorOffset (1, 3) + 224] = 0x7F;

        rig.apple.drives[1] = disk;
        rig.Start();

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_UNRECOGNIZED_VOLUME), rig.Finish (rig.session->Enumerate (2, FilePath()))->result);

        rig.session->SetChosenKind (2, VolumeKind::Cpm);

        auto  list = rig.Finish (rig.session->Enumerate (2, FilePath()));

        AssertSucceeded (list->result);
        Assert::IsTrue (list->volumeKind == VolumeKind::Cpm);
        Assert::AreEqual ((uint32_t) 126, list->listing.freeUnits);

        AssertSucceeded (CpmVolume (disk).Write (FilePath::Parse ("A.COM"), payload, expected));
        AssertSucceeded (rig.Finish (rig.session->WriteFile (2, FilePath::Parse ("A.COM"), payload))->result);
        Assert::IsTrue (rig.Drive (2) == expected);

        auto  read = rig.Finish (rig.session->ReadFile (2, FilePath::Parse ("A.COM")));

        AssertSucceeded (read->result);
        Assert::IsTrue (std::equal (payload.bytes.begin(), payload.bytes.end(), read->file.bytes.begin()));

        {
            std::lock_guard<std::mutex>  guard (rig.apple.lock);

            rig.apple.drives[1] = BuildProDos();
        }

        auto  changed = rig.Finish (rig.session->Enumerate (2, FilePath()));

        AssertSucceeded (changed->result);
        Assert::IsTrue (changed->volumeKind == VolumeKind::ProDos);
        Assert::IsTrue (rig.session->GetChosenKind (2) == VolumeKind::Unknown, L"the choice was for the other disk");
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




    TEST_METHOD (DsrDropping_Disconnects_WhileTheLineIsOpen)
    {
        Rig  rig;

        rig.factory.peerReady = 1;
        rig.Start();
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready));

        std::this_thread::sleep_for (std::chrono::milliseconds (500));

        //  An Apple reset turns its DTR off at once.
        rig.factory.peerReady = 0;
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Disconnected, 2000), L"well before the silence counts");
    }




    TEST_METHOD (DsrOffForAMillisecondAtATime_KeepsTheLink)
    {
        Rig  rig;

        rig.factory.peerReady = 1;
        rig.Start();
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready));

        //  DSR glitches, as a USB serial adapter on a //c cable reports them.
        {
            std::jthread  glitches ([&rig] (std::stop_token stop)
            {
                while (!stop.stop_requested())
                {
                    rig.factory.peerReady = 0;
                    std::this_thread::sleep_for (std::chrono::milliseconds (1));
                    rig.factory.peerReady = 1;
                    std::this_thread::sleep_for (std::chrono::milliseconds (15));
                }
            });

            Assert::IsFalse (rig.sink.WaitForState (LinkState::Disconnected, 2000), L"a DSR glitch is not the Apple gone");
        }
    }



    TEST_METHOD (ReadDisk_WritesTheImage_InEachFormat)
    {
        Rig  rig;

        rig.Start();

        for (const char * name : { "out.dsk", "out.do", "out.po", "out.woz", "out.nib", "out.nb2" })
        {
            std::vector<Byte>   sectors;
            SectorDecodeReport  report;
            auto                read = rig.Finish (rig.session->ReadDisk (2, name, false));

            AssertSucceeded (read->result);
            AssertSucceeded (VolumeImage::Load (rig.fileIo.files[name], name, sectors, report));
            Assert::IsTrue (sectors == rig.Drive (2), std::format (L"{} has the disk", std::wstring (name, name + strlen (name))).c_str());
        }

        Assert::IsTrue (rig.fileIo.files["out.dsk"] == rig.Drive (2), L"a .dsk is the sectors in DOS 3.3 order");
        Assert::AreEqual (NibbleImageCodec::kNb2ImageSize, rig.fileIo.files["out.nb2"].size(), L"a .nb2 has .nb2's tracks");
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
        Assert::IsTrue (rig.fileIo.files.find ("strict.dsk.unreadable.txt") == rig.fileIo.files.end(), L"and no list");

        auto  blank = rig.Finish (rig.session->ReadDisk (1, "blank.dsk", true));

        AssertSucceeded (blank->result);

        expected = rig.Drive (1);

        for (const auto & [track, physical] : rig.apple.badSectors)
        {
            auto  at = expected.begin() + (track * 16 + CassoLinkSession::GetLogicalSector (physical)) * 256;

            std::fill (at, at + 256, (Byte) 0);
            Assert::IsTrue (std::find (blank->unreadable.begin(), blank->unreadable.end(), std::pair<int, int> { track, CassoLinkSession::GetLogicalSector (physical) }) != blank->unreadable.end());
        }

        Assert::IsTrue (rig.fileIo.files["blank.dsk"] == expected, L"the unreadable sectors as zeros, the rest as read");

        const std::vector<Byte> &  list = rig.fileIo.files["blank.dsk.unreadable.txt"];

        Assert::AreEqual (CassoLinkDiskTransfer::FormatUnreadableList (blank->unreadable), std::string (list.begin(), list.end()),
                          L"the sectors written as zeros, listed beside the image");
    }




    //  Each Read sectors request for `track`, in the order received.
    static std::vector<std::vector<Byte>>  GetTrackRequests (FakeApple & apple, int track)
    {
        std::lock_guard<std::mutex>     guard (apple.lock);
        std::vector<std::vector<Byte>>  requests;

        for (const auto & [read, sectors] : apple.readRequests)
        {
            if (read == track)
            {
                requests.push_back (sectors);
            }
        }

        return requests;
    }

    //  A track of drive 1 with every byte set, so a sector left as zeros differs.
    static void  FillTrack (Rig & rig, int track)
    {
        std::lock_guard<std::mutex>  guard (rig.apple.lock);
        auto                         at = rig.apple.drives[0].begin() + track * 16 * 256;

        for (int i = 0; i < 16 * 256; i++)
        {
            at[i] = (Byte) (i * 7 + 1);
        }
    }




    TEST_METHOD (ReadDisk_WithAPartialTrack_RequestsOnlyTheMissingSectorsAgain_InOneRequest)
    {
        Rig  rig;

        rig.apple.flakySectors = { { { 3, 5 }, 1 }, { { 3, 9 }, 1 } };
        FillTrack (rig, 3);
        rig.Start();
        rig.session->SetVerifySettings (false, false, false);

        auto  read     = rig.Finish (rig.session->ReadDisk (1, "partial.dsk", false));
        auto  requests = GetTrackRequests (rig.apple, 3);

        AssertSucceeded (read->result);
        Assert::IsTrue   (read->unreadable.empty());
        Assert::IsTrue   (rig.fileIo.files["partial.dsk"] == rig.Drive (1), L"the sectors of the partial read, and the two read again");
        Assert::AreEqual (size_t (2), requests.size());
        Assert::AreEqual (size_t (16), requests[0].size());
        Assert::IsTrue   (requests[1] == std::vector<Byte> { 5, 9 }, L"the retry is for only the sectors missing");
    }




    TEST_METHOD (ReadDisk_StopsRetrying_AfterARequestWithNoSectorRead)
    {
        Rig                rig;
        std::vector<Byte>  expected;
        int                logical = CassoLinkSession::GetLogicalSector (5);

        rig.apple.badSectors   = { { 3, 5 } };
        rig.apple.flakySectors = { { { 3, 9 }, 1 } };
        FillTrack (rig, 3);
        rig.Start();
        rig.session->SetVerifySettings (false, false, false);

        auto  read     = rig.Finish (rig.session->ReadDisk (1, "bad.dsk", true));
        auto  requests = GetTrackRequests (rig.apple, 3);

        AssertSucceeded (read->result);
        Assert::IsTrue (read->unreadable == std::vector<std::pair<int, int>> { { 3, logical } });
        Assert::IsTrue (requests == std::vector<std::vector<Byte>> { requests[0], { 5, 9 }, { 5 } }, L"9 read on the first retry; none on the second ends the retries");

        expected = rig.Drive (1);
        std::fill_n (expected.begin() + (3 * 16 + logical) * 256, 256, (Byte) 0);
        Assert::IsTrue (rig.fileIo.files["bad.dsk"] == expected);
    }




    TEST_METHOD (ReadDisk_RetriesAtMostKRetryPasses_WhileEachReadsASector)
    {
        Rig  rig;

        rig.apple.flakySectors = { { { 3, 1 }, 1 }, { { 3, 2 }, 2 }, { { 3, 3 }, 3 }, { { 3, 4 }, 4 } };
        FillTrack (rig, 3);
        rig.Start();
        rig.session->SetVerifySettings (false, false, false);

        auto  read     = rig.Finish (rig.session->ReadDisk (1, "slow.dsk", true));
        auto  requests = GetTrackRequests (rig.apple, 3);

        AssertSucceeded (read->result);
        Assert::AreEqual (size_t (1 + CassoLinkDiskTransfer::kRetryPasses), requests.size());
        Assert::IsTrue   (requests[1] == std::vector<Byte> { 1, 2, 3, 4 });
        Assert::IsTrue   (requests[3] == std::vector<Byte> { 3, 4 });
        Assert::IsTrue   (read->unreadable == std::vector<std::pair<int, int>> { { 3, CassoLinkSession::GetLogicalSector (4) } });
    }




    TEST_METHOD (VerifiedReadDisk_WithAPartialTrack_RequestsOnlyTheMissingSectorsAgain)
    {
        Rig  rig;

        rig.apple.flakySectors = { { { 3, 5 }, 1 }, { { 3, 9 }, 1 } };
        FillTrack (rig, 3);
        rig.Start();

        auto  read     = rig.Finish (rig.session->ReadDisk (1, "verified.dsk", false, true));
        auto  requests = GetTrackRequests (rig.apple, 3);

        AssertSucceeded (read->result);
        Assert::IsTrue (rig.fileIo.files["verified.dsk"] == rig.Drive (1));
        Assert::IsTrue (requests.size() >= 2);
        Assert::IsTrue (requests[1] == std::vector<Byte> { 5, 9 }, L"the verifier's data read retries the missing sectors together");
        Assert::IsTrue (std::none_of (requests.begin() + 1, requests.end(), [] (const std::vector<Byte> & r) { return r == std::vector<Byte> { 5 } || r == std::vector<Byte> { 9 }; }),
                        L"and never one sector at a time");
    }




    TEST_METHOD (VerifiedReadDisk_WithASectorReadOnce_ListsItAsUnreliable)
    {
        Rig  rig;

        rig.apple.onceSectors = { { 4, 2 } };
        rig.Start();

        auto  read = rig.Finish (rig.session->ReadDisk (1, "once.dsk", true, true));

        AssertSucceeded (read->result);
        Assert::IsTrue (read->unreadable.empty());
        Assert::IsTrue (rig.fileIo.files["once.dsk"] == rig.Drive (1), L"the image has the one good read");

        const std::vector<Byte> &  list = rig.fileIo.files["once.dsk.unreadable.txt"];

        Assert::AreEqual (CassoLinkDiskTransfer::FormatUnreadableList ({}, { SectorMark { 4, CassoLinkSession::GetLogicalSector (2), 'U' } }),
                          std::string (list.begin(), list.end()), L"the unreliable sector, listed beside the image");
    }




    TEST_METHOD (UnreadableList_IsATrackAndDos33SectorALine_AfterComments)
    {
        std::string  text = CassoLinkDiskTransfer::FormatUnreadableList ({ { 3, 7 }, { 20, 0 } });

        Assert::AreEqual (std::string ("blank.dsk.unreadable.txt"), CassoLinkDiskTransfer::GetUnreadableListPath ("blank.dsk"));
        Assert::AreEqual (std::string ("# Sectors that could not be read when this image was made, written as zeros.\r\n"
                                       "# Each line is a sector's track and DOS 3.3 sector, in decimal.\r\n"
                                       "3 7\r\n"
                                       "20 0\r\n"), text);
    }




    TEST_METHOD (ReadDisk_WithEverySectorRead_RemovesAnEarlierUnreadableList)
    {
        Rig  rig;

        rig.fileIo.files["again.dsk.unreadable.txt"] = { '1', ' ', '2' };
        rig.Start();

        auto  read = rig.Finish (rig.session->ReadDisk (1, "again.dsk", true));

        AssertSucceeded (read->result);
        Assert::IsTrue (read->unreadable.empty());
        Assert::IsTrue (rig.fileIo.files.find ("again.dsk.unreadable.txt") == rig.fileIo.files.end(), L"a list from an earlier read of the image is out of date");
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




    //  A sector of zeros is sent as its numbers alone, and written as zeros.
    TEST_METHOD (WriteDisk_SendsSectorsOfZerosAsBlanks)
    {
        Rig                rig;
        std::vector<Byte>  image  = BuildProDos();
        std::vector<Byte>  file;
        int                blanks = 0;

        VolumeImage::DosLogicalToProDosFile (image, file);
        rig.fileIo.files["in.po"] = file;

        for (size_t at = 0; at < image.size(); at += 256)
        {
            blanks += std::all_of (image.begin() + (ptrdiff_t) at, image.begin() + (ptrdiff_t) at + 256, [] (Byte b) { return b == 0; }) ? 1 : 0;
        }

        rig.Start();

        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);
        AssertSucceeded (rig.Finish (rig.session->WriteDisk (1, "in.po"))->result);

        Assert::IsTrue   (rig.Drive (1) == image);
        Assert::IsTrue   (blanks > 0, L"the image has sectors of zeros");
        Assert::AreEqual (blanks, (int) rig.apple.blanksReceived);
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




    //  A Full Send through the link, ended by Q at its pause: drive 1 and
    //  then drive 2, the screens on the Apple, and the screen cleared at the end.
    TEST_METHOD (VerifiedWrite_RewritesATrackUpToTwice)
    {
        Rig                rig;
        std::vector<Byte>  file;

        VolumeImage::DosLogicalToProDosFile (rig.Drive (1), file);
        rig.fileIo.files["in.po"] = file;
        rig.Start();
        rig.session->SetVerifySettings (false, true, true);

        rig.apple.writeFailures = 2;

        auto  recovered = rig.Finish (rig.session->WriteDisk (1, "in.po"));

        AssertSucceeded (recovered->result);
        Assert::AreEqual (37, rig.apple.trackWrites.load());
        Assert::IsTrue (recovered->message.find ("written again") != std::string::npos);

        rig.apple.writeFailures = 3;

        auto  failed = rig.Finish (rig.session->WriteDisk (1, "in.po"));

        Assert::IsTrue (FAILED (failed->result));
    }

    TEST_METHOD (Write_WithoutVerification_DoesNotRewrite)
    {
        Rig                rig;
        std::vector<Byte>  file;

        VolumeImage::DosLogicalToProDosFile (rig.Drive (1), file);
        rig.fileIo.files["in.po"] = file;
        rig.Start();

        rig.apple.writeFailures = 1;

        auto  failed = rig.Finish (rig.session->WriteDisk (1, "in.po"));

        Assert::IsTrue (FAILED (failed->result));
        Assert::AreEqual (1, rig.apple.trackWrites.load());
    }

    TEST_METHOD (VerifiedWrite_ComparesTheWholeDisk)
    {
        Rig                rig;
        std::vector<Byte>  image = rig.Drive (1);
        std::vector<Byte>  file;

        VolumeImage::DosLogicalToProDosFile (image, file);
        rig.fileIo.files["in.po"] = file;
        rig.Start();

        rig.session->SetVerifySettings (false, true, true);

        AssertSucceeded (rig.Finish (rig.session->WriteDisk (1, "in.po"))->result);

        //  A sector that no longer reads fails the comparison, with its track.
        rig.apple.badSectors.insert ({ 12, 3 });

        auto  write = rig.Finish (rig.session->WriteDisk (1, "in.po"));

        Assert::IsTrue (FAILED (write->result));
    }

    TEST_METHOD (FullSend_SendsBothDrives_AndEndsOnQAtThePause)
    {
        Rig  rig;

        rig.apple.keys.push_back (CassoLinkCodec::kKeySpace);
        rig.apple.keys.push_back (CassoLinkCodec::kKeyQuit);
        rig.Start();

        auto  send = rig.Finish (rig.session->FullSend ("C:\\Box", "", ""));

        Assert::IsTrue (send->state == LinkOperationState::Canceled);
        Assert::IsTrue (rig.fileIo.files["C:\\Box\\0001 HELLO.dsk"] == rig.Drive (1));
        Assert::IsTrue (rig.fileIo.files.contains ("C:\\Box\\0002 LINKED.po"));
        Assert::IsTrue (rig.fileIo.files.contains ("C:\\Box\\manifest.fullsend"));
        Assert::AreEqual (2, rig.apple.keyWaits.load(), L"the entry screen and the pause");
        Assert::IsTrue (send->message.find ("2 sent") != std::string::npos, L"the counts are in the result");

        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);

        std::lock_guard<std::mutex>  guard (rig.apple.lock);

        Assert::IsFalse (rig.apple.screens.empty());
        Assert::AreEqual<int> (0, rig.apple.screens.back()[0], L"the last screen clears the session");
        Assert::AreEqual<int> (0, rig.apple.screens.back()[1]);
    }




    TEST_METHOD (FullSend_FromTheAppleKeyboard_UsesTheSinksTarget)
    {
        Rig  rig;

        rig.sink.fullSendFolder = "C:\\Box";
        rig.apple.keys.push_back (CassoLinkCodec::kKeySpace);
        rig.apple.keys.push_back (CassoLinkCodec::kKeyQuit);
        rig.Start();

        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready));
        rig.apple.fullSendPending = true;

        for (int tries = 0; tries < 2000 && rig.apple.keyWaits == 0; tries++)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        std::shared_ptr<LinkOperation>  send;

        {
            std::lock_guard<std::mutex>  guard (rig.sink.lock);

            Assert::AreEqual ((size_t) 1, rig.sink.queued.size());
            send = rig.sink.queued[0];
        }

        rig.Finish (send);

        Assert::IsTrue (rig.fileIo.files.contains ("C:\\Box\\0001 HELLO.do"));
    }




    TEST_METHOD (FullSend_FromTheAppleKeyboard_WithNoFolder_StartsNothing)
    {
        Rig  rig;

        rig.Start();

        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready));
        rig.apple.fullSendPending = true;

        AssertSucceeded (rig.Finish (rig.session->Enumerate (1, FilePath()))->result);
        std::this_thread::sleep_for (std::chrono::milliseconds (50));

        std::lock_guard<std::mutex>  guard (rig.sink.lock);

        Assert::IsTrue (rig.sink.queued.empty());
        Assert::AreEqual (0, rig.apple.keyWaits.load());
    }




    TEST_METHOD (FullSend_CancelDuringThePause_EndsTheSession)
    {
        Rig  rig;

        rig.apple.drives[1].clear();
        rig.Start();

        auto  send = rig.session->FullSend ("C:\\Box", "", "");

        for (int tries = 0; tries < 2000 && rig.apple.keyWaits == 0; tries++)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        Assert::AreEqual (1, rig.apple.keyWaits.load(), L"an empty drive 2 and a sent drive 1 make a pause");

        rig.session->Cancel();
        rig.Finish (send);

        Assert::IsTrue (send->state == LinkOperationState::Canceled);
        Assert::AreEqual (1, rig.apple.cancels.load());
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready), L"the link goes on");
    }




    TEST_METHOD (FullSend_QDuringARead_WritesNoImage)
    {
        //  Space begins the session.
        Rig  rig;

        rig.apple.sectorDelayMs = 1;
        rig.Start();

        rig.apple.keys.push_back (CassoLinkCodec::kKeySpace);

        auto  send = rig.session->FullSend ("C:\\Box", "", "");

        for (int tries = 0; tries < 2000 && rig.apple.sectorsRead < 100; tries++)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        rig.apple.quitPending = true;
        rig.Finish (send);

        Assert::IsTrue (send->state == LinkOperationState::Canceled);
        Assert::IsTrue (rig.fileIo.files.empty(), L"no image, no manifest");
        Assert::IsTrue (send->message.find ("Q was") != std::string::npos);
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
        Assert::AreEqual (std::string ("RENAMING GAMES/OLD TO NEW"), CassoLinkSession::DescribeOperation (operation));

        operation.kind = LinkOperationKind::ReadDisk;
        Assert::AreEqual (std::string ("SENDING D2 TO PC"), CassoLinkSession::DescribeOperation (operation));

        listing.hasVolumeName = true;
        listing.volumeName    = "GAMES";
        Assert::AreEqual (std::string ("SENDING D2 (/GAMES) TO PC"), CassoLinkSession::DescribeOperation (operation, listing));

        listing.hasVolumeName   = false;
        listing.hasVolumeNumber = true;
        listing.volumeNumber    = 254;
        operation.kind          = LinkOperationKind::WriteDisk;
        operation.imagePath     = "C:\\Disks\\GAMES.DSK";
        Assert::AreEqual (std::string ("WRITING IMAGE GAMES.DSK TO D2"), CassoLinkSession::DescribeOperation (operation, listing), L"not the volume the write replaces");
    }



    //  A drive's speed in rpm and how far it is from 300, with a note past the
    //  usual 1.5%.
    TEST_METHOD (DescribeDriveSpeed_GivesRpmAndTheDeviation)
    {
        Assert::AreEqual (std::string ("D1 turns at 293.1 rpm, 2.3% slow; drives are usually set within 1.5% of 300 rpm"),
                          CassoLinkDiskTransfer::DescribeDriveSpeed (1, 204.68));
        Assert::AreEqual (std::string ("D2 turns at 301.5 rpm, 0.5% fast"), CassoLinkDiskTransfer::DescribeDriveSpeed (2, 199.0));
        Assert::AreEqual (std::string ("D1 turns at 300.0 rpm"), CassoLinkDiskTransfer::DescribeDriveSpeed (1, 200.0));
    }



    //  The history line of an operation that succeeded is in the past tense,
    //  and a Write disk made for a format has the new volume in place of the
    //  image's name.
    TEST_METHOD (DescribeOperation_Finished_IsInThePastTense)
    {
        LinkOperation  operation;

        operation.kind      = LinkOperationKind::WriteDisk;
        operation.drive     = 1;
        operation.imagePath = "C:\\Temp\\casso-format-1.dsk";
        operation.formatted = "VOL 254";
        Assert::AreEqual (std::string ("FORMATTING VOL 254 IN D1"), CassoLinkSession::DescribeOperation (operation));
        Assert::AreEqual (std::string ("FORMATTED VOL 254 IN D1"), CassoLinkSession::DescribeOperation (operation, std::nullopt, true));

        operation.formatted.clear();
        operation.imagePath = "C:\\Disks\\GAMES.DSK";
        Assert::AreEqual (std::string ("WROTE IMAGE GAMES.DSK TO D1"), CassoLinkSession::DescribeOperation (operation, std::nullopt, true));

        operation.kind = LinkOperationKind::ReadDisk;
        Assert::AreEqual (std::string ("SENT D1 TO PC"), CassoLinkSession::DescribeOperation (operation, std::nullopt, true));

        operation.kind = LinkOperationKind::Krunch;
        Assert::AreEqual (std::string ("KRUNCHING D1"), CassoLinkSession::DescribeOperation (operation));
        Assert::AreEqual (std::string ("KRUNCHED D1"),  CassoLinkSession::DescribeOperation (operation, std::nullopt, true));
    }



    //  A krunch that the program stops because the directory isn't the one
    //  the host checked is ERROR_MEDIA_CHANGED.
    TEST_METHOD (FromStatusError_ADirectoryChanged_IsTheMediaChanged)
    {
        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_MEDIA_CHANGED), CassoLinkSession::FromStatusError (CassoLinkHost::kErrorChanged));
    }



    //  The Apple comes up on the line that is open already: the line stays
    //  open between attempts, so its Ready is read there.
    TEST_METHOD (ALinkStartedBeforeItsApple_ConnectsOnItsOwn_WhenTheAppleAnswers)
    {
        Rig  rig;

        rig.factory.answer = false;
        rig.session->SetRetryInterval (100);
        rig.Start (300);

        Assert::IsTrue (rig.sink.WaitForState (LinkState::Disconnected), L"no Apple yet");

        rig.apple.Attach (rig.factory.GetUnansweredEnd());
        Assert::IsTrue (rig.sink.WaitForState (LinkState::Ready), L"connected with no request made");
        Assert::AreEqual (1, rig.factory.connections.load(), L"on the line opened first");
    }
};
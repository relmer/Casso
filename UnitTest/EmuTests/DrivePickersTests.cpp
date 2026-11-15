#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Picker/FileMaskSet.h"
#include "Devices/Disk/ImageClassifier.h"
#include "Shell/DrivePickers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DrivePickersTests
//
//  The emulator's drive pickers over a factory that doesn't open a window: one
//  picker for each drive, the confirm's inspector question and insert, the
//  door calls, and the request made for each kind of drive.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DrivePickersTests)
{
public:

    struct FakePicker : IDrivePicker
    {
        PickerRequest          request;
        bool                   cascade  = false;
        int                    fronted  = 0;
        bool                 * isAlive  = nullptr;
        std::function<void()> onClosed;

        ~FakePicker() override
        {
            if (isAlive != nullptr)
            {
                *isAlive = false;
            }
        }

        HWND  GetHwnd() const override      { return nullptr; }
        void  BringToFront() override       { fronted++; }
        void  Close() override              { End(); }

        void  End()
        {
            std::function<void()>  closed = std::move (onClosed);

            onClosed = nullptr;

            if (closed)
            {
                closed();
            }
        }

        //  The Insert button: the picker ends when the request takes the
        //  image, as ExplorerPickerMode::ConfirmPath does.
        bool  Confirm (const std::wstring & path)
        {
            bool  isTaken = request.onConfirm (path);

            if (isTaken)
            {
                End();
            }

            return isTaken;
        }
    };



    struct FakeFactory : IDrivePickerFactory
    {
        std::vector<FakePicker *>  opened;
        bool                       fails = false;

        std::unique_ptr<IDrivePicker> OpenPicker (PickerRequest request, bool cascade, std::function<void()> onClosed) override
        {
            std::unique_ptr<FakePicker>  picker;

            if (fails)
            {
                return nullptr;
            }

            picker           = std::make_unique<FakePicker>();
            picker->request  = std::move (request);
            picker->cascade  = cascade;
            picker->onClosed = std::move (onClosed);

            opened.push_back (picker.get());

            return picker;
        }
    };



    struct Rig
    {
        FakeFactory                    factory;
        std::vector<std::wstring>      log;
        bool                           discardAnswer = true;
        bool                           insertResult  = true;
        std::wstring                   insertError;
        std::unique_ptr<DrivePickers>  pickers;

        Rig()
        {
            DrivePickers::Hooks  hooks;



            hooks.confirmDiscard = [this] (const DriveAddress & address)
            {
                log.push_back (std::format (L"confirm {},{}", address.slot, address.drive));
                return discardAnswer;
            };

            hooks.insert = [this] (const PickerDrive & drive, const std::wstring & path, std::wstring & outError)
            {
                log.push_back (std::format (L"insert {},{} {}", drive.address.slot, drive.address.drive, path));
                outError = insertError;
                return insertResult;
            };

            hooks.reportFailure = [this] (const std::wstring & text)                  { log.push_back (L"report " + text); };
            hooks.onOpened      = [this] (const DriveAddress & address)               { log.push_back (std::format (L"opened {},{}", address.slot, address.drive)); };
            hooks.onClosed      = [this] (const DriveAddress & address, bool inserted) { log.push_back (std::format (L"closed {},{} {}", address.slot, address.drive, inserted ? L"inserted" : L"not inserted")); };

            pickers = std::make_unique<DrivePickers> (factory, std::move (hooks));
        }

        bool  Logged (const std::wstring & line) const
        {
            return std::find (log.begin(), log.end(), line) != log.end();
        }
    };



    static PickerDrive MakeDrive (int slot, int drive, ReceivingDriveKind kind, const wchar_t * label)
    {
        PickerDrive  made;



        made.address        = DriveAddress { slot, drive, kind == ReceivingDriveKind::Recorder };
        made.receiving.kind = kind;
        made.label          = label;

        return made;
    }



    TEST_METHOD (OpeningADriveTwice_FrontsItsOnePicker)
    {
        Rig          rig;
        PickerDrive  drive = MakeDrive (6, 0, ReceivingDriveKind::Floppy525, L"Drive 1 - Disk ][");



        rig.pickers->Open (drive);
        rig.pickers->Open (drive);

        Assert::AreEqual ((size_t) 1, rig.factory.opened.size());
        Assert::AreEqual (1, rig.factory.opened[0]->fronted);
        Assert::AreEqual ((size_t) 1, rig.pickers->GetOpenCount());
    }



    TEST_METHOD (OpeningAnotherDrive_OpensAnotherPicker_AndTheFirstKeepsItsDrive)
    {
        Rig          rig;
        PickerDrive  first  = MakeDrive (6, 0, ReceivingDriveKind::Floppy525, L"Drive 1 - Disk ][");
        PickerDrive  second = MakeDrive (6, 1, ReceivingDriveKind::Floppy525, L"Drive 2 - Disk ][");



        rig.pickers->Open (first);
        rig.pickers->Open (second);

        Assert::AreEqual ((size_t) 2, rig.factory.opened.size());
        Assert::AreEqual (0, rig.factory.opened[0]->fronted);
        Assert::IsFalse (rig.factory.opened[0]->cascade);
        Assert::IsTrue  (rig.factory.opened[1]->cascade, L"beside the first");

        //  A confirm in the first picker inserts in the first drive.
        Assert::IsTrue (rig.factory.opened[0]->Confirm (L"C:\\Disks\\a.dsk"));
        Assert::IsTrue (rig.Logged (L"insert 6,0 C:\\Disks\\a.dsk"));
        Assert::IsFalse (rig.pickers->IsOpenFor (first.address));
        Assert::IsTrue  (rig.pickers->IsOpenFor (second.address));
    }



    TEST_METHOD (Confirm_AsksAboutInspectorEdits_ThenInserts_AndClosesThePicker)
    {
        Rig          rig;
        PickerDrive  drive = MakeDrive (6, 0, ReceivingDriveKind::Floppy525, L"Drive 1 - Disk ][");



        rig.pickers->Open (drive);
        Assert::IsTrue (rig.factory.opened[0]->Confirm (L"C:\\Disks\\a.dsk"));

        Assert::IsTrue (rig.log == std::vector<std::wstring> { L"opened 6,0", L"confirm 6,0", L"insert 6,0 C:\\Disks\\a.dsk", L"closed 6,0 inserted" });
        Assert::AreEqual ((size_t) 0, rig.pickers->GetOpenCount());
    }



    TEST_METHOD (NoToTheInspectorQuestion_InsertsNothing_AndThePickerStaysOpen)
    {
        Rig          rig;
        PickerDrive  drive = MakeDrive (6, 0, ReceivingDriveKind::Floppy525, L"Drive 1 - Disk ][");



        rig.discardAnswer = false;

        rig.pickers->Open (drive);
        Assert::IsFalse (rig.factory.opened[0]->Confirm (L"C:\\Disks\\a.dsk"));

        Assert::IsTrue (rig.log == std::vector<std::wstring> { L"opened 6,0", L"confirm 6,0" });
        Assert::IsTrue (rig.pickers->IsOpenFor (drive.address));
    }



    TEST_METHOD (FailedInsert_IsReported_AndThePickerStaysOpen)
    {
        Rig          rig;
        PickerDrive  drive = MakeDrive (5, 0, ReceivingDriveKind::Apple35, L"Apple 3.5 Drive - S5,D1");



        rig.insertResult = false;
        rig.insertError  = L"The image could not be read.";

        rig.pickers->Open (drive);
        Assert::IsFalse (rig.factory.opened[0]->Confirm (L"C:\\Disks\\a.po"));

        Assert::IsTrue (rig.Logged (L"report The image could not be read."));
        Assert::IsFalse (rig.Logged (L"closed 5,0 inserted"));
        Assert::IsTrue (rig.pickers->IsOpenFor (drive.address));
    }



    TEST_METHOD (Close_AndCloseAll_InsertNothing)
    {
        Rig          rig;
        PickerDrive  first  = MakeDrive (6, 0, ReceivingDriveKind::Floppy525, L"Drive 1 - Disk ][");
        PickerDrive  second = MakeDrive (5, 0, ReceivingDriveKind::Apple35,   L"Apple 3.5 Drive - S5,D1");
        PickerDrive  tape   = MakeDrive (0, 0, ReceivingDriveKind::Recorder,  L"Cassette recorder");



        rig.pickers->Open (first);
        rig.pickers->Open (second);
        rig.pickers->Open (tape);

        rig.pickers->Close (second.address);
        Assert::IsTrue (rig.Logged (L"closed 5,0 not inserted"));
        Assert::AreEqual ((size_t) 2, rig.pickers->GetOpenCount());

        rig.pickers->CloseAll();
        Assert::AreEqual ((size_t) 0, rig.pickers->GetOpenCount());
        Assert::IsTrue (rig.Logged (L"closed 6,0 not inserted"));
        Assert::IsTrue (rig.Logged (L"closed 0,0 not inserted"));

        for (const std::wstring & line : rig.log)
        {
            Assert::IsFalse (line.starts_with (L"insert"), line.c_str());
        }
    }



    TEST_METHOD (ClosedPicker_IsReleasedBetweenMessages_AndTheDriveOpensANewOne)
    {
        Rig          rig;
        PickerDrive  drive = MakeDrive (6, 0, ReceivingDriveKind::Floppy525, L"Drive 1 - Disk ][");
        bool         alive = true;



        rig.pickers->Open (drive);
        rig.factory.opened[0]->isAlive = &alive;
        rig.pickers->Close (drive.address);

        //  The close ran inside a message of the picker's window, so the picker is
        //  released only by the loop's call.
        Assert::IsTrue (alive);
        rig.pickers->ReleaseClosed();
        Assert::IsFalse (alive);

        rig.pickers->Open (drive);
        Assert::AreEqual ((size_t) 2, rig.factory.opened.size());
        Assert::IsTrue (rig.pickers->IsOpenFor (drive.address));
    }



    TEST_METHOD (PickerThatCannotOpen_PutsTheDoorBack)
    {
        Rig          rig;
        PickerDrive  drive = MakeDrive (6, 0, ReceivingDriveKind::Floppy525, L"Drive 1 - Disk ][");



        rig.factory.fails = true;
        rig.pickers->Open (drive);

        Assert::IsTrue (rig.log == std::vector<std::wstring> { L"opened 6,0", L"closed 6,0 not inserted" });
        Assert::IsFalse (rig.pickers->IsOpenFor (drive.address));
    }



    TEST_METHOD (Request_HasTheDrive_ItsViewKey_AndFilterEntriesForItsMedia)
    {
        PickerDrive    hard    = MakeDrive (7, 0, ReceivingDriveKind::HardDisk, L"ProFile - S7,D1");
        PickerDrive    tape    = MakeDrive (0, 0, ReceivingDriveKind::Recorder, L"Cassette recorder");
        PickerRequest  request;



        hard.receiving.maxBlocks = 9728;
        request                  = DrivePickers::MakeRequest (hard);

        Assert::IsTrue (request.form == PickerForm::DriveWindow);
        Assert::AreEqual (std::wstring (L"Insert disk - ProFile - S7,D1"), request.title);
        Assert::AreEqual (std::wstring (L"HardDisk"), request.viewKey);
        Assert::IsTrue (request.receivingDrive.has_value());
        Assert::IsTrue (request.receivingDrive->kind == ReceivingDriveKind::HardDisk);
        Assert::AreEqual ((uint32_t) 9728, request.receivingDrive->maxBlocks);
        Assert::AreEqual ((size_t) 2, request.filters.size());
        Assert::AreEqual (std::wstring (L"*.*"), request.filters[1].pattern);

        for (const wchar_t * name : { L"a.dsk", L"a.do", L"a.po", L"a.nib", L"a.woz", L"a.2mg", L"a.hdv", L"a.dc42" })
        {
            Assert::IsTrue (FileMaskSet (request.filters[0].pattern).Matches (name), name);
        }

        Assert::IsFalse (FileMaskSet (request.filters[0].pattern).Matches (L"a.wav"));

        request = DrivePickers::MakeRequest (tape);

        Assert::AreEqual (std::wstring (L"Insert tape - Cassette recorder"), request.title);
        Assert::AreEqual (std::wstring (L"Recorder"), request.viewKey);
        Assert::IsTrue  (FileMaskSet (request.filters[0].pattern).Matches (L"a.wav"));
        Assert::IsFalse (FileMaskSet (request.filters[0].pattern).Matches (L"a.dsk"));
    }



    //  Each kind of drive has a separate saved view.
    TEST_METHOD (ViewKeys_DifferForEachKindOfDrive)
    {
        std::set<std::wstring>  keys;



        for (ReceivingDriveKind kind : { ReceivingDriveKind::Floppy525, ReceivingDriveKind::Apple35, ReceivingDriveKind::UniDisk35, ReceivingDriveKind::HardDisk, ReceivingDriveKind::Recorder })
        {
            Assert::IsFalse (DrivePickers::GetViewKey (kind).empty());
            keys.insert (DrivePickers::GetViewKey (kind));
        }

        Assert::AreEqual ((size_t) 5, keys.size());
    }
};

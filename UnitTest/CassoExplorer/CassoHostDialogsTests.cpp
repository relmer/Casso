#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Seams/CassoHostDialogs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoHostDialogsTests
//
//  The dialog decorator over fake inner dialogs and a fake picker runner:
//  which requests go to the picker, which to the inner dialogs, and what a
//  picker's result becomes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoHostDialogsTests)
{
public:

    struct FakeInner : IHostDialogs
    {
        std::vector<std::wstring>  calls;

        HRESULT PickFileToOpen (HWND, const FileDialogSpec &, std::filesystem::path & outPath, bool & outPicked) override
        {
            calls.push_back (L"open");
            outPath   = L"C:\\inner\\open.txt";
            outPicked = true;
            return S_OK;
        }

        HRESULT PickFileToSave (HWND, const FileDialogSpec &, std::filesystem::path & outPath, bool & outPicked) override
        {
            calls.push_back (L"save");
            outPath   = L"C:\\inner\\save.txt";
            outPicked = true;
            return S_OK;
        }

        HRESULT PickFolder (HWND, std::filesystem::path & outFolder, bool & outPicked) override
        {
            calls.push_back (L"folder");
            outFolder = L"C:\\inner";
            outPicked = true;
            return S_OK;
        }
    };



    struct Rig
    {
        FakeInner                    inner;
        std::vector<PickerRequest>   requests;
        PickerOutcome                answer;
        HRESULT                      runnerResult = S_OK;
        CassoHostDialogs             dialogs { inner, [this] (HWND, PickerRequest request, PickerOutcome & outOutcome)
                                               {
                                                   requests.push_back (std::move (request));
                                                   outOutcome = answer;
                                                   return runnerResult;
                                               } };
    };



    TEST_METHOD (ImageOpenRequest_GoesToThePicker_AndReturnsItsPath)
    {
        Rig                    rig;
        FileDialogSpec         spec;
        std::filesystem::path  path;
        bool                   picked = false;



        spec.content            = FileDialogContent::Images;
        spec.title              = L"Write image to Drive 1";
        spec.filters            = { { L"Disk images", L"*.dsk;*.po" } };
        spec.receivingDrive     = ReceivingDrive { ReceivingDriveKind::Floppy525, 0 };
        rig.answer.isConfirmed  = true;
        rig.answer.isClosed     = true;
        rig.answer.path         = L"C:\\Disks\\a.dsk";

        AssertSucceeded (rig.dialogs.PickFileToOpen (nullptr, spec, path, picked));

        Assert::IsTrue (picked);
        Assert::AreEqual (std::wstring (L"C:\\Disks\\a.dsk"), path.wstring());
        Assert::IsTrue (rig.inner.calls.empty());
        Assert::AreEqual ((size_t) 1, rig.requests.size());
        Assert::IsTrue (rig.requests[0].form == PickerForm::ModalOpen);
        Assert::AreEqual (std::wstring (L"Write image to Drive 1"), rig.requests[0].title);
        Assert::IsTrue (rig.requests[0].receivingDrive.has_value());
    }



    TEST_METHOD (CanceledPicker_IsASuccessWithNothingPicked)
    {
        Rig                    rig;
        FileDialogSpec         spec;
        std::filesystem::path  path   = L"C:\\stale";
        bool                   picked = true;



        spec.content        = FileDialogContent::Images;
        rig.answer.isClosed = true;

        AssertSucceeded (rig.dialogs.PickFileToOpen (nullptr, spec, path, picked));

        Assert::IsFalse (picked);
        Assert::IsTrue (path.empty());
    }



    TEST_METHOD (OtherRequests_AndFolders_GoToTheInnerDialogs)
    {
        Rig                    rig;
        FileDialogSpec         spec;
        std::filesystem::path  path;
        bool                   picked = false;



        AssertSucceeded (rig.dialogs.PickFileToOpen (nullptr, spec, path, picked));
        AssertSucceeded (rig.dialogs.PickFileToSave (nullptr, spec, path, picked));
        AssertSucceeded (rig.dialogs.PickFolder (nullptr, path, picked));

        Assert::IsTrue (rig.inner.calls == std::vector<std::wstring> { L"open", L"save", L"folder" });
        Assert::IsTrue (rig.requests.empty());
    }



    TEST_METHOD (ImageSaveRequest_GoesToTheSaveForm_WithItsNameAndExtension)
    {
        Rig                    rig;
        FileDialogSpec         spec;
        std::filesystem::path  path;
        bool                   picked = false;



        spec.content            = FileDialogContent::Images;
        spec.defaultFileName    = L"New tape.wav";
        spec.defaultExtension   = L"wav";
        spec.askToReplace       = false;
        rig.answer.isConfirmed  = true;
        rig.answer.path         = L"C:\\Tapes\\New tape.wav";

        AssertSucceeded (rig.dialogs.PickFileToSave (nullptr, spec, path, picked));

        Assert::IsTrue (picked);
        Assert::IsTrue (rig.inner.calls.empty());
        Assert::IsTrue (rig.requests[0].form == PickerForm::ModalSave);
        Assert::AreEqual (std::wstring (L"New tape.wav"), rig.requests[0].defaultFileName);
        Assert::AreEqual (std::wstring (L"wav"), rig.requests[0].defaultExtension);
        Assert::IsFalse (rig.requests[0].askToReplace);
        Assert::AreEqual (std::wstring (CassoHostDialogs::kSaveTitle), rig.requests[0].title);
    }



    //  A picker that cannot be created is a failure, and Windows' dialog
    //  does not open in its place.
    TEST_METHOD (PickerThatFails_IsAFailure_AndTheInnerDialogsAreNotCalled)
    {
        Rig                    rig;
        FileDialogSpec         spec;
        std::filesystem::path  path;
        bool                   picked = false;
        HRESULT                hr     = S_OK;



        spec.content     = FileDialogContent::Images;
        rig.runnerResult = E_FAIL;

        hr = rig.dialogs.PickFileToOpen (nullptr, spec, path, picked);

        Assert::IsTrue (FAILED (hr));
        Assert::IsFalse (picked);
        Assert::IsTrue (rig.inner.calls.empty());
    }



    TEST_METHOD (RequestWithoutFilters_GetsAllFiles_AndAViewForItsDriveKind)
    {
        FileDialogSpec  spec;
        PickerRequest   plain   = CassoHostDialogs::MakeRequest (spec, false);
        PickerRequest   forDisk;
        PickerRequest   forSave;



        spec.receivingDrive = ReceivingDrive { ReceivingDriveKind::HardDisk, 9728 };
        forDisk             = CassoHostDialogs::MakeRequest (spec, false);
        forSave             = CassoHostDialogs::MakeRequest (spec, true);

        Assert::AreEqual ((size_t) 1, plain.filters.size());
        Assert::AreEqual (std::wstring (L"*.*"), plain.filters[0].pattern);
        Assert::AreEqual (std::wstring (CassoHostDialogs::kOpenTitle), plain.title);
        Assert::AreNotEqual (plain.viewKey, forDisk.viewKey);
        Assert::AreNotEqual (forDisk.viewKey, forSave.viewKey);
    }
};

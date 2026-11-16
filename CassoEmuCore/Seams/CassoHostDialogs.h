#pragma once

#include "Pch.h"

#include "CassoExplorer/Picker/PickerRequest.h"
#include "Seams/IHostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoHostDialogs
//
//  The file dialogs of a process that has the Casso Explorer picker. A
//  request for a disk or tape image (FileDialogContent::Images) opens a
//  modal picker, in its open form or its save form; every other request,
//  and every folder request, goes to the inner dialogs.
//
//  The runner opens a modal picker over the owner and returns once the
//  picker has ended. A picker that cannot be created is a failure, and the
//  inner dialogs are not called in its place.
//
////////////////////////////////////////////////////////////////////////////////

class CassoHostDialogs : public IHostDialogs
{
public:
    using PickerRunner = std::function<HRESULT (HWND owner, PickerRequest request, PickerOutcome & outOutcome)>;

    CassoHostDialogs (IHostDialogs & inner, PickerRunner runner);
    CassoHostDialogs (std::unique_ptr<IHostDialogs> inner, PickerRunner runner);

    HRESULT  PickFileToOpen (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath, bool & outPicked) override;
    HRESULT  PickFileToSave (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath, bool & outPicked) override;
    HRESULT  PickFolder     (HWND owner, std::filesystem::path & outFolder, bool & outPicked) override;

    //  The picker request for a file request. One with no filter entries
    //  gets an "All files" entry.
    static PickerRequest  MakeRequest (const FileDialogSpec & spec, bool isSave);

    static constexpr const wchar_t *  kOpenTitle = L"Open";
    static constexpr const wchar_t *  kSaveTitle = L"Save as";

private:
    HRESULT  RunPicker (HWND owner, const FileDialogSpec & spec, bool isSave, std::filesystem::path & outPath, bool & outPicked);

    std::unique_ptr<IHostDialogs>    m_owned;
    IHostDialogs                   & m_inner;
    PickerRunner                     m_runner;
};

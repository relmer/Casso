#include "Pch.h"

#include "Seams/CassoHostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoHostDialogs::CassoHostDialogs
//
////////////////////////////////////////////////////////////////////////////////

CassoHostDialogs::CassoHostDialogs (IHostDialogs & inner, PickerRunner runner)
    : m_inner  (inner),
      m_runner (std::move (runner))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoHostDialogs::CassoHostDialogs
//
////////////////////////////////////////////////////////////////////////////////

CassoHostDialogs::CassoHostDialogs (std::unique_ptr<IHostDialogs> inner, PickerRunner runner)
    : m_owned  (std::move (inner)),
      m_inner  (*m_owned),
      m_runner (std::move (runner))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoHostDialogs::MakeRequest
//
//  A request for one kind of drive has that kind's saved view, and the
//  open and the save form have one each for requests without a drive.
//
////////////////////////////////////////////////////////////////////////////////

PickerRequest CassoHostDialogs::MakeRequest (const FileDialogSpec & spec, bool isSave)
{
    PickerRequest  request;



    request.form             = isSave ? PickerForm::ModalSave : PickerForm::ModalOpen;
    request.title            = !spec.title.empty() ? spec.title : std::wstring (isSave ? kSaveTitle : kOpenTitle);
    request.initialFolder    = spec.initialFolder;
    request.filters          = spec.filters;
    request.receivingDrive   = spec.receivingDrive;
    request.driveLabel       = spec.driveLabel;
    request.defaultFileName  = spec.defaultFileName;
    request.defaultExtension = spec.defaultExtension;
    request.askToReplace     = spec.askToReplace;
    request.viewKey          = isSave ? L"Save" : L"Open";

    if (spec.receivingDrive.has_value())
    {
        request.viewKey += std::format (L"-{}", (int) spec.receivingDrive->kind);
    }

    if (request.filters.empty())
    {
        request.filters.push_back (FileDialogFilter { L"All files", L"*.*" });
    }

    return request;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoHostDialogs::RunPicker
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoHostDialogs::RunPicker (HWND owner, const FileDialogSpec & spec, bool isSave, std::filesystem::path & outPath, bool & outPicked)
{
    HRESULT        hr = S_OK;
    PickerOutcome  outcome;



    outPath.clear();
    outPicked = false;

    CBREx (m_runner != nullptr, E_NOTIMPL);

    hr = m_runner (owner, MakeRequest (spec, isSave), outcome);
    CHR (hr);

    if (outcome.isConfirmed && !outcome.path.empty())
    {
        outPath   = outcome.path;
        outPicked = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoHostDialogs::PickFileToOpen
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoHostDialogs::PickFileToOpen (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath, bool & outPicked)
{
    if (spec.content != FileDialogContent::Images)
    {
        return m_inner.PickFileToOpen (owner, spec, outPath, outPicked);
    }

    return RunPicker (owner, spec, false, outPath, outPicked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoHostDialogs::PickFileToSave
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoHostDialogs::PickFileToSave (HWND owner, const FileDialogSpec & spec, std::filesystem::path & outPath, bool & outPicked)
{
    if (spec.content != FileDialogContent::Images)
    {
        return m_inner.PickFileToSave (owner, spec, outPath, outPicked);
    }

    return RunPicker (owner, spec, true, outPath, outPicked);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoHostDialogs::PickFolder
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoHostDialogs::PickFolder (HWND owner, std::filesystem::path & outFolder, bool & outPicked)
{
    return m_inner.PickFolder (owner, outFolder, outPicked);
}

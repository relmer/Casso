#include "Pch.h"

#include "CassoExplorer/Window/Managers/CassoLinkDriveManager.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "CassoLink/Session/CassoLinkBlockTransfer.h"
#include "Core/TextEncoding.h"
#include "Seams/IHostDialogs.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::ReadBlockDeviceToImage
//
//  A block device's blocks into a .po, .hdv or .2mg the user picks. A read of
//  the device into that file that the link cut short is offered to continue.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::ReadBlockDeviceToImage (const Location & drive, const std::wstring & link, int number, bool forceVerify)
{
    HRESULT                hr     = S_OK;
    std::filesystem::path  picked;
    bool                   chosen = false;
    bool                   resume = false;
    CassoLinkBlockDevice   device;
    FileDialogSpec         spec;
    std::wstring           title  = m_context.browser.GetTreeModel().GetLinkDriveTitle (drive.path);



    BAIL_OUT_IF (!TryGetBlockDevice (link, number, device), S_OK);

    spec.filters          = { FileDialogFilter { L"Block images (*.po, *.hdv, *.2mg)", L"*.po;*.hdv;*.2mg" },
                              FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid       = s_kLinkDiskPickerGuid;
    spec.defaultFileName  = device.volume.empty() ? std::format (L"Slot {} unit {}.po", device.slot, device.unit) : TextEncoding::NarrowToWide (device.volume) + L".po";
    spec.defaultExtension = L"po";
    spec.title            = std::format (L"Create image from {}", title);
    spec.content          = FileDialogContent::Images;

    hr = m_context.dialogs.PickFileToSave (m_context.window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    BAIL_OUT_IF (!AskToResume (title, device, picked, true, resume), S_OK);

    SubmitBlockTransfer (link, number, picked, true, resume, forceVerify);

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::WriteImageToBlockDevice
//
//  An image onto a block device, once the user confirms, since everything on
//  the device is replaced. A write of that image that the link cut short is
//  offered to continue.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::WriteImageToBlockDevice (const Location & drive, const std::wstring & link, int number, bool forceVerify)
{
    HRESULT                hr     = S_OK;
    std::filesystem::path  picked;
    bool                   chosen = false;
    bool                   resume = false;
    CassoLinkBlockDevice   device;
    FileDialogSpec         spec;
    std::wstring           title  = m_context.browser.GetTreeModel().GetLinkDriveTitle (drive.path);
    std::wstring           text;



    BAIL_OUT_IF (!TryGetBlockDevice (link, number, device), S_OK);

    spec.filters = { FileDialogFilter { L"Block images (*.po, *.hdv, *.2mg)", L"*.po;*.hdv;*.2mg" },
                     FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid = s_kLinkDiskPickerGuid;
    spec.title      = std::format (L"Write image to {}", title);
    spec.content    = FileDialogContent::Images;

    hr = m_context.dialogs.PickFileToOpen (m_context.window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    BAIL_OUT_IF (!AskToResume (title, device, picked, false, resume), S_OK);

    text = std::format (L"Overwrite {} with {}?\n\nEverything on that device is replaced.", title, picked.filename().wstring());

    BAIL_OUT_IF (!resume && DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, text.c_str(), L"Write image",
                                            MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK, S_OK);

    SubmitBlockTransfer (link, number, picked, false, resume, forceVerify);

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::TryGetBlockDevice
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkDriveManager::TryGetBlockDevice (const std::wstring & link, int number, CassoLinkBlockDevice & outDevice) const
{
    std::vector<CassoLinkBlockDevice>  devices = (m_context.app.links != nullptr) ? m_context.app.links->GetBlockDevices (TextEncoding::WideToNarrow (link)) : std::vector<CassoLinkBlockDevice>();
    size_t                             index   = (size_t) (number - CassoLinkBlockDevices::kFirstDrive);
    bool                               found   = number >= CassoLinkBlockDevices::kFirstDrive && index < devices.size();



    outDevice = found ? devices[index] : CassoLinkBlockDevice();

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::AskToResume
//
//  False when the user cancels. With no record of a transfer cut short,
//  outResume is false and the user isn't prompted.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkDriveManager::AskToResume (const std::wstring & title, const CassoLinkBlockDevice & device, const std::filesystem::path & image, bool isRead, bool & outResume)
{
    Win32DiskFileIo  fileIo;
    uint32_t         next   = 0;
    std::wstring     text;
    int              answer = IDNO;



    outResume = false;

    if (!CassoLinkBlockTransfer::FindRecord (fileIo, device, TextEncoding::WideToNarrow (image.wstring()), isRead, next))
    {
        return true;
    }

    text = isRead ? std::format (L"A read of {} into {} stopped at block {} of {}.\n\nContinue from there, or start over?", title, image.filename().wstring(), next, device.blocks)
                  : std::format (L"A write of {} to {} stopped at block {} of {}.\n\nContinue from there, or start over?", image.filename().wstring(), title, next, device.blocks);

    answer    = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, text.c_str(), isRead ? L"Create image" : L"Write image",
                                MB_YESNOCANCEL | MB_ICONQUESTION, { L"Continue", L"Start over", L"Cancel" });
    outResume = answer == IDYES;

    return answer != IDCANCEL;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::SubmitBlockTransfer
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::SubmitBlockTransfer (const std::wstring & link, int number, const std::filesystem::path & image, bool isRead, bool resume, bool forceVerify)
{
    HRESULT      hr   = S_OK;
    std::string  path = TextEncoding::WideToNarrow (image.wstring());



    BAIL_OUT_IF (m_context.app.links == nullptr, S_OK);

    hr = m_context.app.links->Submit (TextEncoding::WideToNarrow (link), [number, path, isRead, resume, forceVerify] (CassoLinkSession & session)
    {
        return isRead ? session.ReadBlockDevice (number, path, resume, forceVerify) : session.WriteBlockDevice (number, path, resume, forceVerify);
    },
    [] (const LinkOperation & operation) -> std::wstring
    {
        return FAILED (operation.result) ? CassoLinkConnectionText::DescribeFailure (operation) : std::wstring();
    });

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
    }

    m_context.cassoLinkManager.HandleConnectionChanged();

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}

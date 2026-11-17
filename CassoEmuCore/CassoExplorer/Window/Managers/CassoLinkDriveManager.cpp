#include "Pch.h"

#include "CassoExplorer/Window/Managers/CassoLinkDriveManager.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Actions/ActionsTypes.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/KrunchMapDialog.h"
#include "CassoLink/FullSend/FullSendProtection.h"
#include "Core/TextEncoding.h"
#include "Seams/IHostDialogs.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::CassoLinkDriveManager
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkDriveManager::CassoLinkDriveManager (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::~CassoLinkDriveManager
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkDriveManager::~CassoLinkDriveManager() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::ReadDiskToImage
//
//  A linked Apple drive's disk into an image file the user picks, in the
//  format of its extension. The drive is the one given, else the open
//  drive, else the drive row selected on its machine; the list stays where
//  it is. When sectors cannot be read, the user may read the disk again
//  with them written as zeros.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::ReadDiskToImage (const Location & drive, bool forceVerify)
{
    HRESULT                        hr     = S_OK;
    std::filesystem::path          picked;
    bool                           chosen = false;
    Location                       target;
    std::wstring                   link;
    int                            number = 0;
    bool                           found  = false;
    FileDialogSpec                 spec;
    CassoExplorerActions::Outcome  outcome;



    found = TryGetTargetDrive (drive, target, link, number);
    BAIL_OUT_IF (!found, S_OK);

    if (number >= CassoLinkBlockDevices::kFirstDrive)
    {
        ReadBlockDeviceToImage (target, link, number, forceVerify);
        BAIL_OUT_IF (true, S_OK);
    }

    spec.filters          = { FileDialogFilter { L"Disk images (*.dsk, *.do, *.po, *.d13, *.woz, *.nib, *.nb2)", L"*.dsk;*.do;*.po;*.d13;*.woz;*.nib;*.nb2" },
                              FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid       = s_kLinkDiskPickerGuid;
    spec.defaultFileName  = ActionsAppleDrive::GetDriveImageName (m_context.browser.GetLinks(), TextEncoding::WideToNarrow (link), 6, number);
    spec.defaultExtension = L"dsk";
    spec.askToReplace     = false;
    spec.title            = std::format (L"Create image from {}", m_context.browser.GetTreeModel().GetLinkDriveTitle (target.path));
    spec.content          = FileDialogContent::Images;

    //  The picker's own replace prompt is Win32's; this one follows the theme,
    //  and No goes back to the picker.
    while (!chosen)
    {
        hr = m_context.dialogs.PickFileToSave (m_context.window.GetHwnd(), spec, picked, chosen);
        CHR (hr);
        BAIL_OUT_IF (!chosen, S_OK);

        if (GetFileAttributesW (picked.c_str()) != INVALID_FILE_ATTRIBUTES &&
            DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, (picked.filename().wstring() + L" already exists. Overwrite it?").c_str(),
                            L"Confirm save as", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK)
        {
            spec.initialFolder   = picked.parent_path();
            spec.defaultFileName = picked.filename().wstring();
            chosen               = false;
        }
    }

    //  The second read goes to the link and drive the first one read, not to
    //  whatever drive the window has open by the time the first one fails.
    outcome = m_context.actions.ReadDiskToImage (TextEncoding::WideToNarrow (link), number, picked.wstring(), false, forceVerify, [this, picked, forceVerify] (const LinkOperation & operation, const std::string & link, int drive)
    {
        std::wstring                   list    = TextEncoding::NarrowToWide (CassoLinkDiskTransfer::GetUnreadableListPath (TextEncoding::WideToNarrow (picked.filename().wstring())));
        std::wstring                   message = TextEncoding::NarrowToWide (operation.message) + L"\n\nRead the disk again, writing those sectors as zeros? Their list is saved beside the image as " + list + L".";
        CassoExplorerActions::Outcome  again;

        if (DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, message.c_str(), L"Read disk to image",
                            MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Read again", L"Cancel" }) == IDOK)
        {
            again = m_context.actions.ReadDiskToImage (link, drive, picked.wstring(), true, forceVerify, nullptr);

            if (!again.Succeeded())
            {
                m_context.window.ShowMessage (again.message, MB_ICONERROR);
            }

            m_context.cassoLinkManager.HandleConnectionChanged();
        }
    }, [this, target, picked] () { OfferRawCapture (target, picked); });

    if (!outcome.Succeeded())
    {
        m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
    }

    m_context.cassoLinkManager.HandleConnectionChanged();

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::OfferRawCapture
//
//  After Create image saves a sector image, a disk with a sign of copy
//  protection that the Full Send options check is offered a raw capture,
//  as a WOZ beside the sector image, which isn't removed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::OfferRawCapture (const Location & drive, const std::filesystem::path & image)
{
    HRESULT                hr       = S_OK;
    std::vector<Byte>      bytes;
    std::wstring           question;
    std::filesystem::path  woz      = image;
    bool                   isLikely = false;



    hr = m_context.browser.GetOperations().GetFileIo().ReadAllBytes (TextEncoding::WideToNarrow (image.wstring()), bytes);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    isLikely = FullSendProtection::IsLikelyProtected (bytes, TextEncoding::WideToNarrow (image.filename().wstring()), m_context.prefs.fullSendProtection);
    BAIL_OUT_IF (!isLikely, S_OK);

    question = std::format (L"This disk may be copy-protected and may not boot successfully as a {} image. Would you like to capture in raw mode, which is compatible with copy protection?",
                            image.extension().wstring());

    if (DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, question.c_str(), L"Create image", MB_YESNO | MB_ICONQUESTION, { L"Yes", L"No" }) == IDYES)
    {
        CaptureRawDisk (drive, woz.replace_extension (L".woz").filename().wstring());
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::CaptureRawDisk
//
//  A linked drive's disk captured raw into a WOZ the user picks, for a
//  copy-protected disk, and the tracks whose reads differ reported when
//  it's done.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::CaptureRawDisk (const Location & drive, const std::wstring & suggested)
{
    HRESULT                        hr     = S_OK;
    std::filesystem::path          picked;
    bool                           chosen = false;
    Location                       target;
    std::wstring                   link;
    int                            number = 0;
    bool                           found  = false;
    FileDialogSpec                 spec;
    CassoExplorerActions::Outcome  outcome;
    HWND                           owner  = m_context.window.GetHwnd();



    found = TryGetTargetDrive (drive, target, link, number);
    BAIL_OUT_IF (!found, S_OK);

    spec.filters          = { FileDialogFilter { L"WOZ disk images (*.woz)", L"*.woz" } };
    spec.clientGuid       = s_kLinkDiskPickerGuid;
    spec.defaultFileName  = !suggested.empty() ? suggested : std::filesystem::path (ActionsAppleDrive::GetDriveImageName (m_context.browser.GetLinks(), TextEncoding::WideToNarrow (link), 6, number)).replace_extension (L".woz").wstring();
    spec.defaultExtension = L"woz";
    spec.title            = std::format (L"Capture raw disk from {}", m_context.browser.GetTreeModel().GetLinkDriveTitle (target.path));
    spec.content          = FileDialogContent::Images;

    hr = m_context.dialogs.PickFileToSave (owner, spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    outcome = m_context.actions.CaptureDisk (TextEncoding::WideToNarrow (link), number, picked.wstring(), [this, picked] (const std::wstring & report)
    {
        std::wstring  text = picked.filename().wstring() + L" was captured." + (report.empty() ? std::wstring() : L"\n\n" + report);

        m_context.window.ShowMessage (text.c_str(), MB_ICONINFORMATION);
    });

    if (!outcome.Succeeded())
    {
        m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
    }

    m_context.cassoLinkManager.HandleConnectionChanged();

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::WriteImageToDisk
//
//  An image file onto the open Apple drive's disk, once the user confirms,
//  since everything on the disk is replaced.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::WriteImageToDisk (const Location & drive, bool forceVerify)
{
    HRESULT                hr     = S_OK;
    std::filesystem::path  picked;
    bool                   chosen = false;
    bool                   found  = false;
    Location               target;
    std::wstring           link;
    int                    number = 0;
    FileDialogSpec         spec;



    found = TryGetTargetDrive (drive, target, link, number);
    BAIL_OUT_IF (!found, S_OK);

    if (number >= CassoLinkBlockDevices::kFirstDrive)
    {
        WriteImageToBlockDevice (target, link, number, forceVerify);
        BAIL_OUT_IF (true, S_OK);
    }

    spec.filters        = { FileDialogFilter { L"Disk images (*.dsk, *.do, *.po, *.d13, *.woz, *.nib, *.nb2)", L"*.dsk;*.do;*.po;*.d13;*.woz;*.nib;*.nb2" },
                            FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid     = s_kLinkDiskPickerGuid;
    spec.title          = std::format (L"Write image to {}", m_context.browser.GetTreeModel().GetLinkDriveTitle (target.path));
    spec.content        = FileDialogContent::Images;
    spec.receivingDrive = ReceivingDrive { ReceivingDriveKind::Floppy525, 0 };

    hr = m_context.dialogs.PickFileToOpen (m_context.window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    StartImageWrite (target, picked.wstring(), forceVerify);

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::StartImageWrite
//
//  The drive's boot sector is read first, so the one confirmation can warn
//  when the disk is the link disk the Apple started from: writing over it
//  does not end the link, but no disk is left to start it from again.
//
//  An image onto a linked Apple's drive: the drive is checked for the
//  link disk, the user confirms, and the write is queued.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::StartImageWrite (const Location & drive, const std::wstring & imagePath, bool forceVerify)
{
    HRESULT       hr     = S_OK;
    std::wstring  link;
    int           slot   = 0;
    int           number = 0;
    std::wstring  title  = m_context.browser.GetTreeModel().GetLinkDriveTitle (drive.path);
    std::wstring  file   = std::filesystem::path (imagePath).filename().wstring();
    std::string   name;



    if (!TreeModel::TryParseLinkId (drive.path, link, slot, number) || number == 0)
    {
        return;
    }

    name = TextEncoding::WideToNarrow (link);

    hr = m_context.actions.CheckLinkDisk (name, number, [this, name, number, title, file, imagePath, forceVerify] (bool linkDisk, HRESULT result)
    {
        std::wstring                   message;
        CassoExplorerActions::Outcome  outcome;

        if (FAILED (result))
        {
            m_context.window.ShowMessage (std::format (L"The disk in {} could not be read, so it was not written.", title), MB_ICONERROR);
            return;
        }

        message = linkDisk
                ? std::format (L"{} has the CassoLink disk this Apple started from. Overwriting it does not end the connection, "
                               L"but the Apple cannot start CassoLink from that disk again until you choose Write CassoLink to disk."
                               L"\n\nOverwrite {} with {}?", title, title, file)
                : std::format (L"Overwrite {} with {}?\n\nEverything on that disk is replaced.", title, file);

        if (DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, message.c_str(), L"Write image",
                            MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK)
        {
            return;
        }

        outcome = m_context.actions.WriteImageToAppleDrive (name, number, imagePath, forceVerify);

        if (!outcome.Succeeded())
        {
            m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
        }
    });

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
    }

    m_context.cassoLinkManager.HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::TryGetSelectedLinkDrive
//
//  A drive row's path is the drive's id.
//
//  The drive of the one drive row selected while the list is on a linked
//  machine. The image commands run on it, and dragging it drags its disk,
//  as for the drive's node in the tree.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkDriveManager::TryGetSelectedLinkDrive (Location & outDrive) const
{
    const std::vector<int>  & selected = m_context.browser.GetSelectedRows();
    std::wstring              link;
    std::wstring              id;
    int                       slot     = 0;
    int                       drive    = 0;
    bool                      found    = IsOnLinkedMachine() && selected.size() == 1 && selected[0] >= 0 && (size_t) selected[0] < m_context.browser.GetRows().size();



    if (found)
    {
        id    = m_context.browser.GetRows()[(size_t) selected[0]].hostPath;
        found = TreeModel::TryParseLinkId (id, link, slot, drive) && drive != 0;
    }

    if (found)
    {
        outDrive = Location::MakeAppleDrive (id, std::string());
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::TryGetTargetDrive
//
//  The drive a whole-disk command works on: the one given, as from a drive's
//  menu, else the open drive, else the drive row selected on its machine.
//  The list stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkDriveManager::TryGetTargetDrive (const Location & drive, Location & outDrive, std::wstring & outLink, int & outNumber) const
{
    int  slot = 0;



    outDrive = drive;

    if (outDrive.path.empty() && m_context.browser.IsAppleDriveOpen())
    {
        outDrive = Location::MakeAppleDrive (m_context.browser.GetLocation().path, std::string());
    }
    else if (outDrive.path.empty() && !TryGetSelectedLinkDrive (outDrive))
    {
        return false;
    }

    return TreeModel::TryParseLinkId (outDrive.path, outLink, slot, outNumber) && outNumber != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::IsOnLinkedMachine
//
//  The list on a linked Apple, which lists its drives, rather than on one of
//  the drives.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkDriveManager::IsOnLinkedMachine() const
{
    Location      shown = m_context.browser.GetLocation();
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    return shown.kind == Location::Kind::Root && TreeModel::TryParseLinkId (shown.path, link, slot, drive) && drive == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::OpenSelectedLinkDrive
//
//  The image commands run on the open drive, so a drive selected on its
//  machine is opened first, as its context menu opens it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::OpenSelectedLinkDrive()
{
    Location  drive;



    if (!m_context.browser.IsAppleDriveOpen() && TryGetSelectedLinkDrive (drive))
    {
        m_context.addressBar.GoToLocation (drive);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::SaveLinkToDisk
//
//  The link's boot disk onto the open Apple drive, once the user confirms,
//  since everything on the disk is replaced.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::SaveLinkToDisk (const Location & drive)
{
    wchar_t                        temp[MAX_PATH] = {};
    std::wstring                   message;
    Location                       target;
    std::wstring                   link;
    int                            number         = 0;
    CassoExplorerActions::Outcome  outcome;



    if (!TryGetTargetDrive (drive, target, link, number) || GetTempPathW (MAX_PATH, temp) == 0)
    {
        return;
    }

    message = std::format (L"Overwrite {} with CassoLink?\n\nEverything on the disk is replaced, and the Apple then starts CassoLink when it boots from the disk.",
                           m_context.browser.GetTreeModel().GetLinkDriveTitle (target.path));

    if (DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, message.c_str(), L"Write CassoLink to disk",
                        MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK)
    {
        return;
    }

    outcome = m_context.actions.SaveLinkToDisk (TextEncoding::WideToNarrow (link), number, std::wstring (temp).substr (0, wcslen (temp) - 1));

    if (!outcome.Succeeded())
    {
        m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
    }

    m_context.cassoLinkManager.HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::FormatAppleDisk
//
//  A linked drive's disk formatted, from the format dialog's choices, once
//  the user confirms that everything on it is erased. The list stays where
//  it is; the drive's row shows the new disk when the write finishes.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::FormatAppleDisk (const Location & location)
{
    wchar_t                              temp[MAX_PATH] = {};
    std::wstring                         link;
    int                                  slot           = 0;
    int                                  number         = 0;
    std::wstring                         drive;
    std::wstring                         message;
    CassoExplorerNewDiskDialog::Outcome  choice;
    CassoExplorerActions::Outcome        outcome;



    if (!TreeModel::TryParseLinkId (location.path, link, slot, number) || number == 0 || GetTempPathW (MAX_PATH, temp) == 0)
    {
        return;
    }

    drive  = m_context.browser.GetTreeModel().GetLinkDriveTitle (location.path);
    choice = CassoExplorerNewDiskDialog::Ask (m_context.window.GetHwnd(), m_context.theme, true, {}, CassoExplorerNewDiskChoices::kFormatDos33, drive, true);

    if (!choice.confirmed || !m_context.commandManager.TryPrepareBootDisk (choice.request))
    {
        return;
    }

    message = std::format (L"Format the disk in {}? Everything on it will be erased.", drive);

    if (DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, message.c_str(), L"Format", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Format", L"Cancel" }) != IDOK)
    {
        return;
    }

    outcome = m_context.actions.FormatAppleDrive (TextEncoding::WideToNarrow (link), number, choice.request, std::wstring (temp).substr (0, wcslen (temp) - 1));

    if (!outcome.Succeeded())
    {
        m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
    }

    m_context.cassoLinkManager.HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::KrunchAppleDisk
//
//  A linked drive's Apple Pascal disk krunched, with no confirmation, as an
//  image's is: no file is lost, and the files move on the Apple, so the disk
//  is whole after a link that drops partway.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::KrunchAppleDisk (const Location & location)
{
    std::wstring                    link;
    int                             slot      = 0;
    int                             number    = 0;
    std::string                     name;
    std::shared_ptr<LinkOperation>  operation;
    CassoExplorerActions::Outcome   outcome;



    if (!TreeModel::TryParseLinkId (location.path, link, slot, number) || number == 0)
    {
        return;
    }

    name    = TextEncoding::WideToNarrow (link);
    outcome = m_context.actions.KrunchAppleDrive (name, number, &operation);

    if (!outcome.Succeeded())
    {
        m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
    }
    else if (operation != nullptr)
    {
        m_krunchMap = KrunchMapDialog::Open (m_context.window.GetHwnd(), m_context.theme, operation, m_context.browser.GetTreeModel().GetLinkDriveTitle (location.path), [this, name]()
        {
            CassoLinkSession  * session = (m_context.app.links != nullptr) ? m_context.app.links->GetSession (name) : nullptr;

            if (session != nullptr)
            {
                session->Cancel();
            }
        });
    }

    m_context.cassoLinkManager.HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::SaveLinkDiskImage
//
//  The link's boot disk as an image file on the PC, for an emulator, at the
//  link's speed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::SaveLinkDiskImage (const std::string & name)
{
    HRESULT                        hr     = S_OK;
    std::filesystem::path          picked;
    bool                           chosen = false;
    FileDialogSpec                 spec;
    CassoLinkSettings              link;
    CassoExplorerActions::Outcome  outcome;



    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        if (settings.name == name)
        {
            link = settings;
        }
    }

    spec.filters          = { FileDialogFilter { L"Disk images (*.dsk)", L"*.dsk" } };
    spec.clientGuid       = s_kLinkDiskPickerGuid;
    spec.defaultFileName  = L"CassoLink.dsk";
    spec.defaultExtension = L"dsk";
    spec.title            = L"Save CassoLink disk image";
    spec.content          = FileDialogContent::Images;

    hr = m_context.dialogs.PickFileToSave (m_context.window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    outcome = m_context.actions.SaveLinkDiskImage (picked.wstring(), link);

    if (!outcome.Succeeded())
    {
        m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}

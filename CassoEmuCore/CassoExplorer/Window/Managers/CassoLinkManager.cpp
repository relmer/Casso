#include "Pch.h"

#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "Seams/Win32FirewallHelper.h"
#include "Net/FirewallText.h"
#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "CassoExplorer/CassoLinkActivityDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Core/TextEncoding.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"
#include "Seams/IHostDialogs.h"
#include "Seams/Win32HostDialogs.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/Window/Panes/ExplorerStatusBar.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "CassoExplorer/Actions/ActionsTypes.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"





//  The window whose m_cassetteWav is the sound in progress. PlaySound reads
//  from that memory, so that window stops the sound before the memory is
//  freed, and no other window stops it.
static const ExplorerContext *  s_cassettePlayer = nullptr;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::CassoLinkManager
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkManager::CassoLinkManager (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::CreateListActions
//
//  The buttons under the list's message for a link: bootstrap the Apple or
//  connect to it, or allow it through Windows Firewall on private or on all
//  networks.
//
//  The list's button under its message: Connect for a link the user
//  disconnected, Bootstrap the Apple for a serial link whose Apple is
//  not replying, or Allow in Windows Firewall for a connection the
//  firewall blocks. On a public network the second button beside it is
//  Make this network private.
//  The buttons under the list's message for a link, and whether the
//  message is the firewall's.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::CreateListActions()
{
    m_listAction = m_context.window.CreateChild<DxuiButton> (L"Bootstrap the Apple");
    m_listAction->SetVisible (false);
    m_listAction->SetOnClick ([this] ()
    {
        std::string  link = m_listActionConnection;

        if (m_listActionFirewall)
        {
            FixFirewall (false);
            return;
        }

        if (link.empty())
        {
            return;
        }

        if (m_listActionConnects)
        {
            Connect (link);
        }
        else
        {
            BootstrapConnection (link);
        }
    });

    m_listActionAlt = m_context.window.CreateChild<DxuiButton> (FirewallText::kPrivateAction);
    m_listActionAlt->SetVisible (false);
    m_listActionAlt->SetOnClick ([this] () { FixFirewall (true); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::LayoutListActions
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::LayoutListActions()
{
    m_listAction->Layout    (GetListActionRect(),    m_context.scaler);
    m_listActionAlt->Layout (GetListActionAltRect(), m_context.scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::HandleListActionMouse
//
//  Each button has every move, so its hover ends as the pointer moves off
//  it, and the presses inside it. Returns whether the event was over one.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::HandleListActionMouse (const DxuiMouseEvent & ev)
{
    for (DxuiButton * action : { m_listAction, m_listActionAlt })
    {
        bool  inside = action->IsVisible() && ExplorerLayout::Contains (action->GetBounds(), ev.positionDip);

        if (action->IsVisible() && (ev.kind == DxuiMouseEventKind::Move || inside))
        {
            action->OnMouse (ev);
            m_context.window.Invalidate();
        }

        if (inside)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::~CassoLinkManager
//
////////////////////////////////////////////////////////////////////////////////

CassoLinkManager::~CassoLinkManager()
{
    if (s_cassettePlayer == &m_context)
    {
        PlaySoundW (nullptr, nullptr, 0);
        s_cassettePlayer = nullptr;
    }

    //  A running helper is left to exit; only the handle is closed.
    if (m_firewallHelper != nullptr)
    {
        CloseHandle (m_firewallHelper);
        m_firewallHelper = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ReadDiskToImage
//
//  A linked Apple drive's disk into an image file the user picks, in the
//  format of its extension. The drive is the one given, else the open
//  drive, else the drive row selected on its machine; the list stays where
//  it is. When sectors cannot be read, the user may read the disk again
//  with them written as zeros.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ReadDiskToImage (const Location & drive)
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

    spec.filters          = { FileDialogFilter { L"Disk images (*.dsk, *.do, *.po, *.woz, *.nib)", L"*.dsk;*.do;*.po;*.woz;*.nib" },
                              FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid       = s_kLinkDiskPickerGuid;
    spec.defaultFileName  = L"disk.dsk";
    spec.defaultExtension = L"dsk";
    spec.askToReplace     = false;
    spec.title            = std::format (L"Create image from {}", m_context.browser.GetTreeModel().GetLinkDriveTitle (target.path));

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
    outcome = m_context.actions.ReadDiskToImage (TextEncoding::WideToNarrow (link), number, picked.wstring(), false, [this, picked] (const LinkOperation & operation, const std::string & link, int drive)
    {
        std::wstring                   list    = TextEncoding::NarrowToWide (CassoLinkDiskTransfer::GetUnreadableListPath (TextEncoding::WideToNarrow (picked.filename().wstring())));
        std::wstring                   message = TextEncoding::NarrowToWide (operation.message) + L"\n\nRead the disk again, writing those sectors as zeros? Their list is saved beside the image as " + list + L".";
        CassoExplorerActions::Outcome  again;

        if (DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, message.c_str(), L"Read disk to image",
                            MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Read again", L"Cancel" }) == IDOK)
        {
            again = m_context.actions.ReadDiskToImage (link, drive, picked.wstring(), true, nullptr);

            if (!again.Succeeded())
            {
                m_context.window.ShowMessage (again.message, MB_ICONERROR);
            }

            HandleConnectionChanged();
        }
    });

    if (!outcome.Succeeded())
    {
        m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
    }

    HandleConnectionChanged();

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::GetDefaultFullSendFolder
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkManager::GetDefaultFullSendFolder()
{
    wchar_t      * downloads = nullptr;
    std::wstring   folder;
    HRESULT        hr        = SHGetKnownFolderPath (FOLDERID_Downloads, 0, nullptr, &downloads);



    if (SUCCEEDED (hr))
    {
        folder = (std::filesystem::path (downloads) / L"Casso Full Send").wstring();
    }

    CoTaskMemFree (downloads);

    return folder;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ApplyFullSendPrefs
//
//  The folder is made when a session starts, not here, so one never used is
//  never made.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ApplyFullSendPrefs()
{
    std::wstring  folder = m_context.prefs.fullSendFolder.empty() ? GetDefaultFullSendFolder() : m_context.prefs.fullSendFolder;



    if (m_context.app.links != nullptr)
    {
        m_context.app.links->SetFullSendTarget (TextEncoding::WideToNarrow (folder), m_context.prefs.fullSendExtension, [] (const std::string & path)
        {
            std::error_code  error;

            std::filesystem::create_directories (TextEncoding::NarrowToWide (path), error);

            return error ? HRESULT_FROM_WIN32 (error.value()) : S_OK;
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::WriteImageToDisk
//
//  An image file onto the open Apple drive's disk, once the user confirms,
//  since everything on the disk is replaced.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::WriteImageToDisk (const Location & drive)
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

    spec.filters    = { FileDialogFilter { L"Disk images (*.dsk, *.do, *.po, *.woz, *.nib)", L"*.dsk;*.do;*.po;*.woz;*.nib" },
                        FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid = s_kLinkDiskPickerGuid;
    spec.title      = std::format (L"Write image to {}", m_context.browser.GetTreeModel().GetLinkDriveTitle (target.path));

    hr = m_context.dialogs.PickFileToOpen (m_context.window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    StartImageWrite (target, picked.wstring());

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::StartImageWrite
//
//  The drive's boot sector is read first, so the one confirmation can warn
//  when the disk is the link disk the Apple started from: writing over it
//  does not end the link, but no disk is left to start it from again.
//
//  An image onto a linked Apple's drive: the drive is checked for the
//  link disk, the user confirms, and the write is queued.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::StartImageWrite (const Location & drive, const std::wstring & imagePath)
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

    hr = m_context.actions.CheckLinkDisk (name, number, [this, name, number, title, file, imagePath] (bool linkDisk, HRESULT result)
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

        outcome = m_context.actions.WriteImageToAppleDrive (name, number, imagePath);

        if (!outcome.Succeeded())
        {
            m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
        }
    });

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
    }

    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::TryGetSelectedLinkDrive
//
//  A drive row's path is the drive's id.
//
//  The drive of the one drive row selected while the list is on a linked
//  machine. The image commands run on it, and dragging it drags its disk,
//  as for the drive's node in the tree.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::TryGetSelectedLinkDrive (Location & outDrive) const
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
//  CassoLinkManager::TryGetTargetDrive
//
//  The drive a whole-disk command works on: the one given, as from a drive's
//  menu, else the open drive, else the drive row selected on its machine.
//  The list stays where it is.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::TryGetTargetDrive (const Location & drive, Location & outDrive, std::wstring & outLink, int & outNumber) const
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
//  CassoLinkManager::IsOnLinkedMachine
//
//  The list on a linked Apple, which lists its drives, rather than on one of
//  the drives.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsOnLinkedMachine() const
{
    Location      shown = m_context.browser.GetLocation();
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    return shown.kind == Location::Kind::Root && TreeModel::TryParseLinkId (shown.path, link, slot, drive) && drive == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::OpenSelectedLinkDrive
//
//  The image commands run on the open drive, so a drive selected on its
//  machine is opened first, as its context menu opens it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::OpenSelectedLinkDrive()
{
    Location  drive;



    if (!m_context.browser.IsAppleDriveOpen() && TryGetSelectedLinkDrive (drive))
    {
        m_context.addressBar.GoToLocation (drive);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::SaveLinkToDisk
//
//  The link's boot disk onto the open Apple drive, once the user confirms,
//  since everything on the disk is replaced.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::SaveLinkToDisk (const Location & drive)
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

    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::FormatAppleDisk
//
//  A linked drive's disk formatted, from the format dialog's choices, once
//  the user confirms that everything on it is erased. The list stays where
//  it is; the drive's row shows the new disk when the write finishes.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::FormatAppleDisk (const Location & location)
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

    if (!choice.confirmed)
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

    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::SaveLinkDiskImage
//
//  The link's boot disk as an image file on the PC, for an emulator, at the
//  link's speed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::SaveLinkDiskImage (const std::string & name)
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





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::GetListActionRect
//
//  Centered under the list's message, which is centered in the list.
//
////////////////////////////////////////////////////////////////////////////////

RECT CassoLinkManager::GetListActionRect() const
{
    RECT  message = m_context.listPane.GetListMessageRect();
    bool  pair    = m_listActionAlt->IsVisible();
    int   width   = m_context.scaler.ToPx (m_listActionFirewall ? kListActionWideDip : kListActionWidthDip);
    int   gap     = m_context.scaler.ToPx (ExplorerMetrics::kListActionGapDip);
    int   height  = m_context.scaler.ToPx (kListActionHeightDip);
    int   span    = pair ? width * 2 + gap : width;
    int   left    = (message.left + message.right - span) / 2;
    int   top     = m_listActionFirewall ? message.bottom + gap : (message.top + message.bottom) / 2 + m_context.scaler.ToPx (ExplorerMetrics::kListActionOffsetDip);



    return RECT { left, top, left + width, top + height };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::GetListActionAltRect
//
//  Right of the first button, the two centered as a pair.
//
////////////////////////////////////////////////////////////////////////////////

RECT CassoLinkManager::GetListActionAltRect() const
{
    RECT  first = GetListActionRect();
    int   gap   = m_context.scaler.ToPx (ExplorerMetrics::kListActionGapDip);
    int   width = first.right - first.left;



    return RECT { first.right + gap, first.top, first.right + gap + width, first.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::FillListAction
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::FillListAction (bool hasMessage)
{
    std::wstring   name;
    int            slot   = 0;
    int            drive  = 0;
    std::string    disconnected;
    LinkTreeEntry  entry;
    bool           onLink = hasMessage && m_context.app.links != nullptr && TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, name, slot, drive) && drive == 0;



    if (onLink && m_context.app.links->IsHeld (TextEncoding::WideToNarrow (name)))
    {
        disconnected = TextEncoding::WideToNarrow (name);
    }

    //  The firewall first: until the port is allowed, no connection the other
    //  actions start arrives at this PC.
    m_listActionFirewall = onLink && disconnected.empty() && TryGetConnectionEntry (name, entry) && !entry.firewall.empty();
    m_listActionConnects          = !m_listActionFirewall && !disconnected.empty();
    m_listActionConnection              = m_listActionFirewall ? std::string() : m_listActionConnects ? disconnected : hasMessage ? GetBootstrapConnection() : std::string();

    m_context.listPane.AlignMessage (m_listActionFirewall);
    m_listAction->SetLabel      (m_listActionFirewall ? FirewallText::kAllowAction : m_listActionConnects ? L"Connect" : L"Bootstrap the Apple");
    m_listActionAlt->SetVisible (m_listActionFirewall && entry.firewallPublic);
    m_listAction->Layout        (GetListActionRect(), m_context.scaler);
    m_listActionAlt->Layout     (GetListActionAltRect(), m_context.scaler);
    m_listAction->SetVisible    (m_listActionFirewall || !m_listActionConnection.empty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::TryGetConnectionEntry
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::TryGetConnectionEntry (const std::wstring & name, LinkTreeEntry & outEntry) const
{
    const std::vector<LinkTreeEntry>  & links = m_context.browser.GetTreeModel().GetLinks();
    auto                                found = std::find_if (links.begin(), links.end(), [&name] (const LinkTreeEntry & link) { return link.name == name; });



    if (found != links.end())
    {
        outEntry = *found;
    }

    return found != links.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::PollFirewall
//
//  Every window polls the one monitor, so each compares the monitor's version
//  with the one it drew rather than relying on Update's result, which only
//  the first window to poll after a change would see.
//
//  The firewall read again on the drive poll, the views redrawn when it
//  changed, and the elevated helper's result once it exits.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::PollFirewall()
{
    HRESULT       result  = S_OK;
    std::wstring  message;



    if (m_context.app.firewall == nullptr)
    {
        return;
    }

    //  The helper changed the rules, which no notification reports.
    if (m_firewallHelper != nullptr && Win32FirewallHelper::TryFinish (m_firewallHelper, result))
    {
        m_context.app.firewall->Invalidate();

        if (FAILED (result))
        {
            message = std::format (L"Windows Firewall could not be changed: {}", ChangePrompt::DescribeError (result));
            m_context.window.ShowMessage (message, MB_ICONWARNING);
        }
    }

    m_context.app.firewall->Update();

    if (m_context.app.firewall->GetVersion() != m_firewallVersion)
    {
        m_firewallVersion = m_context.app.firewall->GetVersion();
        RefreshCassoLinkViews();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::FixFirewall
//
//  Every connection's port is in the plan, not only the one the list is on,
//  so one UAC prompt allows them all. Declining the prompt changes nothing
//  and is not an error.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::FixFirewall (bool markPrivate)
{
    HRESULT                    hr = S_OK;
    std::vector<FirewallPort>  ports;
    FirewallPort               port;
    FirewallPlan               plan;



    BAIL_OUT_IF (m_context.app.firewall == nullptr || m_context.app.links == nullptr || m_firewallHelper != nullptr, S_OK);

    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        if (CassoLinkConnections::GetInboundPort (settings, port) && std::find (ports.begin(), ports.end(), port) == ports.end())
        {
            ports.push_back (port);
        }
    }

    plan = FirewallRemedy::MakePlan (m_context.app.firewall->GetSnapshot(), m_context.app.firewall->GetProgram(), CassoLinkConnections::kFirewallTitle, ports, markPrivate);
    BAIL_OUT_IF (plan.ports.empty() && !plan.markPrivate, S_OK);

    hr = Win32FirewallHelper::Launch (m_context.window.GetHwnd(), plan, m_firewallHelper);
    BAIL_OUT_IF (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED), S_OK);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        m_context.window.ShowMessage (std::format (L"Windows Firewall could not be changed: {}", ChangePrompt::DescribeError (hr)), MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::GetBootstrapConnection
//
//  The serial link the list has, when its Apple is not replying and no
//  bootstrap is under way; empty otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::string CassoLinkManager::GetBootstrapConnection() const
{
    std::wstring        name;
    int                 slot    = 0;
    int                 drive   = 0;
    std::string         link;
    CassoLinkSession  * session = nullptr;
    LinkState           state   = LinkState::Closed;
    bool                waiting = false;



    if (m_context.app.links == nullptr || !TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, name, slot, drive) || drive != 0)
    {
        return std::string();
    }

    link    = TextEncoding::WideToNarrow (name);
    session = m_context.app.links->GetSession (link);
    state   = (session != nullptr) ? session->GetState() : LinkState::Closed;
    waiting = IsSerialConnection (link) && !m_context.browser.GetTreeModel().IsLinkReplying (name)
              && state != LinkState::Bootstrapping && state != LinkState::LoadingProgram;

    return waiting ? link : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::EditConnection
//
//  An empty name adds a link. An edited link is stopped, as its settings
//  are no longer the ones its port was opened with.
//
//  The CassoLink connections: added or edited through the link dialog, removed,
//  or disconnected, which closes the link's port. Each change is saved
//  at once.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::EditConnection (const std::string & name)
{
    HRESULT                           hr      = S_OK;
    CassoLinkSettings                 initial;
    std::vector<std::wstring>         others;
    std::vector<std::wstring>         ports;
    CassoExplorerLinkDialog::Outcome  outcome;
    auto                              isNamed = [&name] (const CassoLinkSettings & link) { return link.name == name; };
    auto                              found   = std::find_if (m_context.prefs.links.begin(), m_context.prefs.links.end(), isNamed);



    if (found != m_context.prefs.links.end())
    {
        initial = *found;
    }

    for (const CassoLinkSettings & link : m_context.prefs.links)
    {
        if (link.name != name)
        {
            others.push_back (TextEncoding::NarrowToWide (link.name));
        }
    }

    //  With no ports listed, the drop-down has Any alone.
    if (m_context.app.links != nullptr)
    {
        hr = m_context.app.links->ListComPorts (ports);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    outcome = CassoExplorerLinkDialog::Ask (m_context.window.GetHwnd(), m_context.theme, initial, others, ports, found == m_context.prefs.links.end(),
                                            [this] (HWND owner, std::wstring & inOutFolder) -> bool
    {
        std::filesystem::path  picked;
        bool                   chosen = false;
        HRESULT                result = m_context.dialogs.PickFolder (owner, picked, chosen);

        if (SUCCEEDED (result) && chosen)
        {
            inOutFolder = picked.wstring();
        }

        return SUCCEEDED (result) && chosen;
    });

    if (!outcome.confirmed)
    {
        return;
    }

    //  Another window can add or remove a connection while the dialog is open,
    //  so the link is found again.
    found = std::find_if (m_context.prefs.links.begin(), m_context.prefs.links.end(), isNamed);

    if (found != m_context.prefs.links.end())
    {
        *found = outcome.settings;
        ApplyConnections();
        return;
    }

    m_context.prefs.links.push_back (outcome.settings);
    ApplyConnections();

    //  A new connection is selected in the tree, as a new folder is.
    m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::MakeLinkId (TextEncoding::NarrowToWide (outcome.settings.name))));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::RemoveConnection
//
//  A tab on the link goes to This PC.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::RemoveConnection (const std::string & name)
{
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    std::erase_if (m_context.prefs.links, [&name] (const CassoLinkSettings & settings) { return settings.name == name; });

    if (TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, link, slot, drive) && TextEncoding::WideToNarrow (link) == name)
    {
        m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::kThisPcRootId));
    }

    ApplyConnections();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::Disconnect
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::Disconnect (const std::string & name)
{
    m_context.app.links->Disconnect (name);
    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::SearchComPorts
//
//  The link is started again, searching the COM ports for the Apple; the
//  link's root reports the search and then what it found.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::SearchComPorts (const std::string & name)
{
    HRESULT  hr = S_OK;



    m_context.app.links->Stop (name);

    hr = m_context.app.links->Start (name, true);

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
    }

    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::BootstrapConnection
//
//  The user readies the Apple first; the loader is then typed into it
//  over the link.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::BootstrapConnection (const std::string & name)
{
    HRESULT       hr      = S_OK;
    std::wstring  steps;
    int           answer  = IDCANCEL;



    steps  = L"On the Apple, at the prompt, type:\n\n"
             L"1. **IN#2** on a //c, or IN# and the Super Serial Card's slot, then press Return\n"
             L"2. **Ctrl+A 14B**, then press Return\n\n"
             L"The loader is then typed into the Apple, which takes about 40 seconds.";
    answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, steps.c_str(), L"Bootstrap the Apple",
                             MB_OKCANCEL | MB_ICONINFORMATION, { L"Bootstrap", L"Cancel" });

    if (answer != IDOK)
    {
        return;
    }

    hr = m_context.app.links->Bootstrap (name, [] (const LinkOperation & operation) -> std::wstring
    {
        return FAILED (operation.result) ? CassoLinkConnections::DescribeFailure (operation) : std::wstring();
    });

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
    }

    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::PlayCassetteBootstrap
//
//  The shim as audio from the PC's sound output into a ][+ or //e's
//  cassette input. The user readies the monitor's read
//  first. The link is started while the audio is sent, so its session is
//  open when the shim sends Ready.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::PlayCassetteBootstrap (const std::string & name)
{
    HRESULT            hr      = S_OK;
    CassoLinkSettings  link;
    std::string        command;
    std::wstring       steps;
    int                answer  = IDCANCEL;
    BOOL               played  = FALSE;



    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        link = (settings.name == name) ? settings : link;
    }

    //  PlaySound has one sound at a time, and the buffer is about to change.
    PlaySoundW (nullptr, nullptr, 0);
    s_cassettePlayer = nullptr;

    hr = LinkAudioBootstrap::BuildWav (CassoLinkConnections::MakeShimOptions (CassoLinkConnections::GetPcName(), link), m_cassetteWav, command);
    CHR (hr);

    command.pop_back();

    steps  = std::format (L"Connect this PC's sound output to the cassette input of the ][+ or //e, and turn the volume up most of the way.\n\n"
                          L"On the Apple, at the prompt, type:\n\n"
                          L"1. **CALL -151**, then press Return\n"
                          L"2. **{}**, then press Return\n\n"
                          L"Then choose Send. The audio takes about 12 seconds.", TextEncoding::NarrowToWide (command));
    answer = DxuiMessageBox (m_context.window.GetHwnd(), m_context.theme, steps.c_str(), L"Bootstrap from the cassette input",
                             MB_OKCANCEL | MB_ICONINFORMATION, { L"Send", L"Cancel" });
    BAIL_OUT_IF (answer != IDOK, S_OK);

    played = PlaySoundW (reinterpret_cast<LPCWSTR> (m_cassetteWav.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    CBR (played);

    s_cassettePlayer = &m_context;

    if (!m_context.app.links->IsStarted (name))
    {
        Connect (name);
    }

Error:
    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"The cassette bootstrap could not be sent to this PC's sound output.", MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::SaveCassetteBootstrap
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::SaveCassetteBootstrap (const std::string & name)
{
    HRESULT                        hr     = S_OK;
    std::filesystem::path          picked;
    bool                           chosen = false;
    FileDialogSpec                 spec;
    CassoLinkSettings              link;
    std::string                    command;
    CassoExplorerActions::Outcome  outcome;



    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        link = (settings.name == name) ? settings : link;
    }

    spec.filters          = { FileDialogFilter { L"Sound files (*.wav)", L"*.wav" } };
    spec.clientGuid       = s_kLinkDiskPickerGuid;
    spec.defaultFileName  = L"CassoLink.wav";
    spec.defaultExtension = L"wav";
    spec.title            = L"Save cassette bootstrap";

    hr = m_context.dialogs.PickFileToSave (m_context.window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    outcome = m_context.actions.SaveCassetteBootstrap (picked.wstring(), link, command);

    if (!outcome.Succeeded())
    {
        m_context.window.ShowMessage (outcome.message, MB_ICONERROR);
    }
    else
    {
        command.pop_back();
        m_context.window.ShowMessage (std::format (L"To load it, type CALL -151 and then {} on the Apple, and play the file into its cassette input.",
                                           TextEncoding::NarrowToWide (command)), MB_ICONINFORMATION);
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ReleaseIdleConnections
//
//  A link connects only while its machine, or one of its drives, is the
//  location of some window. One that has not connected by the time every
//  window has gone elsewhere is stopped, closing its port; one that has
//  connected, or is bootstrapping or loading the program, stays.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ReleaseIdleConnections()
{
    std::wstring              current;
    int                       slot    = 0;
    int                       drive   = 0;
    std::vector<std::string>  release;



    if (m_context.app.links == nullptr)
    {
        return;
    }

    TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, current, slot, drive);
    m_context.app.links->SetViewing (&m_context, TextEncoding::WideToNarrow (current));

    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        CassoLinkSession  * session = m_context.app.links->GetSession (settings.name);
        LinkState           state   = session != nullptr ? session->GetState() : LinkState::Closed;
        bool                connected = state == LinkState::Ready || state == LinkState::Busy
                                     || state == LinkState::Bootstrapping || state == LinkState::LoadingProgram;

        //  ADTPro's client speaks first, so an ADTPro server stays open for it.
        if (session != nullptr && !connected && settings.transport != LinkTransport::AdtProServer && !m_context.app.links->IsViewed (settings.name))
        {
            release.push_back (settings.name);
        }
    }

    for (const std::string & name : release)
    {
        m_context.app.links->Stop (name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::Connect
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::Connect (const std::string & name)
{
    HRESULT  hr = m_context.app.links->Start (name);



    IGNORE_RETURN_VALUE (hr, S_OK);
    HandleConnectionChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsSerialConnection
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsSerialConnection (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.transport == LinkTransport::Serial;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::CanBootstrapConnection
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::CanBootstrapConnection (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.CanBootstrap();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::AddConnectionRows
//
//  A linked machine's settings, its port and the machine itself; a drive's
//  whole disk; and CassoLink's Add a CassoLink machine. The tree's node and
//  the list's row have the same rows.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::AddConnectionRows (std::vector<DxuiPopupMenuItem> & items, bool hasLocation, const Location & location)
{
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    if (m_context.app.links != nullptr && hasLocation && location.kind == Location::Kind::Root && TreeModel::TryParseLinkId (location.path, link, slot, drive))
    {
        std::string  name = TextEncoding::WideToNarrow (link);

        bool  saved = !m_context.app.links->IsDiscovered (name);

        if (saved)
        {
            m_context.menus.AddMenuCommand (items, L"&Edit connection...", [this, name]() { EditConnection (name); });
        }

        if (m_context.app.links->IsStarted (name))
        {
            m_context.menus.AddMenuCommand (items, L"&Disconnect", [this, name]() { Disconnect (name); });
        }
        else
        {
            m_context.menus.AddMenuCommand (items, L"&Connect", [this, name]() { Connect (name); });
        }

        if (IsSerialConnection (name))
        {
            m_context.menus.AddMenuCommand (items, L"&Search COM ports", [this, name]() { SearchComPorts (name); });
        }

        //  Only an Apple on a serial card can be started from here.
        if (CanBootstrapConnection (name))
        {
            m_context.menus.AddMenuCommand (items, L"&Bootstrap the Apple...", [this, name]() { BootstrapConnection (name); });
            m_context.menus.AddMenuCommand (items, L"Bootstrap from the c&assette input...", [this, name]() { PlayCassetteBootstrap (name); });
            m_context.menus.AddMenuCommand (items, L"Save cassette boo&tstrap...", [this, name]() { SaveCassetteBootstrap (name); });
        }

        //  A boot disk is built for its link: at its speed, a Wi-Fi modem's
        //  dialing this PC, and an Uthernet II's with the link's address for
        //  when no DHCP reply comes.
        m_context.menus.AddMenuCommand (items, L"Save CassoLink disk &image...", [this, name]() { SaveLinkDiskImage (name); });

        if (saved)
        {
            m_context.menus.AddMenuCommand (items, L"Re&move connection", [this, name]() { RemoveConnection (name); });
        }
    }
    else if (m_context.app.links != nullptr && hasLocation && location.kind == Location::Kind::AppleDrive && location.innerPath.empty())
    {
        //  A drive's whole disk, read or written from its node.
        std::wstring  title = m_context.browser.GetTreeModel().GetLinkDriveTitle (location.path);

        m_context.menus.AddMenuCommand (items, std::format (L"Create &image from {}...", title).c_str(), [this, location]() { ReadDiskToImage (location); });
        m_context.menus.AddMenuCommand (items, std::format (L"&Write image to {}...", title).c_str(), [this, location]() { WriteImageToDisk (location); });
        m_context.menus.AddMenuCommand (items, L"Write &CassoLink to disk...", [this, location]() { SaveLinkToDisk (location); });
        m_context.menus.AddMenuCommand (items, L"&Format disk...",            [this, location]() { FormatAppleDisk (location); });
    }

    if (m_context.app.links != nullptr && hasLocation && location.kind == Location::Kind::Root && location.path == TreeModel::kCassoLinkRootId)
    {
        m_context.menus.AddMenuCommand (items, L"&Add a CassoLink connection...", [this]() { EditConnection (std::string()); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::StartAdtProServers
//
//  ADTPro's client speaks first, so each ADTPro-compatible server is open
//  from the start, and from when it is added, unless the user disconnected
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::StartAdtProServers()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_context.app.links == nullptr, S_OK);

    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        bool  idle = settings.transport == LinkTransport::AdtProServer && !m_context.app.links->IsStarted (settings.name) && !m_context.app.links->IsHeld (settings.name);

        if (idle)
        {
            hr = m_context.app.links->Start (settings.name);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsConnectionReplying
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsConnectionReplying (const std::string & name) const
{
    CassoLinkSession  * session = m_context.app.links->GetSession (name);
    LinkState           state   = (session != nullptr) ? session->GetState() : LinkState::Closed;



    return state == LinkState::Ready || state == LinkState::Busy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsWifiModemConnection
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsWifiModemConnection (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.transport == LinkTransport::WifiModem;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ApplyConnections
//
//  The preferences' links to the connections and the tree, and saved.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ApplyConnections()
{
    m_context.app.links->SetLinks (m_context.prefs.links);
    m_context.browser.GetTreeModel().SetLinks (m_context.app.links->BuildTreeEntries());

    m_context.window.SaveSession();
    m_context.treePane.RefreshTree();

    //  A link that was never started changes nothing that would refresh the
    //  other windows, and one of them can be on a link just removed.
    if (m_context.app.host != nullptr)
    {
        m_context.app.host->ForEachWindow ([] (CassoExplorerWindow & window) { window.GetCassoLinkManager().RefreshCassoLinkViews(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::HandleConnectionChanged
//
//  An CassoLink finished an operation or changed state. The links post to
//  one window, which runs the finished operations' completions and displays
//  their messages; every window then refreshes its own views.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::HandleConnectionChanged()
{
    std::vector<std::wstring>  messages;
    std::wstring               text;



    if (m_context.app.links == nullptr || !m_context.app.links->TakeChanged())
    {
        return;
    }

    messages = m_context.app.links->RunCompletions();
    m_context.app.links->ProbeDrives();

    if (m_context.app.host != nullptr)
    {
        m_context.app.host->ForEachWindow ([] (CassoExplorerWindow & window) { window.GetCassoLinkManager().RefreshCassoLinkViews(); });
    }
    else
    {
        RefreshCassoLinkViews();
    }

    for (const std::wstring & message : messages)
    {
        text += (text.empty() ? L"" : L"\n") + message;
    }

    if (!text.empty())
    {
        m_context.window.ShowMessage (text, MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::RefreshCassoLinkViews
//
//  The links' roots and drives are listed again where they are in the tree,
//  and the list and preview are refreshed when the location is on a link.
//
//  A CassoLink operation finished or a link changed state: the tree, the
//  list and the status bar are brought up to date.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::RefreshCassoLinkViews()
{
    HRESULT                    hr       = S_OK;
    Location                   location = m_context.browser.GetLocation();
    std::wstring               link;
    int                        slot     = 0;
    int                        drive    = 0;
    bool                       showing  = TreeModel::TryParseLinkId (location.path, link, slot, drive);
    std::wstring               jobs;



    if (m_context.app.links == nullptr)
    {
        return;
    }

    //  The running operation's progress and the queue after it; the status
    //  bar's own text returns once nothing is pending.
    jobs = CassoLinkConnections::DescribeJobs (m_context.app.links->GetPendingJobs());

    if (!jobs.empty())
    {
        m_context.statusBar.SetDetail (jobs);
        m_linkJobsShown = true;
    }
    else if (m_linkJobsShown)
    {
        m_linkJobsShown = false;
        m_context.statusBar.Fill();
    }

    m_context.browser.GetTreeModel().SetLinks (m_context.app.links->BuildTreeEntries());
    m_context.statusBar.SetActivity (GetLatestActivity());

    for (const CassoLinkSettings & settings : m_context.app.links->GetLinks())
    {
        std::wstring  id = TreeModel::MakeLinkId (TextEncoding::NarrowToWide (settings.name));

        for (const std::wstring & node : { id, TreeModel::kLinkCopyTag + id })
        {
            m_context.treePane.ReloadChildren (node);
        }
    }

    for (const std::wstring & node : { std::wstring (TreeModel::kCassoLinkRootId), TreeModel::kLinkCopyTag + std::wstring (TreeModel::kCassoLinkRootId) })
    {
        m_context.treePane.ReloadChildren (node);
    }

    //  A connection removed in another window has nothing to list. A drive of a
    //  machine that stopped answering has nothing either, so the list goes up
    //  to the machine, which has the reason.
    if (showing && IsConnectionRemoved (link))
    {
        m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::kThisPcRootId));
        showing = false;
    }
    else if (showing && drive != 0 && !IsConnectionReplying (TextEncoding::WideToNarrow (link)))
    {
        m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::MakeLinkId (link)));
    }

    showing = showing || m_context.browser.GetLocation().path == TreeModel::kCassoLinkRootId;

    //  The preview of a drive whose catalog was being read is drawn again
    //  with the catalog. A CHANGE TO ANY LINK CALLS THIS, NOT ONLY A CHANGE TO
    //  THE LINK THE LIST IS ON: a link with nothing to connect to changes state
    //  about once a second as it retries, so the list and the preview are
    //  redrawn only when the reload changed them, or each retry would scroll the
    //  preview to its top.
    if (showing)
    {
        std::vector<CatalogRow>  rows     = m_context.browser.GetRows();
        std::vector<int>         selected = m_context.browser.GetSelectedRows();
        std::wstring             message  = m_context.browser.GetListError();
        PreviewContent           preview  = m_context.browser.GetPreview();

        hr = m_context.browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_context.browser.RefreshPreview();

        //  Filling the list fills the preview as well.
        if (m_context.browser.GetRows() != rows || m_context.browser.GetSelectedRows() != selected || m_context.browser.GetListError() != message)
        {
            m_context.listPane.FillList();
        }
        else if (m_context.browser.GetPreview() != preview)
        {
            m_context.preview.FillPreview();
        }
    }

    m_context.window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::IsConnectionRemoved
//
//  True when no connection, saved or found running, has the link's name.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkManager::IsConnectionRemoved (const std::wstring & link) const
{
    const std::vector<CassoLinkSettings> &  links = m_context.app.links->GetLinks();



    return std::none_of (links.begin(), links.end(), [&link] (const CassoLinkSettings & settings) { return TextEncoding::NarrowToWide (settings.name) == link; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::HandleRunningChanged
//
//  A root whose Casso has exited is removed; when the list is on it, the
//  list goes to This PC.
//
//  The running Cassos read again, at start and when their folder changes.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::HandleRunningChanged()
{
    HRESULT                    hr      = S_OK;
    std::vector<RunningCasso>  running;
    std::wstring               link;
    int                        slot    = 0;
    int                        drive   = 0;
    bool                       showing = false;
    bool                       gone    = false;



    hr = m_context.app.running->Enumerate (running);
    CHR (hr);

    m_context.app.links->SetRunning (running);

    if (m_context.app.announced != nullptr)
    {
        m_announcedChanges = m_context.app.announced->GetChangeCount();
        m_context.app.links->SetAnnounced (m_context.app.announced->GetAnnounced());
    }

    showing = TreeModel::TryParseLinkId (m_context.browser.GetLocation().path, link, slot, drive);
    gone    = showing && IsConnectionRemoved (link);

    if (gone)
    {
        m_context.addressBar.GoToLocation (Location::MakeRoot (TreeModel::kThisPcRootId));
    }

    m_context.browser.GetTreeModel().SetLinks (m_context.app.links->BuildTreeEntries());
    m_context.treePane.RefreshTree();
    StartAdtProServers();

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::GetLatestActivity
//
//  The status bar's CassoLink line: the log's latest entry, for a quarter
//  of an hour after it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkManager::GetLatestActivity() const
{
    std::vector<CassoLinkConnections::Activity>  log;
    FILETIME                                     local = {};
    FILETIME                                     then  = {};
    FILETIME                                     now   = {};
    ULARGE_INTEGER                               a     = {};
    ULARGE_INTEGER                               b     = {};



    if (m_context.app.links == nullptr)
    {
        return std::wstring();
    }

    log = m_context.app.links->GetActivity();

    if (log.empty())
    {
        return std::wstring();
    }

    GetSystemTimeAsFileTime (&now);
    SystemTimeToFileTime (&log.back().time, &local);
    LocalFileTimeToFileTime (&local, &then);

    a.LowPart  = then.dwLowDateTime;
    a.HighPart = then.dwHighDateTime;
    b.LowPart  = now.dwLowDateTime;
    b.HighPart = now.dwHighDateTime;

    if (b.QuadPart > a.QuadPart + kActivityShownMinutes * 60ULL * 10'000'000ULL)
    {
        return std::wstring();
    }

    return std::format (L"CassoLink: {}: {}", log.back().connection, log.back().text);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::FormatActivity
//
//  The log's last `lines` entries, or every one for 0, oldest first, each
//  with its time.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkManager::FormatActivity (size_t lines) const
{
    std::vector<CassoLinkConnections::Activity>  log   = (m_context.app.links != nullptr) ? m_context.app.links->GetActivity() : std::vector<CassoLinkConnections::Activity>();
    size_t                                       first = (lines == 0 || log.size() <= lines) ? 0 : log.size() - lines;
    std::wstring                                 text;



    for (size_t i = first; i < log.size(); i++)
    {
        const CassoLinkConnections::Activity &  entry = log[i];

        text += std::format (L"{}{:04}-{:02}-{:02} {:02}:{:02}:{:02}  {}: {}", text.empty() ? L"" : L"\n",
                             entry.time.wYear, entry.time.wMonth, entry.time.wDay, entry.time.wHour, entry.time.wMinute, entry.time.wSecond,
                             entry.connection, entry.text);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkManager::ShowActivityLog
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkManager::ShowActivityLog()
{
    std::vector<CassoLinkConnections::Activity>  log = (m_context.app.links != nullptr) ? m_context.app.links->GetActivity() : std::vector<CassoLinkConnections::Activity>();
    HRESULT                                      hr  = S_OK;



    hr = CassoLinkActivityDialog::Show (m_context.window.GetHwnd(), m_context.theme, log);
    IGNORE_RETURN_VALUE (hr, S_OK);
}

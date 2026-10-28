#include "Pch.h"

#include "CassoExplorer/Window/ExplorerLinks.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "Seams/Win32FirewallHelper.h"
#include "Net/FirewallText.h"
#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Core/TextEncoding.h"





//  The window whose m_cassetteWav is the sound in progress. PlaySound reads
//  from that memory, so that window stops the sound before the memory is
//  freed, and no other window stops it.
static const CassoExplorerWindow *  s_cassettePlayer = nullptr;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::ExplorerLinks
//
////////////////////////////////////////////////////////////////////////////////

ExplorerLinks::ExplorerLinks (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::CreateListActions
//
//  The buttons under the list's message for a link: bootstrap the Apple or
//  connect to it, or allow it through Windows Firewall on private or on all
//  networks.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::CreateListActions()
{
    m_listAction = m_window.CreateChild<DxuiButton> (L"Bootstrap the Apple");
    m_listAction->SetVisible (false);
    m_listAction->SetOnClick ([this] ()
    {
        std::string  link = m_listActionLink;

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
            ConnectLink (link);
        }
        else
        {
            BootstrapLink (link);
        }
    });

    m_listActionAlt = m_window.CreateChild<DxuiButton> (FirewallText::kPrivateAction);
    m_listActionAlt->SetVisible (false);
    m_listActionAlt->SetOnClick ([this] () { FixFirewall (true); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::LayoutListActions
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::LayoutListActions()
{
    m_listAction->Layout    (GetListActionRect(),    m_window.m_scaler);
    m_listActionAlt->Layout (GetListActionAltRect(), m_window.m_scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::OnListActionMouse
//
//  Each button has every move, so its hover ends as the pointer moves off
//  it, and the presses inside it. Returns whether the event was over one.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLinks::OnListActionMouse (const DxuiMouseEvent & ev)
{
    for (DxuiButton * action : { m_listAction, m_listActionAlt })
    {
        bool  inside = action->IsVisible() && CassoExplorerWindow::Contains (action->GetBounds(), ev.positionDip);

        if (action->IsVisible() && (ev.kind == DxuiMouseEventKind::Move || inside))
        {
            action->OnMouse (ev);
            m_window.Invalidate();
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
//  ExplorerLinks::~ExplorerLinks
//
////////////////////////////////////////////////////////////////////////////////

ExplorerLinks::~ExplorerLinks()
{
    if (s_cassettePlayer == &m_window)
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
//  ExplorerLinks::ReadDiskToImage
//
//  The open Apple drive's disk into an image file the user picks, in the
//  format of its extension. When sectors cannot be read, the user may read
//  the disk again with them written as zeros.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::ReadDiskToImage (bool blankUnreadable, const std::wstring & imagePath)
{
    HRESULT                        hr     = S_OK;
    std::filesystem::path          picked = imagePath;
    bool                           chosen = !imagePath.empty();
    FileDialogSpec                 spec;
    CassoExplorerActions::Outcome  outcome;



    OpenSelectedLinkDrive();
    BAIL_OUT_IF (!m_window.m_browser.IsAppleDriveOpen(), S_OK);

    spec.filters          = { FileDialogFilter { L"Disk images (*.dsk, *.do, *.po, *.woz, *.nib)", L"*.dsk;*.do;*.po;*.woz;*.nib" },
                              FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid       = s_kLinkDiskPickerGuid;
    spec.defaultFileName  = L"disk.dsk";
    spec.defaultExtension = L"dsk";
    spec.askToReplace     = false;
    spec.title            = std::format (L"Create image from {}", m_window.m_actions.DescribeAppleDrive());

    //  The picker's own replace prompt is Win32's; this one follows the theme,
    //  and No goes back to the picker.
    while (!chosen)
    {
        hr = m_window.m_dialogs.PickFileToSave (m_window.GetHwnd(), spec, picked, chosen);
        CHR (hr);
        BAIL_OUT_IF (!chosen, S_OK);

        if (GetFileAttributesW (picked.c_str()) != INVALID_FILE_ATTRIBUTES &&
            DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme, (picked.filename().wstring() + L" already exists. Overwrite it?").c_str(),
                            L"Confirm save as", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK)
        {
            spec.initialFolder   = picked.parent_path();
            spec.defaultFileName = picked.filename().wstring();
            chosen               = false;
        }
    }

    //  The second read goes to the link and drive the first one read, not to
    //  whatever drive the window has open by the time the first one fails.
    outcome = m_window.m_actions.ReadDiskToImage (picked.wstring(), blankUnreadable, [this, picked] (const LinkOperation & operation, const std::string & link, int drive)
    {
        std::wstring                   list    = TextEncoding::NarrowToWide (CassoLinkDiskTransfer::GetUnreadableListPath (TextEncoding::WideToNarrow (picked.filename().wstring())));
        std::wstring                   message = TextEncoding::NarrowToWide (operation.message) + L"\n\nRead the disk again, writing those sectors as zeros? Their list is saved beside the image as " + list + L".";
        CassoExplorerActions::Outcome  again;

        if (DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme, message.c_str(), L"Read disk to image",
                            MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Read again", L"Cancel" }) == IDOK)
        {
            again = m_window.m_actions.ReadDiskToImage (link, drive, picked.wstring(), true, nullptr);

            if (!again.Succeeded())
            {
                m_window.ShowMessage (again.message, MB_ICONERROR);
            }

            OnLinkChanged();
        }
    });

    if (!outcome.Succeeded())
    {
        m_window.ShowMessage (outcome.message, MB_ICONERROR);
    }

    OnLinkChanged();

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::WriteImageToDisk
//
//  An image file onto the open Apple drive's disk, once the user confirms,
//  since everything on the disk is replaced.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::WriteImageToDisk()
{
    HRESULT                hr     = S_OK;
    std::filesystem::path  picked;
    bool                   chosen = false;
    FileDialogSpec         spec;



    OpenSelectedLinkDrive();
    BAIL_OUT_IF (!m_window.m_browser.IsAppleDriveOpen(), S_OK);

    spec.filters    = { FileDialogFilter { L"Disk images (*.dsk, *.do, *.po, *.woz, *.nib)", L"*.dsk;*.do;*.po;*.woz;*.nib" },
                        FileDialogFilter { L"All files (*.*)", L"*.*" } };
    spec.clientGuid = s_kLinkDiskPickerGuid;
    spec.title      = std::format (L"Write image to {}", m_window.m_actions.DescribeAppleDrive());

    hr = m_window.m_dialogs.PickFileToOpen (m_window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    StartImageWrite (m_window.m_browser.GetLocation(), picked.wstring());

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::StartImageWrite
//
//  The drive's boot sector is read first, so the one confirmation can warn
//  when the disk is the link disk the Apple started from: writing over it
//  does not end the link, but no disk is left to start it from again.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::StartImageWrite (const Location & drive, const std::wstring & imagePath)
{
    HRESULT       hr     = S_OK;
    std::wstring  link;
    int           slot   = 0;
    int           number = 0;
    std::wstring  title  = m_window.m_browser.GetTreeModel().GetLinkDriveTitle (drive.path);
    std::wstring  file   = std::filesystem::path (imagePath).filename().wstring();
    std::string   name;



    if (!TreeModel::TryParseLinkId (drive.path, link, slot, number) || number == 0)
    {
        return;
    }

    name = TextEncoding::WideToNarrow (link);

    hr = m_window.m_actions.CheckLinkDisk (name, number, [this, name, number, title, file, imagePath] (bool linkDisk, HRESULT result)
    {
        std::wstring                   message;
        CassoExplorerActions::Outcome  outcome;

        if (FAILED (result))
        {
            m_window.ShowMessage (std::format (L"The disk in {} could not be read, so it was not written.", title), MB_ICONERROR);
            return;
        }

        message = linkDisk
                ? std::format (L"{} has the CassoLink disk this Apple started from. Overwriting it does not end the connection, "
                               L"but the Apple cannot start CassoLink from that disk again until you choose Write CassoLink to disk."
                               L"\n\nOverwrite {} with {}?", title, title, file)
                : std::format (L"Overwrite {} with {}?\n\nEverything on that disk is replaced.", title, file);

        if (DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme, message.c_str(), L"Write image",
                            MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK)
        {
            return;
        }

        outcome = m_window.m_actions.WriteImageToAppleDrive (name, number, imagePath);

        if (!outcome.Succeeded())
        {
            m_window.ShowMessage (outcome.message, MB_ICONERROR);
        }
    });

    if (FAILED (hr))
    {
        m_window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
    }

    OnLinkChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::TryGetSelectedLinkDrive
//
//  A drive row's path is the drive's id.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLinks::TryGetSelectedLinkDrive (Location & outDrive) const
{
    const std::vector<int>  & selected = m_window.m_browser.GetSelectedRows();
    std::wstring              link;
    std::wstring              id;
    int                       slot     = 0;
    int                       drive    = 0;
    bool                      found    = IsOnLinkedMachine() && selected.size() == 1 && selected[0] >= 0 && (size_t) selected[0] < m_window.m_browser.GetRows().size();



    if (found)
    {
        id    = m_window.m_browser.GetRows()[(size_t) selected[0]].hostPath;
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
//  ExplorerLinks::IsOnLinkedMachine
//
//  The list on a linked Apple, which lists its drives, rather than on one of
//  the drives.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLinks::IsOnLinkedMachine() const
{
    Location      shown = m_window.m_browser.GetLocation();
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    return shown.kind == Location::Kind::Root && TreeModel::TryParseLinkId (shown.path, link, slot, drive) && drive == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::OpenSelectedLinkDrive
//
//  The image commands run on the open drive, so a drive selected on its
//  machine is opened first, as its context menu opens it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::OpenSelectedLinkDrive()
{
    Location  drive;



    if (!m_window.m_browser.IsAppleDriveOpen() && TryGetSelectedLinkDrive (drive))
    {
        m_window.m_addressBar.GoToLocation (drive);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::SaveLinkToDisk
//
//  The link's boot disk onto the open Apple drive, once the user confirms,
//  since everything on the disk is replaced.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::SaveLinkToDisk()
{
    wchar_t                        temp[MAX_PATH] = {};
    std::wstring                   message;
    CassoExplorerActions::Outcome  outcome;



    if (!m_window.m_browser.IsAppleDriveOpen() || GetTempPathW (MAX_PATH, temp) == 0)
    {
        return;
    }

    message = std::format (L"Overwrite {} with CassoLink?\n\nEverything on the disk is replaced, and the Apple then starts CassoLink when it boots from the disk.",
                           m_window.m_actions.DescribeAppleDrive());

    if (DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme, message.c_str(), L"Write CassoLink to disk",
                        MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Overwrite", L"Cancel" }) != IDOK)
    {
        return;
    }

    outcome = m_window.m_actions.SaveLinkToDisk (std::wstring (temp).substr (0, wcslen (temp) - 1));

    if (!outcome.Succeeded())
    {
        m_window.ShowMessage (outcome.message, MB_ICONERROR);
    }

    OnLinkChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::FormatAppleDisk
//
//  A linked drive's disk formatted, from the format dialog's choices, once
//  the user confirms that everything on it is erased. The list stays where
//  it is; the drive's row shows the new disk when the write finishes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::FormatAppleDisk (const Location & location)
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

    drive  = m_window.m_browser.GetTreeModel().GetLinkDriveTitle (location.path);
    choice = CassoExplorerNewDiskDialog::Ask (m_window.GetHwnd(), m_window.m_theme, true, {}, CassoExplorerNewDiskChoices::kFormatDos33, drive, true);

    if (!choice.confirmed)
    {
        return;
    }

    message = std::format (L"Format the disk in {}? Everything on it will be erased.", drive);

    if (DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme, message.c_str(), L"Format", MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2, { L"Format", L"Cancel" }) != IDOK)
    {
        return;
    }

    outcome = m_window.m_actions.FormatAppleDrive (TextEncoding::WideToNarrow (link), number, choice.request, std::wstring (temp).substr (0, wcslen (temp) - 1));

    if (!outcome.Succeeded())
    {
        m_window.ShowMessage (outcome.message, MB_ICONERROR);
    }

    OnLinkChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::SaveLinkDiskImage
//
//  The link's boot disk as an image file on the PC, for an emulator, at the
//  link's speed.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::SaveLinkDiskImage (const std::string & name)
{
    HRESULT                        hr     = S_OK;
    std::filesystem::path          picked;
    bool                           chosen = false;
    FileDialogSpec                 spec;
    CassoLinkSettings              link;
    CassoExplorerActions::Outcome  outcome;



    for (const CassoLinkSettings & settings : m_window.m_context.links->GetLinks())
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

    hr = m_window.m_dialogs.PickFileToSave (m_window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    outcome = m_window.m_actions.SaveLinkDiskImage (picked.wstring(), link);

    if (!outcome.Succeeded())
    {
        m_window.ShowMessage (outcome.message, MB_ICONERROR);
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::GetListActionRect
//
//  Centered under the list's message, which is centered in the list.
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerLinks::GetListActionRect() const
{
    RECT  message = m_window.m_listPane.GetListMessageRect();
    bool  pair    = m_listActionAlt->IsVisible();
    int   width   = m_window.m_scaler.ToPx (m_listActionFirewall ? kListActionWideDip : kListActionWidthDip);
    int   gap     = m_window.m_scaler.ToPx (kListActionGapDip);
    int   height  = m_window.m_scaler.ToPx (kListActionHeightDip);
    int   span    = pair ? width * 2 + gap : width;
    int   left    = (message.left + message.right - span) / 2;
    int   top     = m_listActionFirewall ? message.bottom + gap : (message.top + message.bottom) / 2 + m_window.m_scaler.ToPx (kListActionOffsetDip);



    return RECT { left, top, left + width, top + height };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::GetListActionAltRect
//
//  Right of the first button, the two centered as a pair.
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerLinks::GetListActionAltRect() const
{
    RECT  first = GetListActionRect();
    int   gap   = m_window.m_scaler.ToPx (kListActionGapDip);
    int   width = first.right - first.left;



    return RECT { first.right + gap, first.top, first.right + gap + width, first.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::FillListAction
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::FillListAction (bool hasMessage)
{
    std::wstring   name;
    int            slot   = 0;
    int            drive  = 0;
    std::string    disconnected;
    LinkTreeEntry  entry;
    bool           onLink = hasMessage && m_window.m_context.links != nullptr && TreeModel::TryParseLinkId (m_window.m_browser.GetLocation().path, name, slot, drive) && drive == 0;



    if (onLink && m_window.m_context.links->IsHeld (TextEncoding::WideToNarrow (name)))
    {
        disconnected = TextEncoding::WideToNarrow (name);
    }

    //  The firewall first: until the port is allowed, no connection the other
    //  actions start arrives at this PC.
    m_listActionFirewall = onLink && disconnected.empty() && TryGetLinkEntry (name, entry) && !entry.firewall.empty();
    m_listActionConnects          = !m_listActionFirewall && !disconnected.empty();
    m_listActionLink              = m_listActionFirewall ? std::string() : m_listActionConnects ? disconnected : hasMessage ? GetBootstrapLink() : std::string();

    m_window.m_listPane.AlignMessage (m_listActionFirewall);
    m_listAction->SetLabel      (m_listActionFirewall ? FirewallText::kAllowAction : m_listActionConnects ? L"Connect" : L"Bootstrap the Apple");
    m_listActionAlt->SetVisible (m_listActionFirewall && entry.firewallPublic);
    m_listAction->Layout        (GetListActionRect(), m_window.m_scaler);
    m_listActionAlt->Layout     (GetListActionAltRect(), m_window.m_scaler);
    m_listAction->SetVisible    (m_listActionFirewall || !m_listActionLink.empty());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::TryGetLinkEntry
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLinks::TryGetLinkEntry (const std::wstring & name, LinkTreeEntry & outEntry) const
{
    const std::vector<LinkTreeEntry>  & links = m_window.m_browser.GetTreeModel().GetLinks();
    auto                                found = std::find_if (links.begin(), links.end(), [&name] (const LinkTreeEntry & link) { return link.name == name; });



    if (found != links.end())
    {
        outEntry = *found;
    }

    return found != links.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::PollFirewall
//
//  Every window polls the one monitor, so each compares the monitor's version
//  with the one it drew rather than relying on Update's result, which only
//  the first window to poll after a change would see.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::PollFirewall()
{
    HRESULT       result  = S_OK;
    std::wstring  message;



    if (m_window.m_context.firewall == nullptr)
    {
        return;
    }

    //  The helper changed the rules, which no notification reports.
    if (m_firewallHelper != nullptr && Win32FirewallHelper::TryFinish (m_firewallHelper, result))
    {
        m_window.m_context.firewall->Invalidate();

        if (FAILED (result))
        {
            message = std::format (L"Windows Firewall could not be changed: {}", ChangePrompt::DescribeError (result));
            m_window.ShowMessage (message, MB_ICONWARNING);
        }
    }

    m_window.m_context.firewall->Update();

    if (m_window.m_context.firewall->GetVersion() != m_firewallVersion)
    {
        m_firewallVersion = m_window.m_context.firewall->GetVersion();
        RefreshLinkViews();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::FixFirewall
//
//  Every connection's port is in the plan, not only the one the list is on,
//  so one UAC prompt allows them all. Declining the prompt changes nothing
//  and is not an error.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::FixFirewall (bool markPrivate)
{
    HRESULT                    hr = S_OK;
    std::vector<FirewallPort>  ports;
    FirewallPort               port;
    FirewallPlan               plan;



    BAIL_OUT_IF (m_window.m_context.firewall == nullptr || m_window.m_context.links == nullptr || m_firewallHelper != nullptr, S_OK);

    for (const CassoLinkSettings & settings : m_window.m_context.links->GetLinks())
    {
        if (CassoLinkConnections::GetInboundPort (settings, port) && std::find (ports.begin(), ports.end(), port) == ports.end())
        {
            ports.push_back (port);
        }
    }

    plan = FirewallRemedy::MakePlan (m_window.m_context.firewall->GetSnapshot(), m_window.m_context.firewall->GetProgram(), CassoLinkConnections::kFirewallTitle, ports, markPrivate);
    BAIL_OUT_IF (plan.ports.empty() && !plan.markPrivate, S_OK);

    hr = Win32FirewallHelper::Launch (m_window.GetHwnd(), plan, m_firewallHelper);
    BAIL_OUT_IF (hr == HRESULT_FROM_WIN32 (ERROR_CANCELLED), S_OK);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        m_window.ShowMessage (std::format (L"Windows Firewall could not be changed: {}", ChangePrompt::DescribeError (hr)), MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::GetBootstrapLink
//
//  The serial link the list has, when its Apple is not replying and no
//  bootstrap is under way; empty otherwise.
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerLinks::GetBootstrapLink() const
{
    std::wstring        name;
    int                 slot    = 0;
    int                 drive   = 0;
    std::string         link;
    CassoLinkSession  * session = nullptr;
    LinkState           state   = LinkState::Closed;
    bool                waiting = false;



    if (m_window.m_context.links == nullptr || !TreeModel::TryParseLinkId (m_window.m_browser.GetLocation().path, name, slot, drive) || drive != 0)
    {
        return std::string();
    }

    link    = TextEncoding::WideToNarrow (name);
    session = m_window.m_context.links->GetSession (link);
    state   = (session != nullptr) ? session->GetState() : LinkState::Closed;
    waiting = IsSerialLink (link) && !m_window.m_browser.GetTreeModel().IsLinkReplying (name)
              && state != LinkState::Bootstrapping && state != LinkState::LoadingProgram;

    return waiting ? link : std::string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::EditLink
//
//  An empty name adds a link. An edited link is stopped, as its settings
//  are no longer the ones its port was opened with.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::EditLink (const std::string & name)
{
    HRESULT                           hr      = S_OK;
    CassoLinkSettings                 initial;
    std::vector<std::wstring>         others;
    std::vector<std::wstring>         ports;
    CassoExplorerLinkDialog::Outcome  outcome;
    auto                              isNamed = [&name] (const CassoLinkSettings & link) { return link.name == name; };
    auto                              found   = std::find_if (m_window.m_prefs.links.begin(), m_window.m_prefs.links.end(), isNamed);



    if (found != m_window.m_prefs.links.end())
    {
        initial = *found;
    }

    for (const CassoLinkSettings & link : m_window.m_prefs.links)
    {
        if (link.name != name)
        {
            others.push_back (TextEncoding::NarrowToWide (link.name));
        }
    }

    //  With no ports listed, the drop-down has Any alone.
    if (m_window.m_context.links != nullptr)
    {
        hr = m_window.m_context.links->ListComPorts (ports);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    outcome = CassoExplorerLinkDialog::Ask (m_window.GetHwnd(), m_window.m_theme, initial, others, ports, found == m_window.m_prefs.links.end(),
                                            [this] (HWND owner, std::wstring & inOutFolder) -> bool
    {
        std::filesystem::path  picked;
        bool                   chosen = false;
        HRESULT                result = m_window.m_dialogs.PickFolder (owner, picked, chosen);

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
    found = std::find_if (m_window.m_prefs.links.begin(), m_window.m_prefs.links.end(), isNamed);

    if (found != m_window.m_prefs.links.end())
    {
        *found = outcome.settings;
        ApplyLinks();
        return;
    }

    m_window.m_prefs.links.push_back (outcome.settings);
    ApplyLinks();

    //  A new connection is selected in the tree, as a new folder is.
    m_window.m_addressBar.GoToLocation (Location::MakeRoot (TreeModel::MakeLinkId (TextEncoding::NarrowToWide (outcome.settings.name))));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::RemoveLink
//
//  A tab on the link goes to This PC.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::RemoveLink (const std::string & name)
{
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    std::erase_if (m_window.m_prefs.links, [&name] (const CassoLinkSettings & settings) { return settings.name == name; });

    if (TreeModel::TryParseLinkId (m_window.m_browser.GetLocation().path, link, slot, drive) && TextEncoding::WideToNarrow (link) == name)
    {
        m_window.m_addressBar.GoToLocation (Location::MakeRoot (TreeModel::kThisPcRootId));
    }

    ApplyLinks();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::DisconnectLink
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::DisconnectLink (const std::string & name)
{
    m_window.m_context.links->Disconnect (name);
    OnLinkChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::SearchLink
//
//  The link is started again, searching the COM ports for the Apple; the
//  link's root reports the search and then what it found.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::SearchLink (const std::string & name)
{
    HRESULT  hr = S_OK;



    m_window.m_context.links->Stop (name);

    hr = m_window.m_context.links->Start (name, true);

    if (FAILED (hr))
    {
        m_window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
    }

    OnLinkChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::BootstrapLink
//
//  The user readies the Apple first; the loader is then typed into it
//  over the link.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::BootstrapLink (const std::string & name)
{
    HRESULT       hr      = S_OK;
    std::wstring  steps;
    int           answer  = IDCANCEL;



    steps  = L"On the Apple, at the prompt, type:\n\n"
             L"1. **IN#2** on a //c, or IN# and the Super Serial Card's slot, then press Return\n"
             L"2. **Ctrl+A 14B**, then press Return\n\n"
             L"The loader is then typed into the Apple, which takes about 40 seconds.";
    answer = DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme, steps.c_str(), L"Bootstrap the Apple",
                             MB_OKCANCEL | MB_ICONINFORMATION, { L"Bootstrap", L"Cancel" });

    if (answer != IDOK)
    {
        return;
    }

    hr = m_window.m_context.links->Bootstrap (name, [] (const LinkOperation & operation) -> std::wstring
    {
        return FAILED (operation.result) ? CassoLinkConnections::DescribeFailure (operation) : std::wstring();
    });

    if (FAILED (hr))
    {
        m_window.ShowMessage (L"CassoLink could not be started.", MB_ICONERROR);
    }

    OnLinkChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::PlayCassetteBootstrap
//
//  The shim as audio from the PC's sound output into a ][+ or //e's
//  cassette input. The user readies the monitor's read
//  first. The link is started while the audio is sent, so its session is
//  open when the shim sends Ready.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::PlayCassetteBootstrap (const std::string & name)
{
    HRESULT            hr      = S_OK;
    CassoLinkSettings  link;
    std::string        command;
    std::wstring       steps;
    int                answer  = IDCANCEL;
    BOOL               played  = FALSE;



    for (const CassoLinkSettings & settings : m_window.m_context.links->GetLinks())
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
    answer = DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme, steps.c_str(), L"Bootstrap from the cassette input",
                             MB_OKCANCEL | MB_ICONINFORMATION, { L"Send", L"Cancel" });
    BAIL_OUT_IF (answer != IDOK, S_OK);

    played = PlaySoundW (reinterpret_cast<LPCWSTR> (m_cassetteWav.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    CBR (played);

    s_cassettePlayer = &m_window;

    if (!m_window.m_context.links->IsStarted (name))
    {
        ConnectLink (name);
    }

Error:
    if (FAILED (hr))
    {
        m_window.ShowMessage (L"The cassette bootstrap could not be sent to this PC's sound output.", MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::SaveCassetteBootstrap
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::SaveCassetteBootstrap (const std::string & name)
{
    HRESULT                        hr     = S_OK;
    std::filesystem::path          picked;
    bool                           chosen = false;
    FileDialogSpec                 spec;
    CassoLinkSettings              link;
    std::string                    command;
    CassoExplorerActions::Outcome  outcome;



    for (const CassoLinkSettings & settings : m_window.m_context.links->GetLinks())
    {
        link = (settings.name == name) ? settings : link;
    }

    spec.filters          = { FileDialogFilter { L"Sound files (*.wav)", L"*.wav" } };
    spec.clientGuid       = s_kLinkDiskPickerGuid;
    spec.defaultFileName  = L"CassoLink.wav";
    spec.defaultExtension = L"wav";
    spec.title            = L"Save cassette bootstrap";

    hr = m_window.m_dialogs.PickFileToSave (m_window.GetHwnd(), spec, picked, chosen);
    CHR (hr);
    BAIL_OUT_IF (!chosen, S_OK);

    outcome = m_window.m_actions.SaveCassetteBootstrap (picked.wstring(), link, command);

    if (!outcome.Succeeded())
    {
        m_window.ShowMessage (outcome.message, MB_ICONERROR);
    }
    else
    {
        command.pop_back();
        m_window.ShowMessage (std::format (L"To load it, type CALL -151 and then {} on the Apple, and play the file into its cassette input.",
                                           TextEncoding::NarrowToWide (command)), MB_ICONINFORMATION);
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::ReleaseIdleLinks
//
//  A link connects only while its machine, or one of its drives, is the
//  location of some window. One that has not connected by the time every
//  window has gone elsewhere is stopped, closing its port; one that has
//  connected, or is bootstrapping or loading the program, stays.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::ReleaseIdleLinks()
{
    std::wstring              current;
    int                       slot    = 0;
    int                       drive   = 0;
    std::vector<std::string>  release;



    if (m_window.m_context.links == nullptr)
    {
        return;
    }

    TreeModel::TryParseLinkId (m_window.m_browser.GetLocation().path, current, slot, drive);
    m_window.m_context.links->SetViewing (&m_window, TextEncoding::WideToNarrow (current));

    for (const CassoLinkSettings & settings : m_window.m_context.links->GetLinks())
    {
        CassoLinkSession  * session = m_window.m_context.links->GetSession (settings.name);
        LinkState           state   = session != nullptr ? session->GetState() : LinkState::Closed;
        bool                connected = state == LinkState::Ready || state == LinkState::Busy
                                     || state == LinkState::Bootstrapping || state == LinkState::LoadingProgram;

        //  ADTPro's client speaks first, so an ADTPro server stays open for it.
        if (session != nullptr && !connected && settings.transport != LinkTransport::AdtProServer && !m_window.m_context.links->IsViewed (settings.name))
        {
            release.push_back (settings.name);
        }
    }

    for (const std::string & name : release)
    {
        m_window.m_context.links->Stop (name);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::ConnectLink
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::ConnectLink (const std::string & name)
{
    HRESULT  hr = m_window.m_context.links->Start (name);



    IGNORE_RETURN_VALUE (hr, S_OK);
    OnLinkChanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::IsSerialLink
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLinks::IsSerialLink (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_window.m_context.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.transport == LinkTransport::Serial;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::CanBootstrapLink
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLinks::CanBootstrapLink (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_window.m_context.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.CanBootstrap();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::AddLinkRows
//
//  A linked machine's settings, its port and the machine itself; a drive's
//  whole disk; and CassoLink's Add a CassoLink machine. The tree's node and
//  the list's row have the same rows.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::AddLinkRows (std::vector<DxuiPopupMenuItem> & items, bool hasLocation, const Location & location)
{
    std::wstring  link;
    int           slot  = 0;
    int           drive = 0;



    if (m_window.m_context.links != nullptr && hasLocation && location.kind == Location::Kind::Root && TreeModel::TryParseLinkId (location.path, link, slot, drive))
    {
        std::string  name = TextEncoding::WideToNarrow (link);

        bool  saved = !m_window.m_context.links->IsDiscovered (name);

        if (saved)
        {
            m_window.m_menus.AddMenuCommand (items, L"&Edit connection...", [this, name]() { EditLink (name); });
        }

        if (m_window.m_context.links->IsStarted (name))
        {
            m_window.m_menus.AddMenuCommand (items, L"&Disconnect", [this, name]() { DisconnectLink (name); });
        }
        else
        {
            m_window.m_menus.AddMenuCommand (items, L"&Connect", [this, name]() { ConnectLink (name); });
        }

        if (IsSerialLink (name))
        {
            m_window.m_menus.AddMenuCommand (items, L"&Search COM ports", [this, name]() { SearchLink (name); });
        }

        //  Only an Apple on a serial card can be started from here.
        if (CanBootstrapLink (name))
        {
            m_window.m_menus.AddMenuCommand (items, L"&Bootstrap the Apple...", [this, name]() { BootstrapLink (name); });
            m_window.m_menus.AddMenuCommand (items, L"Bootstrap from the c&assette input...", [this, name]() { PlayCassetteBootstrap (name); });
            m_window.m_menus.AddMenuCommand (items, L"Save cassette boo&tstrap...", [this, name]() { SaveCassetteBootstrap (name); });
        }

        //  A boot disk is built for its link: at its speed, a Wi-Fi modem's
        //  dialing this PC, and an Uthernet II's with the link's address for
        //  when no DHCP reply comes.
        m_window.m_menus.AddMenuCommand (items, L"Save CassoLink disk &image...", [this, name]() { SaveLinkDiskImage (name); });

        if (saved)
        {
            m_window.m_menus.AddMenuCommand (items, L"Re&move connection", [this, name]() { RemoveLink (name); });
        }
    }
    else if (m_window.m_context.links != nullptr && hasLocation && location.kind == Location::Kind::AppleDrive && location.innerPath.empty())
    {
        //  A drive's whole disk, read or written from its node.
        std::wstring  title = m_window.m_browser.GetTreeModel().GetLinkDriveTitle (location.path);

        m_window.m_menus.AddMenuCommand (items, std::format (L"Create &image from {}...", title).c_str(), [this, location]() { m_window.m_addressBar.GoToLocation (location); ReadDiskToImage(); });
        m_window.m_menus.AddMenuCommand (items, std::format (L"&Write image to {}...", title).c_str(), [this, location]() { m_window.m_addressBar.GoToLocation (location); WriteImageToDisk(); });
        m_window.m_menus.AddMenuCommand (items, L"Write &CassoLink to disk...", [this, location]() { m_window.m_addressBar.GoToLocation (location); SaveLinkToDisk(); });
        m_window.m_menus.AddMenuCommand (items, L"&Format disk...",            [this, location]() { FormatAppleDisk (location); });
    }

    if (m_window.m_context.links != nullptr && hasLocation && location.kind == Location::Kind::Root && location.path == TreeModel::kCassoLinkRootId)
    {
        m_window.m_menus.AddMenuCommand (items, L"&Add a CassoLink connection...", [this]() { EditLink (std::string()); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::StartAdtProServers
//
//  ADTPro's client speaks first, so each ADTPro-compatible server is open
//  from the start, and from when it is added, unless the user disconnected
//  it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::StartAdtProServers()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_window.m_context.links == nullptr, S_OK);

    for (const CassoLinkSettings & settings : m_window.m_context.links->GetLinks())
    {
        bool  idle = settings.transport == LinkTransport::AdtProServer && !m_window.m_context.links->IsStarted (settings.name) && !m_window.m_context.links->IsHeld (settings.name);

        if (idle)
        {
            hr = m_window.m_context.links->Start (settings.name);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::IsLinkReplying
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLinks::IsLinkReplying (const std::string & name) const
{
    CassoLinkSession  * session = m_window.m_context.links->GetSession (name);
    LinkState           state   = (session != nullptr) ? session->GetState() : LinkState::Closed;



    return state == LinkState::Ready || state == LinkState::Busy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::IsWifiModemLink
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLinks::IsWifiModemLink (const std::string & name) const
{
    const std::vector<CassoLinkSettings> &  links = m_window.m_context.links->GetLinks();



    return std::any_of (links.begin(), links.end(), [&name] (const CassoLinkSettings & settings)
    {
        return settings.name == name && settings.transport == LinkTransport::WifiModem;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::ApplyLinks
//
//  The preferences' links to the connections and the tree, and saved.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::ApplyLinks()
{
    m_window.m_context.links->SetLinks (m_window.m_prefs.links);
    m_window.m_browser.GetTreeModel().SetLinks (m_window.m_context.links->BuildTreeEntries());

    m_window.SaveSession();
    m_window.m_treePane.RefreshTree();

    //  A link that was never started changes nothing that would refresh the
    //  other windows, and one of them can be on a link just removed.
    if (m_window.m_context.host != nullptr)
    {
        m_window.m_context.host->ForEachWindow ([] (CassoExplorerWindow & window) { window.m_links.RefreshLinkViews(); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::OnLinkChanged
//
//  An CassoLink finished an operation or changed state. The links post to
//  one window, which runs the finished operations' completions and displays
//  their messages; every window then refreshes its own views.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::OnLinkChanged()
{
    std::vector<std::wstring>  messages;
    std::wstring               text;



    if (m_window.m_context.links == nullptr || !m_window.m_context.links->TakeChanged())
    {
        return;
    }

    messages = m_window.m_context.links->RunCompletions();
    m_window.m_context.links->ProbeDrives();

    if (m_window.m_context.host != nullptr)
    {
        m_window.m_context.host->ForEachWindow ([] (CassoExplorerWindow & window) { window.m_links.RefreshLinkViews(); });
    }
    else
    {
        RefreshLinkViews();
    }

    for (const std::wstring & message : messages)
    {
        text += (text.empty() ? L"" : L"\n") + message;
    }

    if (!text.empty())
    {
        m_window.ShowMessage (text, MB_ICONERROR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::RefreshLinkViews
//
//  The links' roots and drives are listed again where they are in the tree,
//  and the list and preview are refreshed when the location is on a link.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::RefreshLinkViews()
{
    HRESULT                    hr       = S_OK;
    Location                   location = m_window.m_browser.GetLocation();
    std::wstring               link;
    int                        slot     = 0;
    int                        drive    = 0;
    bool                       showing  = TreeModel::TryParseLinkId (location.path, link, slot, drive);
    std::wstring               jobs;



    if (m_window.m_context.links == nullptr)
    {
        return;
    }

    //  The running operation's progress and the queue after it; the status
    //  bar's own text returns once nothing is pending.
    jobs = CassoLinkConnections::DescribeJobs (m_window.m_context.links->GetPendingJobs());

    if (!jobs.empty())
    {
        m_window.m_status->SetText (ExplorerLayout::kStatusDetail, jobs);
        m_window.m_linkJobsShown = true;
    }
    else if (m_window.m_linkJobsShown)
    {
        m_window.m_linkJobsShown = false;
        m_window.m_layout.FillStatus();
    }

    m_window.m_browser.GetTreeModel().SetLinks (m_window.m_context.links->BuildTreeEntries());
    m_window.m_status->SetText (ExplorerLayout::kStatusFill, GetLatestActivity());

    for (const CassoLinkSettings & settings : m_window.m_context.links->GetLinks())
    {
        std::wstring  id = TreeModel::MakeLinkId (TextEncoding::NarrowToWide (settings.name));

        for (const std::wstring & node : { id, TreeModel::kLinkCopyTag + id })
        {
            m_window.m_treePane.ReloadChildren (node);
        }
    }

    for (const std::wstring & node : { std::wstring (TreeModel::kCassoLinkRootId), TreeModel::kLinkCopyTag + std::wstring (TreeModel::kCassoLinkRootId) })
    {
        m_window.m_treePane.ReloadChildren (node);
    }

    //  A connection removed in another window has nothing to list. A drive of a
    //  machine that stopped answering has nothing either, so the list goes up
    //  to the machine, which has the reason.
    if (showing && IsLinkRemoved (link))
    {
        m_window.m_addressBar.GoToLocation (Location::MakeRoot (TreeModel::kThisPcRootId));
        showing = false;
    }
    else if (showing && drive != 0 && !IsLinkReplying (TextEncoding::WideToNarrow (link)))
    {
        m_window.m_addressBar.GoToLocation (Location::MakeRoot (TreeModel::MakeLinkId (link)));
    }

    showing = showing || m_window.m_browser.GetLocation().path == TreeModel::kCassoLinkRootId;

    //  The preview of a drive whose catalog was being read is drawn again
    //  with the catalog. A CHANGE TO ANY LINK CALLS THIS, NOT ONLY A CHANGE TO
    //  THE LINK THE LIST IS ON: a link with nothing to connect to changes state
    //  about once a second as it retries, so the list and the preview are
    //  redrawn only when the reload changed them, or each retry would scroll the
    //  preview to its top.
    if (showing)
    {
        std::vector<CatalogRow>  rows     = m_window.m_browser.GetRows();
        std::vector<int>         selected = m_window.m_browser.GetSelectedRows();
        std::wstring             message  = m_window.m_browser.GetListError();
        PreviewContent           preview  = m_window.m_browser.GetPreview();

        hr = m_window.m_browser.Reload (true);
        IGNORE_RETURN_VALUE (hr, S_OK);

        m_window.m_browser.RefreshPreview();

        //  Filling the list fills the preview as well.
        if (m_window.m_browser.GetRows() != rows || m_window.m_browser.GetSelectedRows() != selected || m_window.m_browser.GetListError() != message)
        {
            m_window.m_listPane.FillList();
        }
        else if (m_window.m_browser.GetPreview() != preview)
        {
            m_window.m_previewPane.FillPreview();
        }
    }

    m_window.Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::IsLinkRemoved
//
//  True when no connection, saved or found running, has the link's name.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerLinks::IsLinkRemoved (const std::wstring & link) const
{
    const std::vector<CassoLinkSettings> &  links = m_window.m_context.links->GetLinks();



    return std::none_of (links.begin(), links.end(), [&link] (const CassoLinkSettings & settings) { return TextEncoding::NarrowToWide (settings.name) == link; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::OnRunningChanged
//
//  A root whose Casso has exited is removed; when the list is on it, the
//  list goes to This PC.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::OnRunningChanged()
{
    HRESULT                    hr      = S_OK;
    std::vector<RunningCasso>  running;
    std::wstring               link;
    int                        slot    = 0;
    int                        drive   = 0;
    bool                       showing = false;
    bool                       gone    = false;



    hr = m_window.m_context.running->Enumerate (running);
    CHR (hr);

    m_window.m_context.links->SetRunning (running);

    if (m_window.m_context.announced != nullptr)
    {
        m_announcedChanges = m_window.m_context.announced->GetChangeCount();
        m_window.m_context.links->SetAnnounced (m_window.m_context.announced->GetAnnounced());
    }

    showing = TreeModel::TryParseLinkId (m_window.m_browser.GetLocation().path, link, slot, drive);
    gone    = showing && IsLinkRemoved (link);

    if (gone)
    {
        m_window.m_addressBar.GoToLocation (Location::MakeRoot (TreeModel::kThisPcRootId));
    }

    m_window.m_browser.GetTreeModel().SetLinks (m_window.m_context.links->BuildTreeEntries());
    m_window.m_treePane.RefreshTree();
    StartAdtProServers();

Error:
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerLinks::GetLatestActivity
//
//  The status bar's CassoLink line: the log's latest entry, for a quarter
//  of an hour after it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerLinks::GetLatestActivity() const
{
    std::vector<CassoLinkConnections::Activity>  log;
    FILETIME                                     local = {};
    FILETIME                                     then  = {};
    FILETIME                                     now   = {};
    ULARGE_INTEGER                               a     = {};
    ULARGE_INTEGER                               b     = {};



    if (m_window.m_context.links == nullptr)
    {
        return std::wstring();
    }

    log = m_window.m_context.links->GetActivity();

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
//  ExplorerLinks::FormatActivity
//
//  The log's last `lines` entries, or every one for 0, oldest first, each
//  with its time.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerLinks::FormatActivity (size_t lines) const
{
    std::vector<CassoLinkConnections::Activity>  log   = (m_window.m_context.links != nullptr) ? m_window.m_context.links->GetActivity() : std::vector<CassoLinkConnections::Activity>();
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
//  ExplorerLinks::ShowActivityLog
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerLinks::ShowActivityLog()
{
    std::wstring  text = FormatActivity (0);



    DxuiMessageBox (m_window.GetHwnd(), m_window.m_theme, text.c_str(), L"CassoLink activity", MB_OK);
}

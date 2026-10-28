#include "Pch.h"

#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/SearchQuery.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ExplorerAddressBar
//
////////////////////////////////////////////////////////////////////////////////

ExplorerAddressBar::ExplorerAddressBar (ExplorerContext & context)
    : m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::Create
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::Create()
{
    m_address = m_context.window.CreateChild<DxuiAddressBar>();
    m_findBox = m_context.window.CreateChild<DxuiTextInput>();

    m_address->SetTextRenderer (m_context.window.GetTextRenderer());
    m_address->SetIconFace     (DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace)
                                ? DxuiToolbar::kFluentIconFace
                                : DxuiToolbar::kMdl2IconFace);
    m_address->SetFont         (DxuiTextRenderer::IsFontFamilyInstalled (DxuiAddressBar::kVariableTextFace)
                                ? DxuiAddressBar::kVariableTextFace
                                : DxuiTheme::GetUiFace(),
                                DxuiAddressBar::kFontDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::Configure
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::Configure()
{
    m_addressRoot = GetProfileRoot();

    //  A local folder only: a network path is never asked about on this thread.
    m_addressRoot.isHostFolder = [this] (const std::wstring & path)
    {
        return m_context.app.fs != nullptr && path.size() > 1 && path[1] == L':' && m_context.app.fs->DirectoryExists (path);
    };

    m_address->SetOnSegment ([this] (int index)
    {
        if (index >= 0 && index < (int) m_addressSegments.size())
        {
            m_context.browser.NavigateToLocation (m_addressSegments[(size_t) index].location);
            m_context.listPane.FillList();
        }
    });
    m_address->SetOnSubmit    ([this] (const std::wstring & text) { SubmitAddress (text); });
    m_address->SetOnSeparator ([this] (int index, const RECT & anchor) { ShowAddressMenu (index, anchor); });
    m_address->SetOnOverflow  ([this] (const RECT & anchor) { ShowAddressOverflowMenu (anchor); });
    m_address->SetOnRoots     ([this] (const RECT & anchor) { ShowAddressRootsMenu (anchor); });

    m_findBox->SetTextRenderer   (m_context.window.GetTextRenderer());
    m_findBox->SetHwnd           (m_context.window.GetHwnd());
    m_findBox->SetFont           (DxuiAddressBar::kVariableTextFace, DxuiAddressBar::kFontDip);
    m_findBox->SetExplorerChrome (true);
    m_findBox->SetTrailingGlyph  (s_kpszMdl2Search, DxuiTextRenderer::IsFontFamilyInstalled (DxuiToolbar::kFluentIconFace) ? DxuiToolbar::kFluentIconFace : DxuiToolbar::kMdl2IconFace);
    m_findBox->SetClearGlyph     (s_kpszMdl2Cancel);

    //  An emptied box leaves the search for the folder it searched, as
    //  Explorer's does, whether by its X or by deleting the query.
    m_findBox->SetOnChange ([this] (const std::wstring & text)
    {
        if (text.empty())
        {
            LeaveSearch();
        }
    });

    m_address->SetOnHistory      ([this] (const RECT & anchor) { ShowAddressHistoryMenu (anchor); });
    m_address->SetHistoryChevron (false);

    //  Explorer drops its history as the address opens for editing, and
    //  turns it into completions as a path is typed.
    m_suggest.SetPopupHost (m_context.window.GetPopupHost());
    m_suggest.SetOnPick    ([this] (const std::wstring & text) { SubmitAddress (text); });

    m_address->SetOnEditState ([this] (bool editing)
    {
        m_addressTyped.clear();

        if (editing)
        {
            ShowAddressSuggestions (std::wstring());
        }
        else
        {
            m_suggest.Close();
        }
    });
    m_address->SetOnEditText ([this] (const std::wstring & text)
    {
        m_addressTyped = text;
        ShowAddressSuggestions (text);
    });
    m_address->SetOnEditKey ([this] (WPARAM vk) { return HandleAddressKey (vk); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::Layout
//
//  The address box is the toolbar's free span, 32 dip tall and centered in
//  the strip above its bottom line. The search box takes the row's right
//  end, 30% of the width right of the tree and a little in from the edge,
//  as Explorer's does, and the address the rest.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::Layout (const RECT & free, const RECT & strip, int rightOfTreePx)
{
    const DxuiDpiScaler  & scaler      = m_context.scaler;
    RECT                   addressRect = GetAddressRect (free, strip, scaler);
    RECT                   findRect    = {};
    int                    findWidth   = 0;



    addressRect.right -= (int) std::lround (scaler.ToPxf (kFindBoxRightInsetDip));
    findWidth          = std::clamp ((int) std::lround (kFindBoxShare * (float) rightOfTreePx), scaler.ToPx (kFindBoxMinDip), scaler.ToPx (kFindBoxMaxDip));
    findRect           = RECT { addressRect.right - findWidth, addressRect.top, addressRect.right, addressRect.bottom };

    addressRect.right = findRect.left - scaler.ToPx (kFindBoxGapDip);
    m_address->Layout (addressRect, scaler);
    m_findBox->Layout (findRect, scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetAddressRect
//
////////////////////////////////////////////////////////////////////////////////

RECT ExplorerAddressBar::GetAddressRect (const RECT & free, const RECT & strip, const DxuiDpiScaler & scaler)
{
    int   height = scaler.ToPx (kAddressBoxDip);
    int   middle = (strip.top + strip.bottom - DxuiToolbar::GetEdgePx (scaler)) / 2;
    RECT  rect   = free;



    if (free.right <= free.left)
    {
        return free;
    }

    rect.top    = middle - height / 2;
    rect.bottom = rect.top + height;

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::HandleLocationChanged
//
//  The address leads with the icon of its first segment. Explorer's search
//  box has the name of what it will search, and is emptied on leaving a
//  search's results. For a search's results, a restored tab's among them,
//  the box has the query, over the folder that was searched.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::HandleLocationChanged (const Location & location)
{
    Location      lead  = GetAddressSegmentsFor (location).front().location;
    std::wstring  scope;
    std::wstring  query;
    Location      up;



    m_address->SetLeadIcon  (BrowserCells::GetLocationIcon (lead, m_context.shellIcons));
    m_address->SetLeadGlyph ((lead.kind == Location::Kind::Root && lead.path == TreeModel::kThisPcRootId) ? s_kpszMdl2ThisPc : nullptr);

    if (!SearchQuery::IsId (location.path))
    {
        std::vector<BrowserModel::AddressSegment>  segments = BrowserModel::GetAddressSegments (location, m_addressRoot);

        m_findBox->SetText        (L"");
        m_findBox->SetPlaceholder (L"Search " + (segments.empty() ? std::wstring() : segments.back().label));
        return;
    }

    SearchQuery::TryParseId (location.path, scope, query);
    m_findBox->SetText        (query);
    m_findBox->SetPlaceholder (L"Search " + (m_context.browser.TryGetUpTarget (up) ? GetNavLabel (up) : std::wstring()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::CloseSuggestionsOnPress
//
//  The suggestions have no capture, so a press anywhere else in the window
//  closes them; a press in the address bar edits the path or, on the caret,
//  closes them there.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::CloseSuggestionsOnPress (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Down && m_suggest.IsOpen() && !ExplorerLayout::Contains (m_address->GetBounds(), ev.positionDip))
    {
        m_suggest.Close();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::HandleMouse
//
//  A right-click on the address, not while it is being edited, opens
//  Explorer's address menu. The address bar is in the toolbar's row, so it is
//  ahead of the toolbar. Moves go to the address and the search box wherever
//  the pointer is, so their hover clears on the way out.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerAddressBar::HandleMouse (const DxuiMouseEvent & ev)
{
    POINT  point  = ev.positionDip;
    bool   press  = ev.kind == DxuiMouseEventKind::Down;
    bool   wasLit = false;



    if (press && ev.button == DxuiMouseButton::Right && !m_address->IsEditing() && ExplorerLayout::Contains (m_address->GetBounds(), point))
    {
        ShowAddressContextMenu (point.x, point.y);
        return true;
    }

    if (ev.kind == DxuiMouseEventKind::Move && !m_address->IsInteracting() && m_address->OnMouse (ev))
    {
        m_context.window.Invalidate();
    }

    if (ev.kind == DxuiMouseEventKind::Move && !m_address->IsInteracting())
    {
        wasLit = m_findBox->IsClearHovered();

        m_findBox->OnMouse (ev);

        if (m_findBox->IsClearHovered() != wasLit)
        {
            m_context.window.Invalidate();
        }
    }

    if (ev.kind == DxuiMouseEventKind::Move && !m_address->IsInteracting())
    {
        return false;
    }

    if (ExplorerLayout::Contains (m_findBox->GetBounds(), point))
    {
        if (press)
        {
            m_context.input.SetFocusPane (ExplorerPane::LocationSearch);
        }

        m_findBox->OnMouse (ev);
        m_context.window.Invalidate();
        return true;
    }

    if (!m_address->IsInteracting() && !ExplorerLayout::Contains (m_address->GetBounds(), point))
    {
        return false;
    }

    if (press)
    {
        m_context.input.SetFocusPane (ExplorerPane::Address);
        m_address->SetFocusCue (false);
    }

    if (!m_address->OnMouse (ev))
    {
        return false;
    }

    m_context.window.Invalidate();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetCursorForPoint
//
//  The address being edited is text, and the cursor over it is the I-beam;
//  over its clear and history buttons it is the arrow.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR ExplorerAddressBar::GetCursorForPoint (POINT clientPx) const
{
    if (m_address->IsEditing() && m_address->HitTest (clientPx.x, clientPx.y).part == DxuiAddressBar::Part::Blank)
    {
        return IDC_IBEAM;
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::HandleFocusChanged
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::HandleFocusChanged (ExplorerPane focus)
{
    m_address->OnFocusChanged (focus == ExplorerPane::Address);
    m_findBox->SetFocused     (focus == ExplorerPane::LocationSearch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::Tick
//
//  Returns whether the address bar's animation moved.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerAddressBar::Tick (int64_t nowMs)
{
    if (!m_address->WantsTick())
    {
        return false;
    }

    m_address->Tick (nowMs);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::EndEdit
//
//  An address being typed is for the tab it was typed in, so it is dropped.
//  Returns whether one was being typed.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerAddressBar::EndEdit()
{
    if (!m_address->IsEditing())
    {
        return false;
    }

    m_address->EndEdit();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GoToLocation
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::GoToLocation (const Location & location)
{
    if (m_context.browser.GetLocation() != location)
    {
        m_context.browser.NavigateToLocation (location);
        m_context.listPane.FillList();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetAddressSegmentsFor
//
//  The address bar's segments, as Explorer shows a path on a drive: This PC,
//  then the drive under its name, then the folders. Never empty.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<BrowserModel::AddressSegment> ExplorerAddressBar::GetAddressSegmentsFor (const Location & location)
{
    std::vector<BrowserModel::AddressSegment>  segments = BrowserModel::GetAddressSegments (location, m_addressRoot);
    IShellIcons::DriveInfo                     drive;
    bool                                       onDrive  = false;



    if (!segments.empty() && segments.front().location.kind == Location::Kind::HostFolder)
    {
        const std::wstring &  root = segments.front().location.path;

        onDrive = root.size() == 3 && root[1] == L':' && root[2] == L'\\';
    }

    if (onDrive)
    {
        if (m_context.shellIcons.GetDriveInfo (segments.front().location.path, drive) && !drive.name.empty())
        {
            segments.front().label = drive.name;
        }

        segments.insert (segments.begin(), BrowserModel::AddressSegment { TreeModel::GetRootLabel (TreeModel::kThisPcRootId),
                                                                          Location::MakeRoot (TreeModel::kThisPcRootId) });
    }

    if (segments.empty())
    {
        segments.push_back (BrowserModel::AddressSegment { std::wstring(), location });
    }

    return segments;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetNavLabel
//
//  A location as the address bar shows it: a drive by its name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerAddressBar::GetNavLabel (const Location & location)
{
    std::vector<BrowserModel::AddressSegment>  segments = GetAddressSegmentsFor (location);



    return segments.back().label.empty() ? BrowserCells::GetLocationLabel (location) : segments.back().label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::FillNavTips
//
//  File Explorer's navigation tips say where each button goes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::FillNavTips()
{
    const BrowserModel &  model = m_context.browser.GetBrowserModel();
    Location              up;
    std::wstring          back    = L"Back";
    std::wstring          forward = L"Forward";
    std::wstring          upTip   = L"Up (Alt + Up Arrow)";



    if (model.HasTabs() && !model.GetActiveTab().back.empty())
    {
        back = L"Back to " + GetNavLabel (model.GetActiveTab().back.back());
    }

    if (model.HasTabs() && !model.GetActiveTab().forward.empty())
    {
        forward = L"Forward to " + GetNavLabel (model.GetActiveTab().forward.back());
    }

    if (m_context.browser.TryGetUpTarget (up))
    {
        upTip = L"Up to \"" + GetNavLabel (up) + L"\" (Alt + Up Arrow)";
    }

    m_context.commands.SetTip (CassoExplorerCommands::kBack,    back);
    m_context.commands.SetTip (CassoExplorerCommands::kForward, forward);
    m_context.commands.SetTip (CassoExplorerCommands::kUp,      upTip);
    m_context.commands.SetTip (CassoExplorerCommands::kRefresh, L"Refresh \"" + GetNavLabel (m_context.browser.GetLocation()) + L"\" (F5)");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::FillAddress
//
//  The segments and the path of where the active tab is.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::FillAddress()
{
    Location                   location = m_context.browser.GetLocation();
    std::vector<std::wstring>  labels;



    m_addressSegments = GetAddressSegmentsFor (location);

    for (const BrowserModel::AddressSegment & segment : m_addressSegments)
    {
        labels.push_back (segment.label);
    }

    m_address->SetSegments (std::move (labels));
    m_address->SetPath (BrowserModel::FormatAddress (location));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::SubmitAddress
//
//  A path that goes nowhere says so and leaves the text as typed.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::SubmitAddress (const std::wstring & text)
{
    if (m_context.browser.NavigateToAddress (text))
    {
        m_context.input.SetFocusPane (ExplorerPane::List);
        m_context.listPane.FillList();

        //  The typed path joins the history now, not only at a clean exit, and
        //  a folder joins Explorer's list too. A path into a disk image stays
        //  in Casso Explorer's own: Explorer would open the image file.
        AddTypedPath (CassoExplorerBrowser::RootOnSystemDrive (text));
        m_context.window.SaveSession();

        if (m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
        {
            WriteExplorerTypedPath (CassoExplorerBrowser::RootOnSystemDrive (text));
        }
    }
    else
    {
        m_context.window.ShowMessage (L"Casso Explorer can't find \"" + text + L"\". Check the path and try again.", MB_ICONWARNING);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::AddTypedPath
//
//  Every window shares the preferences' typed paths, so a path goes on top of
//  those rather than replacing them with this window's list.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::AddTypedPath (const std::wstring & text)
{
    TypedPathHistory  shared;



    shared.Reset (m_context.prefs.typedPaths);
    shared.Add (text);
    m_context.prefs.typedPaths = shared.GetEntries();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressMenu
//
//  The folders and images inside a segment's location, hung from the
//  separator after it, as Explorer's address bar lists a folder's children.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressMenu (int index, const RECT & anchor)
{
    std::vector<BrowserModel::AddressSegment>  children;
    std::vector<DxuiPopupMenuItem>             items;



    if (index < 0 || index >= (int) m_addressSegments.size())
    {
        return;
    }

    m_context.browser.GetFolderChildren (m_addressSegments[(size_t) index].location, children);
    m_context.menus.GetMenuCommands().clear();

    for (const BrowserModel::AddressSegment & child : children)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        Location                      target  = child.location;

        command->label = EscapeMnemonics (child.label);

        command->dispatch = [this, target]()
        {
            m_context.browser.NavigateToLocation (target);
            m_context.listPane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    if (!items.empty())
    {
        m_address->SetOpenSeparator (index);
        DxuiContextMenu::ShowUnder (*m_context.window.GetPopupHost(), anchor, std::move (items), [this] (bool)
        {
            m_address->SetOpenSeparator (-1);
            m_context.window.Invalidate();
        });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressHistoryMenu
//
//  The paths typed into the bar, newest first.
//
//  EACH PICK GOES BACK THROUGH THE TEXT, not through the location it parses
//  to. The history records a path where a typed navigation succeeds, so
//  submitting the text is what moves the pick to the top of the list; handing
//  the parsed location straight to the browser would navigate correctly and
//  silently leave the order alone.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressHistoryMenu (const RECT & anchor)
{
    UNREFERENCED_PARAMETER (anchor);

    //  The caret opens the history and, pressed again, closes it.
    if (m_suggest.IsOpen())
    {
        m_suggest.Close();
        return;
    }

    ShowAddressSuggestions (std::wstring());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressSuggestions
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressSuggestions (const std::wstring & typed)
{
    m_suggest.Show (m_address->GetBounds(), typed.empty() ? GetAddressHistory() : GetCompletions (typed));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::HandleAddressKey
//
//  While the list shows, Up and Down walk it, the box showing the row walked
//  to, or what was typed once the walk leaves the list; Escape closes it and
//  leaves the edit open. Enter goes to whatever the box shows.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerAddressBar::HandleAddressKey (WPARAM vk)
{
    std::wstring  shown;



    if (!m_suggest.IsOpen())
    {
        return false;
    }

    if (vk == VK_DOWN || vk == VK_UP)
    {
        shown = m_suggest.MoveHighlight ((vk == VK_DOWN) ? 1 : -1);
        m_address->SetEditText (shown.empty() ? m_addressTyped : shown);
        m_context.window.Invalidate();
        return true;
    }

    if (vk == VK_ESCAPE)
    {
        m_suggest.Close();
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetAddressHistory
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerAddressBar::GetAddressHistory() const
{
    return TypedPathHistory::Merge (ReadExplorerTypedPaths(), m_context.prefs.typedPaths);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetCompletions
//
//  What is in the folder the typed path has reached, whose names start with
//  what follows its last backslash, as Explorer lists them: folders and
//  files, hidden ones left out, sorted by name.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerAddressBar::GetCompletions (const std::wstring & typed)
{
    std::vector<std::wstring>  out;
    std::wstring               rooted = CassoExplorerBrowser::RootOnSystemDrive (typed);
    size_t                     slash  = rooted.find_last_of (L'\\');
    std::wstring               folder;
    std::wstring               prefix;
    std::wstring               shownFolder;
    WIN32_FIND_DATAW           found  = {};
    HANDLE                     search = INVALID_HANDLE_VALUE;



    if (slash == std::wstring::npos)
    {
        return out;
    }

    folder      = rooted.substr (0, slash + 1);
    prefix      = rooted.substr (slash + 1);
    shownFolder = typed.substr (0, typed.find_last_of (L'\\') + 1);

    search = FindFirstFileExW ((folder + L"*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);

    if (search == INVALID_HANDLE_VALUE)
    {
        return out;
    }

    do
    {
        std::wstring  name = found.cFileName;

        if (name == L"." || name == L".." || (found.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0)
        {
            continue;
        }

        if (_wcsnicmp (name.c_str(), prefix.c_str(), prefix.size()) == 0)
        {
            out.push_back (shownFolder + name);
        }
    }
    while (out.size() < kMaxCompletions && FindNextFileW (search, &found));

    FindClose (search);

    std::sort (out.begin(), out.end(), [] (const std::wstring & a, const std::wstring & b) { return _wcsicmp (a.c_str(), b.c_str()) < 0; });

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressContextMenu
//
//  Explorer's: the location as text (both copies), editing it, and clearing
//  the typed history -- Explorer's own list too, since the two are shown as
//  one.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressContextMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;
    Location                        location = m_context.browser.GetLocation();
    std::wstring                    address  = BrowserModel::FormatAddress (location);



    m_context.menus.GetMenuCommands().clear();

    //  Explorer's two both put the path on as text; neither puts the folder.
    m_context.menus.AddMenuCommand (items, L"&Copy address", [this, address]() { DxuiClipboard::SetText (m_context.window.GetHwnd(), address); });

    m_context.menus.AddMenuCommand (items, L"Copy address as &text", [this, address]() { DxuiClipboard::SetText (m_context.window.GetHwnd(), address); });

    m_context.menus.AddMenuCommand (items, L"&Edit address", [this]()
    {
        m_context.input.SetFocusPane (ExplorerPane::Address);
        m_address->BeginEdit();
        m_context.window.Invalidate();
    });

    m_context.menus.AddMenuCommand (items, L"&Delete history", [this]()
    {
        HKEY  key = nullptr;

        m_context.browser.GetTypedPaths().Clear();
        m_context.prefs.typedPaths.clear();

        if (RegOpenKeyExW (HKEY_CURRENT_USER, kExplorerTypedPathsKey, 0, KEY_ALL_ACCESS, &key) == ERROR_SUCCESS)
        {
            RegDeleteTreeW (key, nullptr);
            RegCloseKey (key);
        }
    });

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ReadExplorerTypedPaths
//
//  Explorer's typed paths, url1 the newest.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ExplorerAddressBar::ReadExplorerTypedPaths()
{
    std::vector<std::wstring>  out;
    HKEY                       key    = nullptr;
    LSTATUS                    status = RegOpenKeyExW (HKEY_CURRENT_USER, kExplorerTypedPathsKey, 0, KEY_READ, &key);
    size_t                     i      = 0;



    if (status != ERROR_SUCCESS)
    {
        return out;
    }

    for (i = 1; i <= TypedPathHistory::kMaxEntries; i++)
    {
        wchar_t  value[MAX_PATH * 2] = {};
        DWORD    bytes               = sizeof (value) - sizeof (wchar_t);
        DWORD    type                = 0;

        status = RegQueryValueExW (key, std::format (L"url{}", i).c_str(), nullptr, &type, (BYTE *) value, &bytes);

        if (status == ERROR_SUCCESS && type == REG_SZ && value[0] != L'\0')
        {
            out.push_back (value);
        }
    }

    RegCloseKey (key);

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::WriteExplorerTypedPath
//
//  Puts a folder at the top of Explorer's typed paths, as Explorer does when
//  one is typed there: a repeat moves up, and the oldest past the limit goes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::WriteExplorerTypedPath (const std::wstring & path)
{
    std::vector<std::wstring>  paths  = ReadExplorerTypedPaths();
    HKEY                       key    = nullptr;
    LSTATUS                    status = ERROR_SUCCESS;
    size_t                     i      = 0;



    paths.erase (std::remove_if (paths.begin(), paths.end(),
                                 [&path] (const std::wstring & held) { return _wcsicmp (held.c_str(), path.c_str()) == 0; }),
                 paths.end());
    paths.insert (paths.begin(), path);

    if (paths.size() > TypedPathHistory::kMaxEntries)
    {
        paths.resize (TypedPathHistory::kMaxEntries);
    }

    status = RegCreateKeyExW (HKEY_CURRENT_USER, kExplorerTypedPathsKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);

    if (status != ERROR_SUCCESS)
    {
        return;
    }

    for (i = 0; i < paths.size(); i++)
    {
        status = RegSetValueExW (key, std::format (L"url{}", i + 1).c_str(), 0, REG_SZ,
                                 (const BYTE *) paths[i].c_str(), (DWORD) ((paths[i].size() + 1) * sizeof (wchar_t)));
        IGNORE_RETURN_VALUE (status, ERROR_SUCCESS);
    }

    RegCloseKey (key);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressOverflowMenu
//
//  The segments collapsed behind the address bar's overflow button, nearest
//  first, as Explorer lists them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressOverflowMenu (const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>  items;
    int                             i = 0;



    m_context.menus.GetMenuCommands().clear();

    for (i = (std::min) (m_address->GetFirstShown(), (int) m_addressSegments.size()) - 1; i >= 0; i--)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        Location                      target  = m_addressSegments[(size_t) i].location;

        command->label    = EscapeMnemonics (m_addressSegments[(size_t) i].label);
        command->dispatch = [this, target]()
        {
            m_context.browser.NavigateToLocation (target);
            m_context.listPane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*m_context.window.GetPopupHost(), anchor, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::LeaveSearch
//
//  Back to the folder a search covered: by Back when the search was run here,
//  and by going to the folder when the results tab was restored, with no
//  history to go back through.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::LeaveSearch()
{
    Location  searched;



    if (!SearchQuery::IsId (m_context.browser.GetLocation().path))
    {
        return;
    }

    if (!m_context.browser.GoBack() && m_context.browser.TryGetUpTarget (searched))
    {
        m_context.browser.NavigateToLocation (searched);
    }

    m_context.listPane.FillList();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::HandleFindBoxKey
//
//  Enter searches, Escape empties the box and leaves a search's results, as
//  Explorer's box does; every other key edits the query.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerAddressBar::HandleFindBoxKey (const DxuiKeyEvent & ev)
{
    bool  down = ev.kind == DxuiKeyEventKind::Down;



    if (down && ev.vk == VK_RETURN)
    {
        SearchLocation (m_findBox->GetText());
        return true;
    }

    if (down && ev.vk == VK_ESCAPE)
    {
        m_findBox->SetText (L"");
        LeaveSearch();

        m_context.input.SetFocusPane (ExplorerPane::List);
        return true;
    }

    return m_findBox->OnKey (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetCassoSearchScope
//
//  A search from Casso's root covers each of Casso's own folders on the host.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerAddressBar::GetCassoSearchScope()
{
    std::vector<TreeNode>      nodes;
    std::vector<std::wstring>  folders;
    HRESULT                    hr = m_context.browser.GetTreeModel().GetChildren (TreeModel::kCassoRootId, nodes);



    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const TreeNode & node : nodes)
    {
        if (node.location.kind == Location::Kind::HostFolder && !node.location.path.empty())
        {
            folders.push_back (node.location.path);
        }
    }

    return SearchQuery::JoinScopes (folders);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::SearchLocation
//
//  The folder shown and everything below it; a search from a search's
//  results searches the same folder again.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::SearchLocation (const std::wstring & query)
{
    Location      at    = m_context.browser.GetLocation();
    std::wstring  scope;
    std::wstring  previous;



    //  Enter in an empty box goes back to the list, as File Explorer's does.
    if (query.empty())
    {
        m_context.input.SetFocusPane (ExplorerPane::List);
        return;
    }

    if (at.kind == Location::Kind::HostFolder)
    {
        scope = at.path;
    }
    else if (SearchQuery::IsId (at.path))
    {
        SearchQuery::TryParseId (at.path, scope, previous);
    }
    else if (at.kind == Location::Kind::ShellFolder && !at.path.empty() && !Location::IsShellName (at.path))
    {
        scope = at.path;
    }
    else if (at.kind == Location::Kind::Root && at.path == TreeModel::kCassoRootId)
    {
        scope = GetCassoSearchScope();
    }

    if (scope.empty())
    {
        m_context.window.ShowMessage (L"Search covers folders on a disk; this location has none.", MB_ICONINFORMATION);
        return;
    }

    m_context.browser.NavigateToLocation (Location::MakeShellFolder (SearchQuery::MakeId (scope, query), SearchQuery::GetLabel (scope)));
    m_context.listPane.FillList();
    m_context.input.SetFocusPane (ExplorerPane::List);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowAddressRootsMenu
//
//  The chevron after the location's icon: the desktop's roots, then the
//  folders on the user's desktop, each with its icon, as Explorer's has.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowAddressRootsMenu (const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>                 items;
    std::vector<IShellItemVerbs::ShellFolderItem>  folders;
    HRESULT                                        hr = m_context.shellVerbs.ListDesktopFolders (folders);



    IGNORE_RETURN_VALUE (hr, S_OK);
    m_context.menus.GetMenuCommands().clear();

    for (const IShellItemVerbs::ShellFolderItem & folder : folders)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        std::wstring                  target  = folder.path.empty() ? folder.id : folder.path;

        //  This PC and the Recycle Bin open as Casso Explorer's own, with
        //  their drives and their Restore.
        Location  own = folder.isRecycleBin ? Location::MakeRecycleBin()
                      : folder.isThisPc     ? Location::MakeRoot (TreeModel::kThisPcRootId)
                                            : Location();

        command->label     = EscapeMnemonics (folder.name);
        command->menuImage = m_context.shellIcons.GetForPath (target, true);
        command->dispatch  = [this, target, own]()
        {
            if (own.kind != Location::Kind::None)
            {
                m_context.browser.NavigateToLocation (own);
            }
            else
            {
                m_context.browser.NavigateToAddress (target);
            }

            m_context.listPane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*m_context.window.GetPopupHost(), anchor, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::ShowHistoryMenu
//
//  Where Back or Forward would go, nearest first; picking one takes as many
//  steps as it is away.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerAddressBar::ShowHistoryMenu (bool forward, const RECT & anchor)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::vector<Location>           stack;
    size_t                          i     = 0;



    if (!m_context.browser.GetBrowserModel().HasTabs())
    {
        return;
    }

    stack = forward ? m_context.browser.GetBrowserModel().GetActiveTab().forward
                    : m_context.browser.GetBrowserModel().GetActiveTab().back;

    m_context.menus.GetMenuCommands().clear();

    for (i = 0; i < stack.size(); i++)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        size_t                        steps   = i + 1;

        command->label    = EscapeMnemonics (BrowserCells::GetLocationLabel (stack[stack.size() - 1 - i]));
        command->dispatch = [this, forward, steps]()
        {
            if (forward ? m_context.browser.GoForwardBy (steps) : m_context.browser.GoBackBy (steps))
            {
                m_context.listPane.FillList();
            }
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*m_context.window.GetPopupHost(), anchor, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::EscapeMnemonics
//
//  A menu label reads an ampersand as a mnemonic marker, so a name's own is
//  doubled.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerAddressBar::EscapeMnemonics (const std::wstring & text)
{
    std::wstring  escaped;



    for (wchar_t ch : text)
    {
        escaped += (ch == L'&') ? L"&&" : std::wstring (1, ch);
    }

    return escaped;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerAddressBar::GetProfileRoot
//
//  The user's profile folder and the name the shell gives it, which is the
//  user's own name; empty when the shell will not say.
//
////////////////////////////////////////////////////////////////////////////////

BrowserModel::AddressRoot ExplorerAddressBar::GetProfileRoot()
{
    BrowserModel::AddressRoot  root;
    IShellItem               * item = nullptr;
    PWSTR                      path = nullptr;
    PWSTR                      name = nullptr;
    HRESULT                    hr   = S_OK;



    //  The profile as the shell's namespace holds it, where its display name
    //  is the user's name. The item for its file system path is displayed as
    //  the folder's own name instead.
    hr = SHCreateItemFromParsingName (L"shell:UsersFilesFolder", nullptr, IID_PPV_ARGS (&item));

    if (SUCCEEDED (hr))
    {
        hr = item->GetDisplayName (SIGDN_FILESYSPATH, &path);
    }

    if (SUCCEEDED (hr))
    {
        hr = item->GetDisplayName (SIGDN_NORMALDISPLAY, &name);
    }

    if (SUCCEEDED (hr))
    {
        root.path  = path;
        root.label = name;
    }

    CoTaskMemFree (name);
    CoTaskMemFree (path);

    if (item != nullptr)
    {
        item->Release();
    }

    return root;
}

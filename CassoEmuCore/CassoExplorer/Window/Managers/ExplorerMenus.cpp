#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ExplorerMenus
//
////////////////////////////////////////////////////////////////////////////////

ExplorerMenus::ExplorerMenus (CassoExplorerWindow & window)
    : m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ShowScrollbarMenu
//
//  The lists take points relative to their own bounds, as their OnMouse
//  does; the other views take the window's. Returns whether the point was
//  on a bar.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerMenus::ShowScrollbarMenu (const DxuiMouseEvent & ev)
{
    POINT                           point = ev.positionDip;
    std::vector<DxuiPopupMenuItem>  items;



    if (CassoExplorerWindow::Contains (m_window.m_treePane.GetBounds(), point))
    {
        items = m_window.m_treePane.GetScrollbarMenu (point);
    }
    else if (CassoExplorerWindow::Contains (m_window.m_listPane.GetBounds(), point))
    {
        items = m_window.m_listPane.GetScrollbarMenu (ev);
    }
    else
    {
        items = m_window.m_previewPane.GetScrollbarMenu (ev);
    }

    if (items.empty())
    {
        return false;
    }

    DxuiContextMenu::Show (*m_window.GetPopupHost(), point.x, point.y, std::move (items), [this] (bool) { m_window.Invalidate(); });

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetVerbLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:           return L"&Open";
        case CassoExplorerActions::Verb::Get:            return L"&Copy to folder...";
        case CassoExplorerActions::Verb::Put:            return L"&Put file...";
        case CassoExplorerActions::Verb::Delete:         return L"&Delete";
        case CassoExplorerActions::Verb::Rename:         return L"Re&name...";
        case CassoExplorerActions::Verb::Boot:           return L"Set as &startup program";
        case CassoExplorerActions::Verb::InsertDrive1:   return L"Insert into Casso drive &1";
        case CassoExplorerActions::Verb::InsertDrive2:   return L"Insert into Casso drive &2";
        case CassoExplorerActions::Verb::OpenInNewCasso: return L"Open in &new Casso";
        case CassoExplorerActions::Verb::NewDisk:        return L"&Disk image...";
        case CassoExplorerActions::Verb::NewFolder:      return L"&Folder";
        case CassoExplorerActions::Verb::Format:         return L"&Format disk image...";
        case CassoExplorerActions::Verb::ReadSectors:    return L"Read &sectors to file...";
        case CassoExplorerActions::Verb::WriteSectors:   return L"&Write sectors from file...";
        case CassoExplorerActions::Verb::ReadBlocks:     return L"Read &blocks to file...";
        case CassoExplorerActions::Verb::WriteBlocks:    return L"Write b&locks from file...";
        case CassoExplorerActions::Verb::Refresh:        return L"&Refresh";
        case CassoExplorerActions::Verb::OpenWith:       return L"Open wit&h";
        case CassoExplorerActions::Verb::MoreOptions:    return L"Show more &options";
        case CassoExplorerActions::Verb::Cut:            return L"Cu&t";
        case CassoExplorerActions::Verb::Copy:           return L"&Copy";
        case CassoExplorerActions::Verb::Paste:          return L"&Paste";
        case CassoExplorerActions::Verb::Share:          return L"&Share";
        case CassoExplorerActions::Verb::Restore:        return L"&Restore";
        case CassoExplorerActions::Verb::EmptyRecycleBin: return L"Empty Recycle &Bin";
        case CassoExplorerActions::Verb::ExtractAll:     return L"Extract A&ll...";
        case CassoExplorerActions::Verb::ExtractFiles:   return L"E&xtract all files";
        default:                                   return L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetVerbMenuGlyph
//
//  The icon beside a verb's row in a context menu: Explorer's own for the
//  verbs it has, and for Casso's, the nearest in the same icon font.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ExplorerMenus::GetVerbMenuGlyph (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:           return s_kpszMdl2OpenFile;
        case CassoExplorerActions::Verb::OpenWith:       return s_kpszMdl2OpenWith;
        case CassoExplorerActions::Verb::InsertDrive1:   return s_kpszMdl2Save;
        case CassoExplorerActions::Verb::InsertDrive2:   return s_kpszMdl2Save;
        case CassoExplorerActions::Verb::OpenInNewCasso: return s_kpszMdl2Play;
        case CassoExplorerActions::Verb::Format:         return s_kpszMdl2HardDrive;
        case CassoExplorerActions::Verb::Refresh:        return s_kpszMdl2Refresh;
        case CassoExplorerActions::Verb::MoreOptions:    return s_kpszMdl2OpenInNewWindow;
        default:                                         return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ApplyMenuSvgs
//
//  The commands the menus share -- the views, the preview pane -- take File
//  Explorer's icons for the theme in use.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::ApplyMenuSvgs()
{
    static constexpr std::pair<DxuiListView::View, const wchar_t *>  kViews[] =
    {
        { DxuiListView::View::ExtraLargeIcons, L"windows.iconsize.extralarge" },
        { DxuiListView::View::LargeIcons,      L"windows.iconsize.large"      },
        { DxuiListView::View::MediumIcons,     L"windows.iconsize.medium"     },
        { DxuiListView::View::SmallIcons,      L"windows.iconsize.smallicon"  },
        { DxuiListView::View::List,            L"windows.iconsize.list"       },
        { DxuiListView::View::Details,         L"windows.iconsize.details"    },
        { DxuiListView::View::Tiles,           L"windows.iconsize.tile"       },
        { DxuiListView::View::Content,         L"windows.iconsize.content"    },
    };



    for (const auto & [view, name] : kViews)
    {
        m_window.m_commands.SetMenuSvg (CassoExplorerCommands::kViewFirst + (int) view, GetMenuSvg (name));
    }

    m_window.m_commands.SetMenuSvg (CassoExplorerCommands::kTogglePreview, GetMenuSvg (L"windows.previewpane"));
    m_window.m_commands.SetMenuSvg (CassoExplorerCommands::kNewFolder,     GetMenuSvg (L"newfolder"));
    m_window.m_commands.SetMenuSvg (CassoExplorerCommands::kNewDisk,       GetMenuSvg (L"casso.floppy525"));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetVerbMenuSvgName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Open:         return L"windows.openfolder";
        case CassoExplorerActions::Verb::OpenWith:     return L"windows.openwith";
        case CassoExplorerActions::Verb::InsertDrive1: return L"casso.floppy525";
        case CassoExplorerActions::Verb::InsertDrive2: return L"casso.floppy525";
        case CassoExplorerActions::Verb::Format:       return L"windows.diskformat";
        case CassoExplorerActions::Verb::Refresh:      return L"windows.refresh";
        case CassoExplorerActions::Verb::MoreOptions:  return L"expandtoclassic";
        case CassoExplorerActions::Verb::Cut:          return L"windows.cut";
        case CassoExplorerActions::Verb::Copy:         return L"windows.copy";
        case CassoExplorerActions::Verb::Paste:        return L"windows.paste";
        case CassoExplorerActions::Verb::Rename:       return L"windows.rename";
        case CassoExplorerActions::Verb::Delete:       return L"windows.ribbondelete";
        case CassoExplorerActions::Verb::Share:        return L"Windows.ModernShare";
        case CassoExplorerActions::Verb::Restore:      return L"windows.recyclebin.restoreitems";
        case CassoExplorerActions::Verb::EmptyRecycleBin: return L"windows.recyclebin.empty";
        case CassoExplorerActions::Verb::ExtractAll:   return L"windows.compressedfile.extract";
        case CassoExplorerActions::Verb::ExtractFiles: return L"windows.compressedfile.extract";
        default:                                       return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetOpenMenuImage
//
//  Explorer's Open row carries the icon of the program that will open the
//  item -- its own yellow folder for a folder -- so a folder or disk image,
//  which opens here, carries Casso Explorer's, and any other file the icon
//  of the program Windows opens it in. Drawn at the menu's icon size.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> ExplorerMenus::GetOpenMenuImage (bool browsable, const std::wstring & path)
{
    int                                   sizePx  = m_window.m_scaler.ToPx (kMenuIconDip);
    std::wstring                          program;
    std::wstring                          key;
    HICON                                 icon    = nullptr;
    HRESULT                               hr      = S_OK;
    auto                                  image   = std::make_shared<DxuiIconImage>();
    std::shared_ptr<const DxuiIconImage>  found;



    if (!browsable)
    {
        std::wstring  extension     = std::filesystem::path (path).extension().wstring();
        wchar_t       exe[MAX_PATH] = {};
        DWORD         length        = MAX_PATH;
        HRESULT       hrAssoc       = extension.empty() ? E_FAIL
                                                        : AssocQueryStringW (ASSOCF_NOTRUNCATE | ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_EXECUTABLE, extension.c_str(), L"open", exe, &length);

        if (FAILED (hrAssoc))
        {
            return nullptr;
        }

        program = exe;
    }

    key = std::to_wstring (sizePx) + L":" + program;

    if (m_openMenuImages.count (key) != 0)
    {
        return m_openMenuImages[key];
    }

    if (browsable)
    {
        icon = (HICON) LoadImageW (GetModuleHandleW (nullptr), MAKEINTRESOURCEW (IDI_CASSO_EXPLORER), IMAGE_ICON, sizePx, sizePx, LR_DEFAULTCOLOR);
    }
    else
    {
        hr = SHDefExtractIconW (program.c_str(), 0, 0, &icon, nullptr, (UINT) sizePx);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (icon != nullptr)
    {
        hr = DxuiIconImage::FromHicon (icon, sizePx, *image);
        DestroyIcon (icon);

        if (SUCCEEDED (hr))
        {
            found = image;
        }
    }

    m_openMenuImages[key] = found;

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetIconButtonTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerMenus::GetIconButtonTip (const DxuiCommand & command)
{
    std::wstring     tip   = command.GetLabelText();
    const wchar_t *  keys  = nullptr;



    tip.erase (std::remove (tip.begin(), tip.end(), L'&'), tip.end());

    switch ((CassoExplorerActions::Verb) command.id)
    {
        case CassoExplorerActions::Verb::Cut:    keys = L"Ctrl+X"; break;
        case CassoExplorerActions::Verb::Copy:   keys = L"Ctrl+C"; break;
        case CassoExplorerActions::Verb::Paste:  keys = L"Ctrl+V"; break;
        case CassoExplorerActions::Verb::Rename: keys = L"F2";     break;
        case CassoExplorerActions::Verb::Delete: keys = L"Delete"; break;
        default:                                                   break;
    }

    return (keys != nullptr) ? tip + L" (" + keys + L")" : tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetMenuSvg
//
//  THE ICONS ARE READ, NOT SHIPPED: they are Microsoft's, in File Explorer's
//  own package, so Casso Explorer draws them from the copy on this machine and
//  a missing one falls back to the icon font. Each theme has its set -- the
//  light one fills its outlines, the dark one does not -- so a file is read
//  once per theme. Casso's own art, the 5.25-inch floppy, is drawn in the same
//  hand and the same three colors.
//
////////////////////////////////////////////////////////////////////////////////

const std::string * ExplorerMenus::GetMenuSvg (const wchar_t * name)
{
    bool          dark  = DxuiColor::ComputeRelativeLuminance (m_window.m_theme->Background()) < 0.5f;
    std::wstring  key   = std::wstring (dark ? L"dark:" : L"light:") + ((name != nullptr) ? name : L"");
    auto          found = m_menuSvgs.find (key);
    std::string   svg;



    if (name == nullptr)
    {
        return nullptr;
    }

    if (found != m_menuSvgs.end())
    {
        return found->second ? found->second.get() : nullptr;
    }

    if (wcsncmp (name, L"casso.", 6) == 0)
    {
        svg = kFloppy525Svg;

        for (auto [from, to] : { std::pair<const char *, const char *> { "{INK}",  dark ? "#E0DFDF" : "#555" },
                                 std::pair<const char *, const char *> { "{ACC}",  dark ? "#4CC2FF" : "#0078D4" },
                                 std::pair<const char *, const char *> { "{BODY}", dark ? "none"    : "#FAFAFA" } })
        {
            for (size_t at = svg.find (from); at != std::string::npos; at = svg.find (from, at))
            {
                svg.replace (at, strlen (from), to);
            }
        }
    }
    else
    {
        wchar_t          windows[MAX_PATH] = {};
        UINT             length            = GetWindowsDirectoryW (windows, MAX_PATH);
        std::wstring     path              = std::wstring (windows, length) + kExplorerIconFolder + (dark ? L"theme-dark\\" : L"theme-light\\") + name + L".svg";
        std::ifstream    file              (path, std::ios::binary);

        //  A few are kept at the top of the folder rather than in a theme's.
        if (!file)
        {
            file.open (std::wstring (windows, length) + kExplorerIconFolder + name + L".svg", std::ios::binary);
        }

        if (file)
        {
            svg.assign (std::istreambuf_iterator<char> (file), std::istreambuf_iterator<char>());
        }
    }

    m_menuSvgs[key] = svg.empty() ? nullptr : std::make_unique<std::string> (std::move (svg));

    return m_menuSvgs[key] ? m_menuSvgs[key].get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetVerbGlyph
//
//  The verbs Explorer shows as buttons, with its glyphs; null for the rest.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ExplorerMenus::GetVerbGlyph (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Cut:    return s_kpszMdl2Cut;
        case CassoExplorerActions::Verb::Copy:   return s_kpszMdl2Copy;
        case CassoExplorerActions::Verb::Paste:  return s_kpszMdl2Paste;
        case CassoExplorerActions::Verb::Rename: return s_kpszMdl2Rename;
        case CassoExplorerActions::Verb::Delete: return s_kpszMdl2Delete;
        case CassoExplorerActions::Verb::Share:  return s_kpszMdl2Share;
        default:                           return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetIconOrder
//
////////////////////////////////////////////////////////////////////////////////

int ExplorerMenus::GetIconOrder (CassoExplorerActions::Verb verb)
{
    switch (verb)
    {
        case CassoExplorerActions::Verb::Cut:    return 0;
        case CassoExplorerActions::Verb::Copy:   return 1;
        case CassoExplorerActions::Verb::Paste:  return 2;
        case CassoExplorerActions::Verb::Rename: return 3;
        case CassoExplorerActions::Verb::Share:  return 4;
        default:                           return 5;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddPinMenuCommand
//
//  The shell carries the change out, so File Explorer's Quick access changes
//  with Casso Explorer's, and the pane reads the list again after it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddPinMenuCommand (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder)
{
    bool  pinned = m_window.m_browser.IsPinnedToQuickAccess (folder);



    AddMenuCommand (items, pinned ? L"Unpin from &Quick access" : L"Pin to &Quick access", [this, folder, pinned]()
    {
        HRESULT  hr = m_window.m_shellVerbs.SetPinnedToQuickAccess (m_window.GetHwnd(), folder, !pinned);

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_window.m_browser.RefreshShellRoots();
        m_window.m_treePane.RefreshTree();
    });
    m_menuCommands.back()->menuSvg = GetMenuSvg (pinned ? L"windows.unpinfromhome" : L"windows.pintohome");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddOpenRows
//
//  File Explorer's rows for a folder, in its order: Open in new tab, Open in
//  new window, then Quick access's and Start's pins. A disk image opens in a
//  tab or a window too, and has no pins.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddOpenRows (std::vector<DxuiPopupMenuItem> & items, const Location & location, bool withOpen)
{
    Win32PackagedCommands::Entry  startPin;



    //  A folder's row opens it here, first on its menu, as in File Explorer.
    if (withOpen && location.kind == Location::Kind::HostFolder)
    {
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::Open), [this, location]() { m_window.m_addressBar.GoToLocation (location); }, L"Enter");
        m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenFile;
        m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.openfolder");
    }

    AddMenuCommand (items, L"Open in new &tab", [this, location]()
    {
        m_window.m_browser.OpenInNewTab (location);
        m_window.m_listPane.FillList();
    });
    m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
    m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.opennewtab");

    if (location.kind == Location::Kind::HostFolder || location.kind == Location::Kind::DiskImage)
    {
        AddMenuCommand (items, L"Open in new &window", [this, path = location.path]() { m_window.m_tabManager.OpenInNewWindow (path); });
        m_menuCommands.back()->menuSvg = GetMenuSvg (L"windows.opennewwindow");
    }

    if (location.kind != Location::Kind::HostFolder)
    {
        return;
    }

    AddPinMenuCommand (items, location.path);

    if (m_packagedCommands.GetStartPinEntry (location.path, startPin))
    {
        AddPackagedEntry (items, startPin, { location.path });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddNewChoices
//
//  File Explorer's order: Folder and Shortcut, then the file types the shell
//  offers. A disk image is Casso's own and follows Shortcut. Inside an image
//  only a folder can be made.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddNewChoices (std::vector<DxuiPopupMenuItem> & items, const Location & target,
                                   std::shared_ptr<const DxuiCommand> folder, std::shared_ptr<const DxuiCommand> disk)
{
    std::vector<Win32PackagedCommands::NewChoice>  choices = m_packagedCommands.GetNewChoices();
    bool                                           host    = target.kind == Location::Kind::HostFolder;
    bool                                           types   = false;



    if (folder != nullptr)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (folder));
    }

    for (const Win32PackagedCommands::NewChoice & choice : choices)
    {
        if (host && _wcsicmp (choice.verb.c_str(), Win32PackagedCommands::kNewLinkVerb) == 0)
        {
            AddMenuCommand (items, choice.title.c_str(), [this, target, verb = choice.verb]() { RunNewChoice (target, verb); });
            m_menuCommands.back()->menuSvg = GetMenuSvg (L"link");
        }
    }

    if (host && disk != nullptr)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (disk));
    }

    for (const Win32PackagedCommands::NewChoice & choice : choices)
    {
        if (!host || choice.verb.empty() || choice.verb[0] != L'.')
        {
            continue;
        }

        if (!types)
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
            types = true;
        }

        AddMenuCommand (items, choice.title.c_str(), [this, target, verb = choice.verb]() { RunNewChoice (target, verb); });
        m_menuCommands.back()->menuImage = m_window.m_shellIcons.GetForExtension (choice.verb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::RunNewChoice
//
//  The new item is selected and opened for renaming, as File Explorer does,
//  except a shortcut, which is renamed when the shell's wizard finishes.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::RunNewChoice (const Location & target, const std::wstring & verb)
{
    std::wstring  created;
    HRESULT       hr      = S_OK;
    bool          named   = _wcsicmp (verb.c_str(), Win32PackagedCommands::kNewLinkVerb) != 0;



    m_window.m_addressBar.GoToLocation (target);

    hr = m_packagedCommands.CreateNew (m_window.GetHwnd(), target.path, verb, created);

    if (FAILED (hr))
    {
        m_window.ShowMessage (L"The new item could not be created.", MB_ICONERROR);
        return;
    }

    m_window.m_folderSync.RefreshAfterHostChange();

    if (!created.empty() && named)
    {
        m_window.m_fileOps.PushUndo (ExplorerFileOps::UndoStep { ExplorerFileOps::UndoStep::Kind::HostNewItem, Location(), { created } });
        if (m_window.m_listPane.SelectRowNamed (std::filesystem::path (created).filename().wstring()))
        {
            m_window.m_fileOps.BeginRename();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddMenuCommand
//
//  One row of a context menu, its command kept alive until the next menu.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddMenuCommand (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, std::function<void()> dispatch, const wchar_t * accelerator,
                                    const wchar_t * menuGlyph)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();



    command->label       = label;
    command->accelerator = accelerator;
    command->menuGlyph   = menuGlyph;
    command->dispatch    = std::move (dispatch);

    items.push_back (DxuiPopupMenuItem::ForCommand (command));
    m_menuCommands.push_back (std::move (command));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddPackagedCommands
//
//  After a separator, each command an installed app adds for the selected
//  files or folders, with the app's own title and icon. A selection of files
//  and folders together has none, as no registration covers both.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddPackagedCommands (std::vector<DxuiPopupMenuItem> & items)
{
    std::vector<std::wstring>        paths;
    Win32PackagedCommands::ItemKind  kind = Win32PackagedCommands::ItemKind::File;



    if (GetSelectionKind (paths, kind))
    {
        AddPackagedCommandsFor (items, paths, kind);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddWindowsCommands
//
//  Windows' own commands for the selected files or folders, in the first
//  group with Open and Copy as path, as File Explorer lists them. Returns
//  whether any was added.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerMenus::AddWindowsCommands (std::vector<DxuiPopupMenuItem> & items, const std::vector<std::wstring> & paths, Win32PackagedCommands::ItemKind kind)
{
    std::vector<Win32PackagedCommands::Entry>  entries = m_packagedCommands.GetWindowsEntries (paths, kind);



    for (const Win32PackagedCommands::Entry & entry : entries)
    {
        AddPackagedEntry (items, entry, paths);
    }

    return !entries.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetSelectionKind
//
//  The selection as one kind of item for the shell's commands: all files or
//  all folders. Returns false for a mixed selection, which no registration
//  covers, or for none.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerMenus::GetSelectionKind (std::vector<std::wstring> & outPaths, Win32PackagedCommands::ItemKind & outKind) const
{
    size_t  folders = 0;



    for (int row : m_window.m_browser.GetSelectedRows())
    {
        folders += m_window.m_browser.GetRows()[(size_t) row].isDirectory ? 1 : 0;
    }

    m_window.m_browser.GetSelectedHostPaths (outPaths);
    outKind = (folders != 0) ? Win32PackagedCommands::ItemKind::Folder : Win32PackagedCommands::ItemKind::File;

    return !outPaths.empty() && (folders == 0 || folders == outPaths.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddPackagedCommandsFor
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddPackagedCommandsFor (std::vector<DxuiPopupMenuItem> & items, const std::vector<std::wstring> & paths, Win32PackagedCommands::ItemKind kind)
{
    std::vector<Win32PackagedCommands::Entry>  entries = m_packagedCommands.GetEntries (paths, kind);



    if (entries.empty())
    {
        return;
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (const Win32PackagedCommands::Entry & entry : entries)
    {
        AddPackagedEntry (items, entry, paths);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddPackagedEntry
//
//  An app's title marks its mnemonic with an ampersand, as Explorer reads it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddPackagedEntry (std::vector<DxuiPopupMenuItem> & items, const Win32PackagedCommands::Entry & entry, const std::vector<std::wstring> & paths)
{
    std::shared_ptr<DxuiCommand>     command  = std::make_shared<DxuiCommand>();
    std::vector<DxuiPopupMenuItem>   children;
    bool                             enabled  = entry.enabled;



    //  Windows' own commands use File Explorer's icons for them, where the
    //  theme has one; the rest use the icon from the app's command.
    command->label     = entry.title;
    command->menuSvg   = entry.explorerIcon.empty() ? nullptr : GetMenuSvg (entry.explorerIcon.c_str());
    command->menuImage = (command->menuSvg == nullptr) ? m_window.m_shellIcons.GetForIconResource (entry.icon) : nullptr;
    command->isEnabled = [enabled]() { return enabled; };

    if (!entry.children.empty())
    {
        for (const Win32PackagedCommands::Entry & child : entry.children)
        {
            AddPackagedEntry (children, child, paths);
        }

        items.push_back (DxuiPopupMenuItem::ForSubmenu (command, std::move (children)));
        m_menuCommands.push_back (std::move (command));
        return;
    }

    //  A static verb runs through the shell's own menu for the items, by name.
    command->dispatch = [this, entry, paths]()
    {
        HRESULT  hr = entry.command ? m_packagedCommands.Invoke (m_window.GetHwnd(), entry, paths)
                                    : m_window.m_shellVerbs.RunItemVerb (m_window.GetHwnd(), paths, entry.verb.c_str());

        IGNORE_RETURN_VALUE (hr, S_OK);
    };

    items.push_back (DxuiPopupMenuItem::ForCommand (command));
    m_menuCommands.push_back (std::move (command));
}

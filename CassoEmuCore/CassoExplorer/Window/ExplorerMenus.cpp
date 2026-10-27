#include "Pch.h"

#include "CassoExplorer/Window/ExplorerMenus.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/RunningCassoRegistry.h"
#include "Seams/ILinkAnnouncements.h"
#include "Seams/Win32FirewallHelper.h"
#include "Net/FirewallMonitor.h"
#include "Net/FirewallText.h"
#include "Devices/Disk/DiskImageStore.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/CassoExplorerAbout.h"
#include "CassoExplorer/CassoExplorerDragOut.h"
#include "CassoExplorer/CassoExplorerLinkDialog.h"
#include "CassoExplorer/CassoExplorerNewDiskDialog.h"
#include "CassoExplorer/CassoExplorerProperties.h"
#include "CassoExplorer/CassoExplorerRawDialog.h"
#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "CassoExplorer/Model/CassoTargeting.h"
#include "CassoExplorer/CassoExplorerPromptDialog.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/KnownFolderStore.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "AssetBootstrap.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Config/FileAssociations.h"
#include "Config/GlobalUserPrefs.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Seams/Win32DiskFileIo.h"
#include "Seams/Win32UserClasses.h"
#include "Config/UserConfigStore.h"
#include "Config/Win32FileSystem.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"
#include "Core/MachineConfig.h"
#include "Core/MachineScanner.h"
#include "Core/PathResolver.h"
#include "Core/TextEncoding.h"
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



    if (m_window.m_tree->IsVisible() && CassoExplorerWindow::Contains (m_window.m_tree->GetBounds(), point))
    {
        items = m_window.m_tree->GetScrollbarMenu (point);
    }
    else if (m_window.m_list->IsVisible() && CassoExplorerWindow::Contains (m_window.m_list->GetBounds(), point))
    {
        items = m_window.m_list->GetScrollbarMenu (CassoExplorerWindow::ToLocal (ev, m_window.m_list->GetBounds()).positionDip);
    }
    else if (m_window.m_previewList->IsVisible() && CassoExplorerWindow::Contains (m_window.m_previewList->GetBounds(), point))
    {
        items = m_window.m_previewList->GetScrollbarMenu (CassoExplorerWindow::ToLocal (ev, m_window.m_previewList->GetBounds()).positionDip);
    }
    else if (m_window.m_textView->IsVisible() && CassoExplorerWindow::Contains (m_window.m_textView->GetBounds(), point))
    {
        items = m_window.m_textView->GetScrollbarMenu (point);
    }
    else if (m_window.m_hexView->IsVisible() && CassoExplorerWindow::Contains (m_window.m_hexView->GetBounds(), point))
    {
        items = m_window.m_hexView->GetScrollbarMenu (point);
    }
    else if (m_window.m_picture->IsVisible() && CassoExplorerWindow::Contains (m_window.m_picture->GetBounds(), point))
    {
        items = m_window.m_picture->GetScrollbarMenu (point);
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
//  ExplorerMenus::ShowListHeaderMenu
//
//  Explorer's header menu: fit the column under the pointer, fit them all,
//  then every column with a check by those showing. Name always shows.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::ShowListHeaderMenu (int x, int y, int column)
{
    std::vector<DxuiPopupMenuItem>     items;
    std::vector<DxuiListView::Column>  columns = CassoExplorerBrowser::GetColumns();



    m_window.m_menuCommands.clear();

    if (column >= 0)
    {
        AddMenuCommand (items, L"Size column to &fit", [this, column]()
        {
            m_window.m_list->FitColumnToContent ((size_t) column);
            m_window.Invalidate();
        });
    }

    AddMenuCommand (items, L"Size &all columns to fit", [this]()
    {
        m_window.m_list->FitAllColumnsToContent();
        m_window.Invalidate();
    });

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (size_t c = 0; c < columns.size() && c < m_window.m_listColumnChosen.size(); c++)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label     = columns[c].title;
        command->isChecked = [this, c]() { return m_window.m_listColumnChosen[c]; };
        command->isEnabled = [c]() { return c != (size_t) CatalogModel::Column::Name; };
        command->dispatch  = [this, c]()
        {
            m_window.m_listColumnChosen[c] = !m_window.m_listColumnChosen[c];

            //  This folder's choice, and the one others without their own follow.
            m_window.m_prefs.hiddenColumns.clear();

            for (size_t each = 0; each < m_window.m_listColumnChosen.size(); each++)
            {
                if (!m_window.m_listColumnChosen[each])
                {
                    m_window.m_prefs.hiddenColumns.push_back ((int) each);
                }
            }

            m_window.m_listPane.RememberFolderColumns();
            m_window.m_listPane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_window.m_menuCommands.push_back (std::move (command));
    }

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ShowListContextMenu
//
//  The rows come from the actions' verb list; each command runs its verb.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::ShowListContextMenu (int x, int y, int group)
{
    std::vector<DxuiPopupMenuItem>                   items;
    Location                                         location;
    std::wstring                                     folderForCasso;
    bool                                             known          = false;
    bool                                             moreOptions    = false;
    bool                                             listFocused    = false;
    bool                                             browsable      = false;
    std::vector<std::shared_ptr<const DxuiCommand>>  iconCommands;
    std::vector<DxuiPopupMenuItem>                   newChoices;
    std::shared_ptr<const DxuiCommand>               newFolder;
    std::shared_ptr<const DxuiCommand>               newDisk;
    bool                                             background     = m_window.m_browser.GetSelectedRows().empty() && group < 0;
    Location                                         shown          = m_window.m_browser.GetLocation();
    size_t                                           undoRow        = 0;



    m_window.m_menuCommands.clear();
    m_window.m_fileOps.AskCassoToDescribe();

    //  The background's menu opens on the view's own choices, as Explorer's
    //  does.
    if (background)
    {
        AddViewMenus (items);
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    //  Explorer's Undo, with the step it reverses in its label, while there
    //  is one: on the background's menu only, at the head of the group New
    //  and Properties are in, as in File Explorer.
    if (background && !m_window.m_undo.empty())
    {
        AddMenuCommand (items, ExplorerFileOps::GetUndoLabel (m_window.m_undo.back().kind), [this]() { m_window.m_fileOps.UndoLast(); }, L"Ctrl+Z");
        m_window.m_menuCommands.back()->menuSvg = GetMenuSvg (L"windows.undo");
        undoRow                                 = items.size();
    }

    //  From a group's header: open or close it, and all of them, with the
    //  keys that do the same on a focused header.
    if (group >= 0 && m_window.m_listView != DxuiListView::View::List)
    {
        bool  collapsed = m_window.m_list->IsGroupCollapsed (group);

        AddMenuCommand (items, collapsed ? L"E&xpand group" : L"C&ollapse group", [this, group, collapsed]()
        {
            m_window.m_list->SetGroupCollapsed (group, !collapsed);
            m_window.Invalidate();
        }, collapsed ? L"Right" : L"Left");
        AddMenuCommand (items, L"&Expand all groups",   [this]() { m_window.m_list->SetAllGroupsCollapsed (false); m_window.Invalidate(); });
        AddMenuCommand (items, L"Co&llapse all groups", [this]() { m_window.m_list->SetAllGroupsCollapsed (true);  m_window.Invalidate(); });
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    //  A folder or a disk image opens in a new tab, as Explorer's items do.
    if (m_window.m_browser.GetSelectedRows().size() == 1 && m_window.m_browser.TryGetRowLocation (m_window.m_browser.GetSelectedRows()[0], location))
    {
        browsable = true;

        AddOpenRows (items, location, true);

        //  A folder's rows run on into Copy as path, as in File Explorer; an
        //  image's Casso commands follow in a group of their own.
        if (location.kind != Location::Kind::HostFolder)
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        //  A linked machine's or drive's rows, as its node in the tree has.
        if (location.kind == Location::Kind::AppleDrive || (location.kind == Location::Kind::Root && location.path.starts_with (TreeModel::kLinkRootTag)))
        {
            size_t  before = items.size();

            m_window.m_links.AddLinkRows (items, true, location);

            if (items.size() > before)
            {
                items.push_back (DxuiPopupMenuItem::ForSeparator());
            }
        }
    }

    //  The open drive's whole disk, from the background of its list, as a
    //  disk with no catalog has no rows to right-click.
    if (background && ((shown.kind == Location::Kind::Root && shown.path == TreeModel::kCassoLinkRootId) || (shown.kind == Location::Kind::AppleDrive && shown.innerPath.empty())))
    {
        m_window.m_links.AddLinkRows (items, true, shown);
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    for (CassoExplorerActions::Verb verb : m_window.m_actions.GetListVerbs())
    {
        std::shared_ptr<DxuiCommand>  command;

        if (verb == CassoExplorerActions::Verb::MoreOptions)
        {
            moreOptions = true;
            continue;
        }

        //  The clipboard, rename and delete are Explorer's buttons along the
        //  menu's edge rather than rows, for a file and for an image's entry.
        //  Paste is added only with files to paste, so a menu with nothing to
        //  cut or copy and nothing to paste has no button row at all.
        if (verb == CassoExplorerActions::Verb::Paste && !m_window.m_shellVerbs.ClipboardHasFiles())
        {
            continue;
        }

        if (GetVerbGlyph (verb) != nullptr)
        {
            command           = std::make_shared<DxuiCommand>();
            command->id       = (int) verb;
            command->label    = GetVerbLabel (verb);
            command->glyph    = GetVerbGlyph (verb);
            command->menuSvg  = GetMenuSvg (GetVerbMenuSvgName (verb));
            command->dispatch = [this, verb]() { m_window.m_dispatch.RunVerb (verb); };

            iconCommands.push_back (command);
            m_window.m_menuCommands.push_back (std::move (command));
            continue;
        }

        if (verb == CassoExplorerActions::Verb::OpenWith)
        {
            AddOpenWithMenu (items);
            continue;
        }

        //  New's choices go in its own submenu, where the Refresh row starts.
        if (verb == CassoExplorerActions::Verb::NewFolder || verb == CassoExplorerActions::Verb::NewDisk)
        {
            command           = std::make_shared<DxuiCommand>();
            command->id       = (int) verb;
            command->label    = GetVerbLabel (verb);
            command->menuSvg  = GetMenuSvg ((verb == CassoExplorerActions::Verb::NewFolder) ? L"newfolder" : L"casso.floppy525");
            command->dispatch = [this, verb]() { m_window.m_dispatch.RunVerb (verb); };

            if (verb == CassoExplorerActions::Verb::NewFolder)
            {
                command->isEnabled = [this]() { return !m_window.m_browser.IsVolumeLocation() || m_window.m_browser.GetVolumeKind() == VolumeKind::ProDos; };
                newFolder          = command;
            }
            else
            {
                newDisk = command;
            }

            m_window.m_menuCommands.push_back (std::move (command));
            continue;
        }

        if (verb == CassoExplorerActions::Verb::Refresh && (newFolder != nullptr || newDisk != nullptr))
        {
            std::shared_ptr<DxuiCommand>  parent = std::make_shared<DxuiCommand>();
            bool                          dos33  = m_window.m_browser.IsVolumeLocation() && m_window.m_browser.GetVolumeKind() != VolumeKind::ProDos;

            AddNewChoices (newChoices, m_window.m_browser.GetLocation(), newFolder, newDisk);

            parent->label     = L"Ne&w";
            parent->menuGlyph = s_kpszMdl2Add;
            parent->menuSvg   = GetMenuSvg (L"windows.newitem");
            parent->isEnabled = [dos33]() { return !dos33; };

            if (!items.empty() && items.back().kind != DxuiPopupMenuItem::Kind::Separator && items.size() != undoRow)
            {
                items.push_back (DxuiPopupMenuItem::ForSeparator());
            }

            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (newChoices)));
            m_window.m_menuCommands.push_back (std::move (parent));
            newChoices.clear();
        }

        //  Refresh only places New; Explorer's menus leave it to F5.
        if (verb == CassoExplorerActions::Verb::Refresh)
        {
            continue;
        }

        command            = std::make_shared<DxuiCommand>();
        command->id        = (int) verb;
        command->label     = GetVerbLabel (verb);
        command->menuGlyph = GetVerbMenuGlyph (verb);
        command->menuSvg   = GetMenuSvg (GetVerbMenuSvgName (verb));
        command->dispatch  = [this, verb]() { m_window.m_dispatch.RunVerb (verb); };

        if (verb == CassoExplorerActions::Verb::Open)
        {
            command->menuImage   = GetOpenMenuImage (browsable, m_window.m_fileOps.GetSelectedImagePath());
            command->accelerator = L"Enter";
        }

        //  A machine known to have one drive cannot take drive 2; one not
        //  described yet is given the benefit of the doubt.
        if (verb == CassoExplorerActions::Verb::InsertDrive2)
        {
            command->isEnabled = [this]() { return m_window.m_cassoDriveCount != 1; };
        }

        if (verb == CassoExplorerActions::Verb::Paste)
        {
            command->isEnabled = [this]() { return m_window.m_shellVerbs.ClipboardHasFiles(); };
        }

        //  Copying out of an image goes by the clipboard or a drag, as in
        //  Explorer, which has no Copy to folder: the Copy verb is the button
        //  row's, and this one stands for the forms Copy as offers.
        if (verb == CassoExplorerActions::Verb::Get)
        {
            AddCopyAsMenu (items);
            continue;
        }

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_window.m_menuCommands.push_back (std::move (command));

        if (verb == CassoExplorerActions::Verb::Format)
        {
            std::vector<DxuiPopupMenuItem>  advanced;
            std::shared_ptr<DxuiCommand>    parent = std::make_shared<DxuiCommand>();

            for (CassoExplorerActions::Verb raw : { CassoExplorerActions::Verb::ReadSectors, CassoExplorerActions::Verb::WriteSectors,
                                              CassoExplorerActions::Verb::ReadBlocks,  CassoExplorerActions::Verb::WriteBlocks })
            {
                std::shared_ptr<DxuiCommand>  child = std::make_shared<DxuiCommand>();

                child->id       = (int) raw;
                child->label    = GetVerbLabel (raw);
                child->dispatch = [this, raw]() { m_window.m_dispatch.RunRawVerb (raw); };

                advanced.push_back (DxuiPopupMenuItem::ForCommand (child));
                m_window.m_menuCommands.push_back (std::move (child));
            }

            parent->label = L"&Advanced";
            parent->menuGlyph = s_kpszMdl2Setting;
            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (advanced)));
            m_window.m_menuCommands.push_back (std::move (parent));
        }
    }

    //  Casso's known folders, from a folder row or from the background of the
    //  folder being shown, as the tree offers them from a node.
    if (m_window.m_browser.GetSelectedRows().size() == 1)
    {
        folderForCasso = (m_window.m_browser.TryGetRowLocation (m_window.m_browser.GetSelectedRows()[0], location) &&
                          location.kind == Location::Kind::HostFolder) ? location.path : std::wstring();
    }
    else if (m_window.m_browser.GetSelectedRows().empty() && m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        folderForCasso = m_window.m_browser.GetLocation().path;
    }

    if (!folderForCasso.empty())
    {
        known = m_window.m_browser.IsKnownFolder (folderForCasso);

        AddMenuCommand (items, known ? L"&Remove from Casso" : L"&Add to Casso", [this, folderForCasso, known]()
        {
            m_window.m_folderSync.ChangeKnownFolder (folderForCasso, !known);
        });
    }

    //  From the current folder's background: Properties, then the commands
    //  installed apps add there, such as Open in Terminal, then the shell's
    //  own background menu.
    if (background && (shown.kind == Location::Kind::HostFolder || m_window.m_browser.IsImageLocation()) && !SearchQuery::IsId (shown.path))
    {
        AddMenuCommand (items, L"P&roperties", [this, shown]() { m_window.m_fileOps.ShowLocationProperties (shown); }, L"Alt+Enter");
        m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2Repair;
        m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.properties");
    }

    if (background && shown.kind == Location::Kind::HostFolder && !SearchQuery::IsId (shown.path))
    {
        POINT  screen = { x, y };

        ClientToScreen (m_window.GetHwnd(), &screen);

        AddPackagedCommandsFor (items, { shown.path }, Win32PackagedCommands::ItemKind::Background);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, shown, screen]()
        {
            HRESULT  hr = m_window.m_shellVerbs.ShowBackgroundMenu (m_window.GetHwnd(), shown.path, screen);

            IGNORE_RETURN_VALUE (hr, S_OK);
            m_window.m_folderSync.RefreshAfterHostChange();
        });
        m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
        m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));
    }

    //  A deleted item has no path to copy, and the bin itself describes it.
    if (!m_window.m_browser.GetSelectedRows().empty() && m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, L"P&roperties", [this]() { m_window.m_dispatch.RunRecycleBinVerb (CassoExplorerActions::Verb::Open); });
        m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2Repair;
        m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.properties");
    }
    else if (!m_window.m_browser.GetSelectedRows().empty())
    {
        std::vector<std::wstring>        selectedPaths;
        Win32PackagedCommands::ItemKind  kind    = Win32PackagedCommands::ItemKind::File;
        bool                             windows = false;

        //  Windows' own commands lead into Copy as path in one group, as in
        //  File Explorer; without them the group starts here.
        if (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder && GetSelectionKind (selectedPaths, kind))
        {
            windows = AddWindowsCommands (items, selectedPaths, kind);
        }

        if (!windows)
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        AddMenuCommand (items, L"Copy as &path", [this]() { m_window.m_fileOps.CopySelectedPaths(); }, L"Ctrl+Shift+C", s_kpszMdl2CopyPath);
        m_window.m_menuCommands.back()->menuSvg = GetMenuSvg (L"windows.copyaspath");

        if (m_window.m_browser.GetSelectedRows().size() == 1)
        {
            AddMenuCommand (items, L"P&roperties", [this]()
            {
                if (!m_window.m_browser.GetSelectedRows().empty())
                {
                    m_window.m_fileOps.ShowRowProperties (m_window.m_browser.GetSelectedRows()[0]);
                }
            }, L"Alt+Enter");
            m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2Repair;
            m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.properties");
        }
        else if (!selectedPaths.empty())
        {
            AddMenuCommand (items, L"P&roperties", [this, selectedPaths]() { m_window.m_fileOps.ShowHostProperties (selectedPaths); }, L"Alt+Enter");
            m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2Repair;
            m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.properties");
        }
    }

    //  In Explorer's order, whatever order the verbs came in.
    std::stable_sort (iconCommands.begin(), iconCommands.end(), [] (const auto & a, const auto & b)
    {
        return GetIconOrder ((CassoExplorerActions::Verb) a->id) < GetIconOrder ((CassoExplorerActions::Verb) b->id);
    });

    if (!iconCommands.empty())
    {
        items.insert (items.begin(), DxuiPopupMenuItem::ForSeparator());
        items.insert (items.begin(), DxuiPopupMenuItem::ForIconRow (std::move (iconCommands)));
    }

    //  The commands installed apps add for these items, as Explorer lists
    //  them, above the shell's own menu.
    if (moreOptions && m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        AddPackagedCommands (items);
    }

    //  Last, as Explorer has it: everything else Windows and other programs
    //  offer for these items, in the shell's own menu.
    if (moreOptions)
    {
        POINT  screen = { x, y };

        ClientToScreen (m_window.GetHwnd(), &screen);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, screen]()
        {
            std::vector<std::wstring>  paths;
            HRESULT                    hr = S_OK;

            if (m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin)
            {
                m_window.m_browser.GetSelectedRecycledIds (paths);
                hr = m_window.m_shellVerbs.ShowRecycledMenu (m_window.GetHwnd(), paths, screen);
                m_window.m_folderSync.RefreshAfterHostChange();
            }
            else if (m_window.m_fileOps.TryGetSelectedShellIds (paths))
            {
                hr = m_window.m_shellVerbs.ShowShellMenu (m_window.GetHwnd(), paths, screen, [this]() { m_window.m_dispatch.RunVerb (CassoExplorerActions::Verb::Rename); });
            }

            IGNORE_RETURN_VALUE (hr, S_OK);
        });
        m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
        m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));
    }

    //  The menu takes the focus look from the list while it is open, as
    //  Explorer's does: the item keeps its selection, drawn unfocused, and
    //  loses its focus border until the menu goes.
    listFocused = m_window.m_list->IsListFocused();

    if (listFocused)
    {
        m_window.m_list->SetListFocused (false);
        m_window.Invalidate();
    }

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items), [this, listFocused] (bool)
    {
        if (listFocused && m_window.m_focus == ExplorerInput::Pane::List)
        {
            m_window.m_list->SetListFocused (true);
            m_window.Invalidate();
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddOpenWithMenu
//
//  The programs Windows recommends for the selected file, then its own
//  dialog for any other.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddOpenWithMenu (std::vector<DxuiPopupMenuItem> & items)
{
    std::wstring                          path   = m_window.m_fileOps.GetSelectedImagePath();
    std::vector<IShellItemVerbs::Handler> handlers;
    std::vector<DxuiPopupMenuItem>        children;
    std::shared_ptr<DxuiCommand>          parent = std::make_shared<DxuiCommand>();
    HRESULT                               hr     = S_OK;
    size_t                                i      = 0;



    if (path.empty())
    {
        return;
    }

    hr = m_window.m_shellVerbs.GetOpenWithHandlers (path, handlers);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (i = 0; i < handlers.size(); i++)
    {
        AddMenuCommand (children, ExplorerAddressBar::EscapeMnemonics (handlers[i].name).c_str(), [this, path, i]()
        {
            HRESULT  opened = m_window.m_shellVerbs.OpenWith (m_window.GetHwnd(), path, i);

            IGNORE_RETURN_VALUE (opened, S_OK);
        });
    }

    if (!children.empty())
    {
        children.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    AddMenuCommand (children, L"&Choose another app", [this, path]()
    {
        HRESULT  chosen = m_window.m_shellVerbs.ChooseOtherApp (m_window.GetHwnd(), path);

        IGNORE_RETURN_VALUE (chosen, S_OK);
    });

    parent->label = GetVerbLabel (CassoExplorerActions::Verb::OpenWith);
    parent->menuGlyph = s_kpszMdl2OpenWith;
    parent->menuSvg   = GetMenuSvg (L"windows.openwith");
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (children)));
    m_window.m_menuCommands.push_back (std::move (parent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::IsListVerbOffered
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerMenus::IsListVerbOffered (CassoExplorerActions::Verb verb) const
{
    std::vector<CassoExplorerActions::Verb>  verbs = m_window.m_actions.GetListVerbs();



    return std::find (verbs.begin(), verbs.end(), verb) != verbs.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetSortChoices
//
//  One row for each column in the list here, in Explorer's order, then the
//  sort direction.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerMenus::GetSortChoices()
{
    std::vector<DxuiPopupMenuItem>  items;
    size_t                          column = 0;



    for (column = 0; column < CassoExplorerBrowser::GetColumns().size(); column++)
    {
        bool  chosen = column >= m_window.m_listColumnChosen.size() || m_window.m_listColumnChosen[column];

        if (!ExplorerListPane::IsListColumnShown (column, chosen, m_window.m_browser.GetLocation().kind, SearchQuery::IsId (m_window.m_browser.GetLocation().path)))
        {
            continue;
        }

        items.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (CassoExplorerCommands::kSortByColumn + (int) column)));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (CassoExplorerCommands::kSortAscending)));
    items.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (CassoExplorerCommands::kSortDescending)));

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetGroupChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerMenus::GetGroupChoices()
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int field : { 1, 2, 3, 4, 0 })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (CassoExplorerCommands::kGroupByField + field)));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (CassoExplorerCommands::kGroupAscending)));
    items.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (CassoExplorerCommands::kGroupDescending)));

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetViewChoices
//
//  In Explorer's order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerMenus::GetViewChoices()
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int view : { 1, 2, 3, 4, 5, 0, 6, 7 })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (CassoExplorerCommands::kViewFirst + view)));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::SetCommandBarDropDowns
//
//  The rows of New, Sort, View and Theme. They are commands, so each reads
//  whether it is enabled and checked when the menu draws.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::SetCommandBarDropDowns()
{
    std::vector<DxuiPopupMenuItem>  newItems;
    std::vector<DxuiPopupMenuItem>  sortItems = GetSortChoices();
    std::vector<DxuiPopupMenuItem>  viewItems = GetViewChoices();
    std::vector<DxuiPopupMenuItem>  themeItems;



    AddNewChoices (newItems, m_window.m_browser.GetLocation(), m_window.m_commands.Find (CassoExplorerCommands::kNewFolder),
                   m_window.m_commands.Find (CassoExplorerCommands::kNewDisk));

    //  Group by is at the foot of Explorer's Sort menu.
    sortItems.push_back (DxuiPopupMenuItem::ForSeparator());
    sortItems.push_back (DxuiPopupMenuItem::ForSubmenu (m_window.m_commands.Find (CassoExplorerCommands::kGroupBy), GetGroupChoices()));

    viewItems.push_back (DxuiPopupMenuItem::ForSeparator());
    viewItems.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (CassoExplorerCommands::kOptions)));

    for (int id : { (int) CassoExplorerCommands::kThemeSkeuomorphic, (int) CassoExplorerCommands::kThemeRetroTerminal, (int) CassoExplorerCommands::kThemeDarkModern,
                    (int) CassoExplorerCommands::kThemeDark, (int) CassoExplorerCommands::kThemeLight, (int) CassoExplorerCommands::kThemeSystem })
    {
        themeItems.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (id)));
    }

    m_window.m_commandBar->SetDropDownItems (CassoExplorerCommands::kNew,   std::move (newItems));
    m_window.m_commandBar->SetDropDownItems (CassoExplorerCommands::kSort,  std::move (sortItems));
    m_window.m_commandBar->SetDropDownItems (CassoExplorerCommands::kView,  std::move (viewItems));
    m_window.m_commandBar->SetDropDownItems (CassoExplorerCommands::kTheme, std::move (themeItems));

    //  Each theme is shown as the pointer passes over its row; leaving the
    //  menu without a choice puts back the one it opened on.
    m_window.m_commandBar->SetDropDownSinks (CassoExplorerCommands::kTheme, [this] (int index) { m_window.m_layout.PreviewTheme (index); }, nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ShowViewOnButton
//
//  The View button shows the current view's icon, as Explorer's does.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::ShowViewOnButton()
{
    if (m_window.m_commandBar != nullptr)
    {
        m_window.m_commandBar->SetEntryVectorIcon (CassoExplorerCommands::kView, CassoExplorerCommands::GetViewButtonIcon (m_window.m_listView));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ShowTreeContextMenu
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::ShowTreeContextMenu (int x, int y, const std::wstring & id)
{
    std::vector<DxuiPopupMenuItem>  items;
    std::wstring                    folder;
    Location                        location;
    bool                            hasFolder   = m_window.m_browser.TryGetNodePath (id, folder);
    bool                            hasLocation = m_window.m_browser.TryGetNodeLocation (id, location);
    const DxuiTreeNode            * node        = m_window.m_tree->GetNodeAt (m_window.m_tree->FindRowById (id));



    m_window.m_menuCommands.clear();

    //  A known folder that is not there offers only its removal; every other
    //  command acts on a folder that cannot be opened.
    if (node != nullptr && node->dimmed)
    {
        hasLocation = false;
    }

    //  A folder on the disk has Explorer's row of Cut, Copy, Paste, Rename and
    //  Delete.
    if (hasLocation && ((location.kind == Location::Kind::HostFolder && location.path.size() > 3) || location.kind == Location::Kind::DiskImage))
    {
        AddTreeIconRow (items, location.path, id);
    }

    //  Explorer's first command expands or collapses the node, in a group
    //  of its own.
    if (node != nullptr && (!node->childrenLoaded || !node->children.empty()))
    {
        AddMenuCommand (items, node->expanded ? L"&Collapse" : L"&Expand", [this, id, expand = !node->expanded]()
        {
            int   row     = m_window.m_tree->FindRowById (id);
            bool  changed = (row >= 0) && m_window.m_tree->SetRowExpanded (row, expand);

            if (changed)
            {
                m_window.Invalidate();
            }
        });
        m_window.m_menuCommands.back()->menuSvg = GetMenuSvg (node->expanded ? L"windows.collapse" : L"windows.expand");
    }

    //  The open rows follow Collapse in its group, with no Open, as in File
    //  Explorer's navigation pane.
    if (hasLocation)
    {
        AddOpenRows (items, location, false);
    }

    //  A disk image's Casso commands, as its row in the list has them.
    if (hasLocation && location.kind == Location::Kind::DiskImage)
    {
        std::wstring  image = location.path;

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::InsertDrive1),   [this, image]() { m_window.m_fileOps.InsertIntoDrive (image, 1); });
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::InsertDrive2),   [this, image]() { m_window.m_fileOps.InsertIntoDrive (image, 2); });
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::OpenInNewCasso), [this, image]() { m_window.m_fileOps.OpenInNewCasso (image); });
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::ExtractFiles),   [this, image]() { m_window.m_dispatch.ExtractImageFiles (image); });
        m_window.m_menuCommands.back()->menuSvg = GetMenuSvg (L"windows.compressedfile.extract");
    }

    if (hasFolder && m_window.m_browser.CanRemoveFromCasso (id))
    {
        AddMenuCommand (items, L"&Remove from Casso", [this, folder]() { m_window.m_folderSync.ChangeKnownFolder (folder, false); });
    }
    else if (hasFolder && m_window.m_browser.CanAddToCasso (id))
    {
        AddMenuCommand (items, L"&Add to Casso", [this, folder]() { m_window.m_folderSync.ChangeKnownFolder (folder, true); });
    }

    m_window.m_links.AddLinkRows (items, hasLocation, location);

    //  The Recycle Bin's node empties it, as Explorer's does.
    if (hasLocation && location.kind == Location::Kind::RecycleBin)
    {
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::EmptyRecycleBin), [this]()
        {
            HRESULT  hr = m_window.m_shellVerbs.EmptyRecycleBin (m_window.GetHwnd());

            IGNORE_RETURN_VALUE (hr, S_OK);
            m_window.m_folderSync.RefreshAfterHostChange();
        });
        m_window.m_menuCommands.back()->menuSvg = GetMenuSvg (GetVerbMenuSvgName (CassoExplorerActions::Verb::EmptyRecycleBin));
    }
    else if (hasLocation && location.kind != Location::Kind::Root && location.kind != Location::Kind::AppleDrive)
    {
        //  One group, as in File Explorer: Windows' commands, Copy as path,
        //  New and Properties follow the open and pin rows.
        if (location.kind == Location::Kind::HostFolder && location.path.size() > 3)
        {
            AddWindowsCommands (items, { location.path }, Win32PackagedCommands::ItemKind::Folder);
        }

        AddMenuCommand (items, L"Copy as &path", [this, location]()
        {
            DxuiClipboard::SetText (m_window.GetHwnd(), L"\"" + BrowserModel::FormatAddress (location) + L"\"");
        }, L"Ctrl+Shift+C", s_kpszMdl2CopyPath);
        m_window.m_menuCommands.back()->menuSvg = GetMenuSvg (L"windows.copyaspath");

        if (location.kind == Location::Kind::HostFolder)
        {
            std::shared_ptr<DxuiCommand>    parent    = std::make_shared<DxuiCommand>();
            std::shared_ptr<DxuiCommand>    newFolder = std::make_shared<DxuiCommand>();
            std::shared_ptr<DxuiCommand>    newDisk   = std::make_shared<DxuiCommand>();
            std::vector<DxuiPopupMenuItem>  choices;

            newFolder->label    = GetVerbLabel (CassoExplorerActions::Verb::NewFolder);
            newFolder->menuSvg  = GetMenuSvg (L"newfolder");
            newFolder->dispatch = [this, location]() { m_window.m_addressBar.GoToLocation (location); m_window.m_dispatch.RunVerb (CassoExplorerActions::Verb::NewFolder); };
            newDisk->label      = GetVerbLabel (CassoExplorerActions::Verb::NewDisk);
            newDisk->menuSvg    = GetMenuSvg (L"casso.floppy525");
            newDisk->dispatch   = [this, location]() { m_window.m_addressBar.GoToLocation (location); m_window.m_dispatch.RunVerb (CassoExplorerActions::Verb::NewDisk); };

            AddNewChoices (choices, location, newFolder, newDisk);

            parent->label     = L"Ne&w";
            parent->menuGlyph = s_kpszMdl2Add;
            parent->menuSvg   = GetMenuSvg (L"windows.newitem");
            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (choices)));

            m_window.m_menuCommands.push_back (std::move (parent));
            m_window.m_menuCommands.push_back (std::move (newFolder));
            m_window.m_menuCommands.push_back (std::move (newDisk));
        }

        AddMenuCommand (items, L"P&roperties", [this, location]() { m_window.m_fileOps.ShowLocationProperties (location); }, L"Alt+Enter");
        m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2Repair;
        m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.properties");
    }

    //  A folder on the disk has the commands installed apps add, then the
    //  shell's own menu, as in Explorer's.
    if (hasLocation && location.kind == Location::Kind::HostFolder)
    {
        POINT  screen = { x, y };

        ClientToScreen (m_window.GetHwnd(), &screen);

        AddPackagedCommandsFor (items, { location.path }, (location.path.size() <= 3) ? Win32PackagedCommands::ItemKind::Drive : Win32PackagedCommands::ItemKind::Folder);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, location, screen]()
        {
            HRESULT  hr = m_window.m_shellVerbs.ShowShellMenu (m_window.GetHwnd(), { location.path }, screen, {});

            IGNORE_RETURN_VALUE (hr, S_OK);
        });
        m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
        m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));
    }

    if (!items.empty())
    {
        DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddTreeIconRow
//
//  Add a folder's Cut, Copy, Paste, Rename and Delete as buttons atop the
//  menu, as File Explorer does for a node. Cut and Copy place the folder on
//  the clipboard as a file, Paste goes into it while the clipboard has
//  files, Rename edits the node's label, and Delete moves the folder to the
//  Recycle Bin.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddTreeIconRow (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder, const std::wstring & id)
{
    static constexpr CassoExplorerActions::Verb  kIconVerbs[] =
    {
        CassoExplorerActions::Verb::Cut,
        CassoExplorerActions::Verb::Copy,
        CassoExplorerActions::Verb::Paste,
        CassoExplorerActions::Verb::Rename,
        CassoExplorerActions::Verb::Delete,
    };
    std::vector<std::shared_ptr<const DxuiCommand>>  iconCommands;



    for (CassoExplorerActions::Verb verb : kIconVerbs)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        //  Paste is added only with files to paste, as on a folder's row.
        if (verb == CassoExplorerActions::Verb::Paste && !m_window.m_shellVerbs.ClipboardHasFiles())
        {
            continue;
        }

        command->id       = (int) verb;
        command->label    = GetVerbLabel (verb);
        command->glyph    = GetVerbGlyph (verb);
        command->menuSvg  = GetMenuSvg (GetVerbMenuSvgName (verb));
        command->dispatch = [this, verb, folder, id]()
        {
            HRESULT  hr = S_OK;

            if (verb == CassoExplorerActions::Verb::Paste)
            {
                m_window.m_fileOps.PasteHere (folder);
                return;
            }

            if (verb == CassoExplorerActions::Verb::Rename)
            {
                m_window.m_fileOps.BeginTreeRename (id);
                return;
            }

            if (verb == CassoExplorerActions::Verb::Delete)
            {
                hr = m_window.m_shellVerbs.Recycle (m_window.GetHwnd(), { folder });
                m_window.m_folderSync.RefreshAfterHostChange();
            }
            else
            {
                hr = m_window.m_shellVerbs.PlaceOnClipboard (m_window.GetHwnd(), { folder }, verb == CassoExplorerActions::Verb::Cut);
            }

            IGNORE_RETURN_VALUE (hr, S_OK);
        };
        iconCommands.push_back (command);
        m_window.m_menuCommands.push_back (std::move (command));
    }

    items.push_back (DxuiPopupMenuItem::ForIconRow (std::move (iconCommands)));
    items.push_back (DxuiPopupMenuItem::ForSeparator());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ShowTreeEmptyMenu
//
//  The pane's own options, as Explorer's menu below its last node has them,
//  each kept across runs.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::ShowTreeEmptyMenu (int x, int y)
{
    std::vector<DxuiPopupMenuItem>  items;



    m_window.m_menuCommands.clear();

    AddNavPaneToggle (items, L"Show This &PC",          &CassoExplorerPrefs::navShowThisPc);
    AddNavPaneToggle (items, L"Show &Network",          &CassoExplorerPrefs::navShowNetwork);
    AddNavPaneToggle (items, L"Show &libraries",        &CassoExplorerPrefs::navShowLibraries);
    AddNavPaneToggle (items, L"Show &all folders",      &CassoExplorerPrefs::navShowAllFolders);
    AddNavPaneToggle (items, L"&Expand to open folder", &CassoExplorerPrefs::navExpandToCurrent);

    if (m_window.m_context.links != nullptr)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        AddMenuCommand (items, L"Add a &CassoLink machine...", [this]() { m_window.m_links.EditLink (std::string()); });
    }

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddNavPaneToggle
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddNavPaneToggle (std::vector<DxuiPopupMenuItem> & items, const wchar_t * label, CassoExplorerPrefs::NavOption option)
{
    //  The first change parts this option from File Explorer's for good.
    AddMenuCommand (items, label, [this, option]()
    {
        m_window.m_prefs.*option = !IsNavOptionOn (option);
        ApplyNavPaneOptions();
        m_window.m_listPane.RefreshTree();

        if (IsNavOptionOn (&CassoExplorerPrefs::navExpandToCurrent))
        {
            m_window.m_listPane.RevealLocationInTree();
        }
    });
    m_window.m_menuCommands.back()->isChecked = [this, option]() { return IsNavOptionOn (option); };
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::IsNavOptionOn
//
//  Casso Explorer's own value where the user set one; otherwise File
//  Explorer's, read from the shell for the roots and from its settings for
//  the other two.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerMenus::IsNavOptionOn (CassoExplorerPrefs::NavOption option) const
{
    const TreeModel &  tree = m_window.m_browser.GetTreeModel();



    if (option == &CassoExplorerPrefs::navShowThisPc)
    {
        return tree.IsNavRootShown (TreeModel::NavRoot::ThisPc);
    }

    if (option == &CassoExplorerPrefs::navShowNetwork)
    {
        return tree.IsNavRootShown (TreeModel::NavRoot::Network);
    }

    if (option == &CassoExplorerPrefs::navShowLibraries)
    {
        return tree.IsNavRootShown (TreeModel::NavRoot::Libraries);
    }

    if (option == &CassoExplorerPrefs::navShowAllFolders)
    {
        return m_window.m_prefs.navShowAllFolders.value_or (m_window.m_explorerOptions.paneShowsAllFolders);
    }

    return m_window.m_prefs.navExpandToCurrent.value_or (m_window.m_explorerOptions.paneExpandsToCurrent);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ApplyNavPaneOptions
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::ApplyNavPaneOptions()
{
    TreeModel::NavPaneOptions  options;



    options.showThisPc     = m_window.m_prefs.navShowThisPc;
    options.showNetwork    = m_window.m_prefs.navShowNetwork;
    options.showLibraries  = m_window.m_prefs.navShowLibraries;
    options.showAllFolders = IsNavOptionOn (&CassoExplorerPrefs::navShowAllFolders);

    m_window.m_browser.GetTreeModel().SetNavPaneOptions (options);
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
        m_window.m_listPane.RefreshTree();
    });
    m_window.m_menuCommands.back()->menuSvg = GetMenuSvg (pinned ? L"windows.unpinfromhome" : L"windows.pintohome");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddViewMenus
//
//  View, Sort by and Group by, as the first rows of Explorer's background
//  menu, with the command bar's rows.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddViewMenus (std::vector<DxuiPopupMenuItem> & items)
{
    std::shared_ptr<DxuiCommand>  view  = std::make_shared<DxuiCommand>();
    std::shared_ptr<DxuiCommand>  sort  = std::make_shared<DxuiCommand>();
    std::shared_ptr<DxuiCommand>  group = std::make_shared<DxuiCommand>();



    view->label    = L"&View";
    view->menuSvg  = GetMenuSvg (L"View");
    sort->label    = L"S&ort by";
    sort->menuSvg  = GetMenuSvg (L"SortBy");
    group->label   = L"Grou&p by";
    group->menuSvg = GetMenuSvg (L"windows.groupbycolumn");

    items.push_back (DxuiPopupMenuItem::ForSubmenu (view,  GetViewChoices()));
    items.push_back (DxuiPopupMenuItem::ForSubmenu (sort,  GetSortChoices()));
    items.push_back (DxuiPopupMenuItem::ForSubmenu (group, GetGroupChoices()));

    m_window.m_menuCommands.push_back (std::move (view));
    m_window.m_menuCommands.push_back (std::move (sort));
    m_window.m_menuCommands.push_back (std::move (group));
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
        m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenFile;
        m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.openfolder");
    }

    AddMenuCommand (items, L"Open in new &tab", [this, location]()
    {
        m_window.m_browser.OpenInNewTab (location);
        m_window.m_listPane.FillList();
    });
    m_window.m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
    m_window.m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.opennewtab");

    if (location.kind == Location::Kind::HostFolder || location.kind == Location::Kind::DiskImage)
    {
        AddMenuCommand (items, L"Open in new &window", [this, path = location.path]() { m_window.m_tabStrip.OpenInNewWindow (path); });
        m_window.m_menuCommands.back()->menuSvg = GetMenuSvg (L"windows.opennewwindow");
    }

    if (location.kind != Location::Kind::HostFolder)
    {
        return;
    }

    AddPinMenuCommand (items, location.path);

    if (m_window.m_packagedCommands.GetStartPinEntry (location.path, startPin))
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
    std::vector<Win32PackagedCommands::NewChoice>  choices = m_window.m_packagedCommands.GetNewChoices();
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
            m_window.m_menuCommands.back()->menuSvg = GetMenuSvg (L"link");
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
        m_window.m_menuCommands.back()->menuImage = m_window.m_shellIcons.GetForExtension (choice.verb);
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

    hr = m_window.m_packagedCommands.CreateNew (m_window.GetHwnd(), target.path, verb, created);

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
    m_window.m_menuCommands.push_back (std::move (command));
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
    std::vector<Win32PackagedCommands::Entry>  entries = m_window.m_packagedCommands.GetWindowsEntries (paths, kind);



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
    std::vector<Win32PackagedCommands::Entry>  entries = m_window.m_packagedCommands.GetEntries (paths, kind);



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
        m_window.m_menuCommands.push_back (std::move (command));
        return;
    }

    //  A static verb runs through the shell's own menu for the items, by name.
    command->dispatch = [this, entry, paths]()
    {
        HRESULT  hr = entry.command ? m_window.m_packagedCommands.Invoke (m_window.GetHwnd(), entry, paths)
                                    : m_window.m_shellVerbs.RunItemVerb (m_window.GetHwnd(), paths, entry.verb.c_str());

        IGNORE_RETURN_VALUE (hr, S_OK);
    };

    items.push_back (DxuiPopupMenuItem::ForCommand (command));
    m_window.m_menuCommands.push_back (std::move (command));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::AddCopyAsMenu
//
//  The button row's Copy puts the selected files on the clipboard as real
//  files, in the style the Options dialog sets, and Copy as offers each style
//  for this copy alone. A paste into Explorer then writes them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddCopyAsMenu (std::vector<DxuiPopupMenuItem> & items)
{
    static constexpr struct { const wchar_t * label; HostFileNaming::Style style; }  kStyles[] =
    {
        { L"&Descriptive",  HostFileNaming::Style::Descriptive },
        { L"&CiderPress",   HostFileNaming::Style::CiderPress  },
        { L"&AppleSingle",  HostFileNaming::Style::AppleSingle },
    };
    std::vector<DxuiPopupMenuItem>  choices;
    std::shared_ptr<DxuiCommand>    parent = std::make_shared<DxuiCommand>();



    for (const auto & row : kStyles)
    {
        std::shared_ptr<DxuiCommand>  child = std::make_shared<DxuiCommand>();
        HostFileNaming::Style         style = row.style;

        child->label    = row.label;
        child->dispatch = [this, style]() { m_window.m_fileOps.CopyEntriesToClipboard (style); };

        choices.push_back (DxuiPopupMenuItem::ForCommand (child));
        m_window.m_menuCommands.push_back (std::move (child));
    }

    parent->label = L"Cop&y as";
    parent->menuGlyph = s_kpszMdl2Copy;
    parent->menuSvg   = GetMenuSvg (L"windows.copy");
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (choices)));
    m_window.m_menuCommands.push_back (std::move (parent));
}

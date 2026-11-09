#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "resource.h"
#include "CassoExplorer/CassoExplorerCommands.h"
#include "CassoExplorer/Window/Managers/ExplorerLayout.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Managers/ExplorerTabManager.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "Seams/Win32ShellIcons.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Managers/ExplorerRename.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ExplorerMenus
//
////////////////////////////////////////////////////////////////////////////////

ExplorerMenus::ExplorerMenus (ExplorerContext & context)
    : m_context (context)
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



    if (ExplorerLayout::Contains (m_context.treePane.GetBounds(), point))
    {
        items = m_context.treePane.GetScrollbarMenu (point);
    }
    else if (ExplorerLayout::Contains (m_context.listPane.GetBounds(), point))
    {
        items = m_context.listPane.GetScrollbarMenu (ev);
    }
    else
    {
        items = m_context.preview.GetScrollbarMenu (ev);
    }

    if (items.empty())
    {
        return false;
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), point.x, point.y, std::move (items), [this] (bool) { m_context.window.Invalidate(); });

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
        case CassoExplorerActions::Verb::Krunch:         return L"&Krunch disk";
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
        case CassoExplorerActions::Verb::ReadAsCpm:      return L"Read as C&P/M";
        case CassoExplorerActions::Verb::DetectFilesystem: return L"Detect the file s&ystem";
        case CassoExplorerActions::Verb::Lock:           return L"&Lock";
        case CassoExplorerActions::Verb::Unlock:         return L"&Unlock";
        case CassoExplorerActions::Verb::ConvertTo:      return L"Con&vert to";
        case CassoExplorerActions::Verb::InspectDiskImage: return L"&Inspect disk image";
        case CassoExplorerActions::Verb::CompareDiskImages: return L"Co&mpare disk images";
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
        case CassoExplorerActions::Verb::Krunch:         return s_kpszMdl2BackToWindow;
        case CassoExplorerActions::Verb::Refresh:        return s_kpszMdl2Refresh;
        case CassoExplorerActions::Verb::MoreOptions:    return s_kpszMdl2OpenInNewWindow;
        case CassoExplorerActions::Verb::Lock:           return s_kpszMdl2Lock;
        case CassoExplorerActions::Verb::Unlock:         return s_kpszMdl2Unlock;
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
        m_context.commands.SetMenuSvg (CassoExplorerCommands::kViewFirst + (int) view, GetMenuSvg (name));
    }

    m_context.commands.SetMenuSvg (CassoExplorerCommands::kTogglePreview,       GetMenuSvg (L"windows.previewpane"));
    m_context.commands.SetMenuSvg (CassoExplorerCommands::kNewFolder,           GetMenuSvg (L"newfolder"));
    m_context.commands.SetMenuSvg (CassoExplorerCommands::kNewDisk,             GetMenuSvg (L"casso.floppy525"));
    m_context.commands.SetMenuSvg (CassoExplorerCommands::kOptions,             GetMenuSvg (L"windows.folderoptions"));
    m_context.commands.SetMenuSvg (CassoExplorerCommands::kAddLink,             GetMenuSvg (L"windows.addnetworklocation"));
    m_context.commands.SetMenuSvg (CassoExplorerCommands::kAbout,               GetMenuSvg (L"casso.about"));
    m_context.commands.SetMenuSvg (CassoExplorerCommands::kTheme,               GetMenuSvg (L"Personalize"));
    m_context.commands.SetMenuSvg (CassoExplorerCommands::kReadDiskToImage,     GetMenuSvg (L"casso.floppy525"));
    m_context.commands.SetMenuSvg (CassoExplorerCommands::kWriteImageToDisk,    GetMenuSvg (L"windows.discimage.burn"));
    m_context.commands.SetMenuSvg (CassoExplorerCommands::kCancelLinkOperation, GetMenuSvg (L"windows.close"));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetVerbMenuSvgName
//
//  File Explorer's own menu icons, read from its package on this machine
//  for the theme in use, or null where it has none; Casso's own art for a
//  name that starts with "casso.". Held for the window's life.
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
        case CassoExplorerActions::Verb::OpenInNewCasso: return L"windows.openinnewwindow";
        case CassoExplorerActions::Verb::Krunch:       return L"Windows.Zip.Action";
        case CassoExplorerActions::Verb::Lock:         return L"casso.lock";
        case CassoExplorerActions::Verb::Unlock:       return L"casso.unlock";
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
//  The icon of the program that opens an item, for its Open row: Casso
//  Explorer's own for what it browses, the file's default program's for
//  anything else; null when there is none. Held for the window's life.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> ExplorerMenus::GetOpenMenuImage (bool browsable, const std::wstring & path)
{
    int                                   sizePx  = m_context.scaler.ToPx (kMenuIconDip);
    std::wstring                          program;
    std::wstring                          packaged;
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

        //  A packaged app has no program file, and its icon is its Apps folder item's.
        if (FAILED (hrAssoc))
        {
            wchar_t  appId[256] = {};

            length  = (DWORD) std::size (appId);
            hrAssoc = extension.empty() ? E_FAIL : AssocQueryStringW (ASSOCF_INIT_IGNOREUNKNOWN, ASSOCSTR_APPID, extension.c_str(), nullptr, appId, &length);

            if (FAILED (hrAssoc))
            {
                return nullptr;
            }

            packaged = std::wstring (L"shell:AppsFolder\\") + appId;
        }

        program = packaged.empty() ? std::wstring (exe) : packaged;
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
    else if (!packaged.empty())
    {
        PIDLIST_ABSOLUTE  pidl = nullptr;
        SHFILEINFOW       info = {};

        hr = SHParseDisplayName (packaged.c_str(), nullptr, &pidl, 0, nullptr);

        if (SUCCEEDED (hr))
        {
            DWORD_PTR  got = SHGetFileInfoW ((LPCWSTR) pidl, 0, &info, sizeof (info), SHGFI_PIDL | SHGFI_ICON | SHGFI_SMALLICON);

            icon = (got != 0) ? info.hIcon : nullptr;
            CoTaskMemFree (pidl);
        }
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
//  An icon-row button's tip, as Explorer words it: "Cut (Ctrl+X)".
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
    bool          dark  = DxuiColor::ComputeRelativeLuminance (m_context.theme->Background()) < 0.5f;
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
        static constexpr std::pair<const wchar_t *, const char *>  s_kArt[] =
        {
            { L"casso.floppy525", kFloppy525Svg },
            { L"casso.about",     kAboutSvg     },
            { L"casso.lock",      kLockSvg      },
            { L"casso.unlock",    kUnlockSvg    },
        };

        for (const auto & [artName, art] : s_kArt)
        {
            if (wcscmp (name, artName) == 0)
            {
                svg = ApplyIconPalette (art, dark);
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

        //  A color icon is in the folder above the contrast themes.
        if (!file)
        {
            file.clear();
            file.open (std::wstring (windows, length) + kExplorerIconFolder + L"..\\" + name + L".svg", std::ios::binary);
        }

        if (file)
        {
            svg.assign (std::istreambuf_iterator<char> (file), std::istreambuf_iterator<char>());
        }

        //  An icon the light theme has no file for takes its colors from the dark.
        if (svg.empty() && !dark)
        {
            std::ifstream  darkFile (std::wstring (windows, length) + kExplorerIconFolder + L"theme-dark\\" + name + L".svg", std::ios::binary);

            if (darkFile)
            {
                svg = ConvertDarkToLight (std::string (std::istreambuf_iterator<char> (darkFile), std::istreambuf_iterator<char>()));
            }
        }
    }

    m_menuSvgs[key] = svg.empty() ? nullptr : std::make_unique<std::string> (std::move (svg));

    return m_menuSvgs[key] ? m_menuSvgs[key].get() : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ApplyIconPalette
//
//  The one place the theme's icon colors are set: Casso's art has them as
//  tokens, and File Explorer's dark icons are mapped to its light colors by
//  ConvertDarkToLight below.
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerMenus::ApplyIconPalette (std::string svg, bool dark)
{
    for (auto [from, to] : { std::pair<const char *, const char *> { "{INK}",  dark ? kDarkInk    : kLightInk    },
                             std::pair<const char *, const char *> { "{ACC}",  dark ? kDarkAccent : kLightAccent },
                             std::pair<const char *, const char *> { "{BODY}", dark ? "none"      : kLightBody   } })
    {
        for (size_t at = svg.find (from); at != std::string::npos; at = svg.find (from, at))
        {
            svg.replace (at, strlen (from), to);
        }
    }

    return svg;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::ConvertDarkToLight
//
////////////////////////////////////////////////////////////////////////////////

std::string ExplorerMenus::ConvertDarkToLight (std::string svg)
{
    for (auto [from, to] : { std::pair<const char *, const char *> { kDarkInk, kLightInk }, std::pair<const char *, const char *> { kDarkAccent, kLightAccent } })
    {
        for (size_t at = svg.find (from); at != std::string::npos; at = svg.find (from, at))
        {
            svg.replace (at, strlen (from), to);
        }
    }

    return svg;
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
//  Pin to Quick access, or Unpin, for a folder, as Explorer's menus have it.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddPinMenuCommand (std::vector<DxuiPopupMenuItem> & items, const std::wstring & folder)
{
    bool  pinned = m_context.browser.IsPinnedToQuickAccess (folder);



    AddMenuCommand (items, pinned ? L"Unpin from &Quick access" : L"Pin to &Quick access", [this, folder, pinned]()
    {
        HRESULT  hr = m_context.shellVerbs.SetPinnedToQuickAccess (m_context.window.GetHwnd(), folder, !pinned);

        IGNORE_RETURN_VALUE (hr, S_OK);
        m_context.browser.RefreshShellRoots();
        m_context.treePane.RefreshTree();
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
//  Open in new tab and new window, and the pins, for a folder or image.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddOpenRows (std::vector<DxuiPopupMenuItem> & items, const Location & location, bool withOpen)
{
    Win32PackagedCommands::Entry  startPin;



    //  A folder's row opens it here, first on its menu, as in File Explorer.
    if (withOpen && location.kind == Location::Kind::HostFolder)
    {
        AddMenuCommand (items, GetVerbLabel (CassoExplorerActions::Verb::Open), [this, location]() { m_context.addressBar.GoToLocation (location); }, L"Enter");
        m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenFile;
        m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.openfolder");
    }

    AddMenuCommand (items, L"Open in new &tab", [this, location]()
    {
        m_context.browser.OpenInNewTab (location);
        m_context.listPane.FillList();
    });
    m_menuCommands.back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
    m_menuCommands.back()->menuSvg   = GetMenuSvg (L"windows.opennewtab");

    if (location.kind == Location::Kind::HostFolder || location.kind == Location::Kind::DiskImage)
    {
        AddMenuCommand (items, L"Open in new &window", [this, path = location.path]() { m_context.tabManager.OpenInNewWindow (path); });
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
//  New's choices for a location, as File Explorer's New lists them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddNewChoices (std::vector<DxuiPopupMenuItem> & items, const Location & target,
                                   std::shared_ptr<const DxuiCommand> folder, std::shared_ptr<const DxuiCommand> disk)
{
    std::vector<Win32PackagedCommands::NewChoice>  choices = m_packagedCommands.GetNewChoices();
    bool                                           host    = target.kind == Location::Kind::HostFolder;
    bool                                           types   = false;



    //  File Explorer's Folder and Shortcut have the shell's colored icons.
    if (folder != nullptr)
    {
        std::shared_ptr<DxuiCommand>  colored = std::make_shared<DxuiCommand> (*folder);

        colored->menuSvg   = nullptr;
        colored->menuGlyph = nullptr;
        colored->menuImage = GetStockMenuImage (SIID_FOLDER);
        m_menuCommands.push_back (colored);
        items.push_back (DxuiPopupMenuItem::ForCommand (std::move (colored)));
    }

    for (const Win32PackagedCommands::NewChoice & choice : choices)
    {
        if (host && _wcsicmp (choice.verb.c_str(), Win32PackagedCommands::kNewLinkVerb) == 0)
        {
            AddMenuCommand (items, choice.title.c_str(), [this, target, verb = choice.verb]() { RunNewChoice (target, verb); });
            m_menuCommands.back()->menuImage = GetIconFileMenuImage (L"shell32.dll", -16769);
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
        m_menuCommands.back()->menuImage = m_context.shellIcons.GetForExtension (choice.verb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetStockMenuImage
//
//  A shell stock icon at the menu's icon size, as File Explorer's New draws
//  Folder and Shortcut; null when the shell doesn't have one. Cached for the
//  window's life.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> ExplorerMenus::GetStockMenuImage (SHSTOCKICONID id)
{
    int              sizePx = m_context.scaler.ToPx (kMenuIconDip);
    std::wstring     key    = std::format (L"{}:stock:{}", sizePx, (int) id);
    SHSTOCKICONINFO  info   = { sizeof (info) };
    HRESULT          hr     = S_OK;



    if (m_openMenuImages.count (key) != 0)
    {
        return m_openMenuImages[key];
    }

    m_openMenuImages[key] = nullptr;

    hr = SHGetStockIconInfo (id, SHGSI_ICONLOCATION, &info);

    if (SUCCEEDED (hr))
    {
        m_openMenuImages[key] = GetIconFileMenuImage (info.szPath, info.iIcon);
    }

    return m_openMenuImages[key];
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetIconFileMenuImage
//
//  An icon in a file, by its index or, when negative, its resource id, at the
//  menu's icon size, as New's Shortcut is in shell32.dll. Cached for the
//  window's life.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiIconImage> ExplorerMenus::GetIconFileMenuImage (const std::wstring & file, int index)
{
    int           sizePx = m_context.scaler.ToPx (kMenuIconDip);
    std::wstring  key    = std::format (L"{}:{}:{}", sizePx, file, index);
    HICON         icon   = nullptr;
    HRESULT       hr     = S_OK;
    auto          image  = std::make_shared<DxuiIconImage>();



    if (m_openMenuImages.count (key) != 0)
    {
        return m_openMenuImages[key];
    }

    m_openMenuImages[key] = nullptr;

    hr = SHDefExtractIconW (file.c_str(), index, 0, &icon, nullptr, (UINT) sizePx);

    if (SUCCEEDED (hr) && icon != nullptr)
    {
        hr = DxuiIconImage::FromHicon (icon, sizePx, *image);
        DestroyIcon (icon);

        if (SUCCEEDED (hr))
        {
            m_openMenuImages[key] = image;
        }
    }

    return m_openMenuImages[key];
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



    m_context.addressBar.GoToLocation (target);

    hr = m_packagedCommands.CreateNew (m_context.window.GetHwnd(), target.path, verb, created);

    if (FAILED (hr))
    {
        m_context.window.ShowMessage (L"The new item could not be created.", MB_ICONERROR);
        return;
    }

    m_context.folderSync.RefreshAfterHostChange();

    if (!created.empty() && named)
    {
        m_context.undo.Push (ExplorerUndoStep { ExplorerUndoStep::Kind::HostNewItem, Location(), { created } });
        if (m_context.listPane.SelectRowNamed (std::filesystem::path (created).filename().wstring()))
        {
            m_context.rename.BeginRename();
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
//  ExplorerMenus::RunAskCopilot
//
//  Copilot's command, which isn't offered for every type, is found through a
//  type it is offered for when the file's isn't one, then run on the file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerMenus::RunAskCopilot (const std::wstring & path)
{
    static constexpr const wchar_t *  s_kpszTitle = L"Ask Copilot";



    wchar_t  windows[MAX_PATH] = {};
    UINT     length            = GetWindowsDirectoryW (windows, MAX_PATH);



    for (const std::wstring & probe : { path, std::wstring (windows, length) + L"\\win.ini" })
    {
        for (const Win32PackagedCommands::Entry & entry : m_packagedCommands.GetEntries ({ probe }, Win32PackagedCommands::ItemKind::File))
        {
            if (entry.command && entry.title == s_kpszTitle)
            {
                return m_packagedCommands.Invoke (m_context.window.GetHwnd(), entry, { path });
            }
        }
    }

    return HRESULT_FROM_WIN32 (ERROR_NOT_FOUND);
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
//  ExplorerMenus::AddShellMenuCommands
//
//  The commands File Explorer lists on its first level from the items' shell
//  menu rather than from the registry: for drives, BitLocker, Format and,
//  for a removable drive, Eject; for files, Run as administrator and, for a
//  shortcut, Open file location. Each is run by its verb as if picked there.
//  A single program or shortcut also has Pin to Start.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerMenus::AddShellMenuCommands (std::vector<DxuiPopupMenuItem> & items, const std::vector<std::wstring> & paths)
{
    static const std::vector<std::string>   kDriveVerbs = { "encrypt-bde-elev", "encrypt-bde", "format", "eject" };
    static const std::vector<std::string>   kFileVerbs  = { "runas", "opencontaining" };
    std::vector<IShellItemVerbs::ItemVerb>  found;
    Win32PackagedCommands::Entry            startPin;
    bool                                    drives      = false;
    bool                                    files       = false;
    HRESULT                                 hr          = S_OK;



    if (paths.empty())
    {
        return;
    }

    drives = std::all_of (paths.begin(), paths.end(), [] (const std::wstring & path) { return path.size() == 3 && path[1] == L':' && path[2] == L'\\'; });
    //  The shell's menu loads every handler registered for the items, so it
    //  is built only for the types these commands are on: programs, scripts,
    //  installers and shortcuts.
    files  = !drives && std::all_of (paths.begin(), paths.end(), [] (const std::wstring & path)
    {
        std::wstring  extension = std::filesystem::path (path).extension().wstring();

        for (const wchar_t * runnable : { L".exe", L".com", L".bat", L".cmd", L".msi", L".lnk" })
        {
            if (_wcsicmp (extension.c_str(), runnable) == 0)
            {
                return true;
            }
        }

        return false;
    });

    if (!drives && !files)
    {
        return;
    }

    hr = m_context.shellVerbs.ListItemVerbs (paths, drives ? kDriveVerbs : kFileVerbs, found);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const IShellItemVerbs::ItemVerb & each : found)
    {
        std::string  verb = each.verb;

        AddMenuCommand (items, each.label.c_str(), [this, paths, verb]()
        {
            HRESULT  ran = m_context.shellVerbs.RunItemVerb (m_context.window.GetHwnd(), paths, verb.c_str());

            IGNORE_RETURN_VALUE (ran, S_OK);
            m_context.folderSync.RefreshAfterHostChange();
        });

        //  File Explorer's icons for them.
        m_menuCommands.back()->menuSvg = GetMenuSvg (GetShellVerbSvgName (verb));
    }

    //  A program or shortcut pins to Start, as a folder does.
    if (files && paths.size() == 1 && m_packagedCommands.GetStartPinEntry (paths[0], startPin))
    {
        AddPackagedEntry (items, startPin, paths);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerMenus::GetShellVerbSvgName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ExplorerMenus::GetShellVerbSvgName (const std::string & verb)
{
    if (verb.starts_with ("encrypt-bde"))
    {
        return L"encrypt-bde-elev";
    }

    if (verb == "format")
    {
        return L"windows.diskformat";
    }

    if (verb == "eject")
    {
        return L"windows.eject";
    }

    if (verb == "runas")
    {
        return L"RunAsAdmin";
    }

    return (verb == "opencontaining") ? L"windows.shortcut.opencontaining" : nullptr;
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



    for (int row : m_context.browser.GetSelectedRows())
    {
        folders += m_context.browser.GetRows()[(size_t) row].isDirectory ? 1 : 0;
    }

    m_context.browser.GetSelectedHostPaths (outPaths);
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



    //  On the background of a folder not on the disk, File Explorer doesn't list
    //  a command that takes such a folder as its item, as Beyond Compare does.
    //  PowerRename hides itself for one, and is listed. Network is the test.
    if (paths.empty())
    {
        for (const Win32PackagedCommands::Entry & folderEntry : m_packagedCommands.GetEntries ({ L"::{F02C1A0D-BE21-4350-88B0-7367FC96EF3C}" }, kind))
        {
            std::erase_if (entries, [&folderEntry] (const Win32PackagedCommands::Entry & entry) { return entry.title == folderEntry.title; });
        }
    }

    if (entries.empty())
    {
        return;
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    //  A sync client's commands, as OneDrive's, are among the rest in the order
    //  Windows lists them, as in File Explorer.
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
    command->menuImage = (command->menuSvg == nullptr) ? m_context.shellIcons.GetForIconResource (entry.icon) : nullptr;
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
        HRESULT  hr = entry.command ? m_packagedCommands.Invoke (m_context.window.GetHwnd(), entry, paths)
                                    : m_context.shellVerbs.RunItemVerb (m_context.window.GetHwnd(), paths, entry.verb.c_str());

        //  Another program's command faulted, and was stopped.
        if (hr == E_UNEXPECTED)
        {
            m_context.window.ShowMessage (L"\"" + entry.title + L"\" stopped working.", MB_ICONWARNING);
        }

        IGNORE_RETURN_VALUE (hr, S_OK);
    };

    items.push_back (DxuiPopupMenuItem::ForCommand (command));
    m_menuCommands.push_back (std::move (command));
}

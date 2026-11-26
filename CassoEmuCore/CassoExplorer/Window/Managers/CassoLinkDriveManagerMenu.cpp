#include "Pch.h"

#include "CassoExplorer/Window/Managers/CassoLinkDriveManager.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/CassoLinkConnections.h"
#include "CassoExplorer/Model/CassoLinkConnectionText.h"
#include "CassoExplorer/Model/TreeModel.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::IsFloppyImage
//
//  By extension, and for a .2mg by its size, so a menu opens without reading
//  the file.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoLinkDriveManager::IsFloppyImage (const std::wstring & path)
{
    static constexpr const wchar_t *  s_kExtensions[] = { L".woz", L".nib", L".nb2", L".dsk", L".do", L".po", L".d13" };
    static constexpr uintmax_t        s_k2mgMaxBytes  = 143360 + 1024;



    std::wstring     extension = std::filesystem::path (path).extension().wstring();
    std::error_code  error;
    uintmax_t        size      = 0;



    std::transform (extension.begin(), extension.end(), extension.begin(), ::towlower);

    if (extension == L".2mg" || extension == L".2img")
    {
        size = std::filesystem::file_size (path, error);
        return !error && size <= s_k2mgMaxBytes;
    }

    return std::any_of (std::begin (s_kExtensions), std::end (s_kExtensions), [&extension] (const wchar_t * each) { return extension == each; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::AddWriteToFloppyMenu
//
//  A submenu of each connected Apple's floppy drives, or the drives alone
//  when one Apple is connected. A drive that is busy is grayed, and with no
//  Apple connected the row is grayed.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::AddWriteToFloppyMenu (std::vector<DxuiPopupMenuItem> & items, size_t at, const std::wstring & image)
{
    CassoLinkConnections            * links   = m_context.browser.GetLinks();
    std::vector<LinkTreeEntry>        entries;
    std::vector<DxuiPopupMenuItem>    rows;
    std::shared_ptr<DxuiCommand>      parent  = std::make_shared<DxuiCommand>();
    size_t                            apples  = 0;



    if (links == nullptr || !IsFloppyImage (image))
    {
        return;
    }

    entries = links->BuildTreeEntries();

    std::erase_if (entries, [] (const LinkTreeEntry & entry)
    {
        return !entry.replying || std::none_of (entry.drives.begin(), entry.drives.end(), [] (const LinkTreeEntry::Drive & drive) { return !drive.isBlockDevice; });
    });

    apples = entries.size();

    for (const LinkTreeEntry & entry : entries)
    {
        std::vector<DxuiPopupMenuItem>  drives;
        std::shared_ptr<DxuiCommand>    apple = std::make_shared<DxuiCommand>();

        for (const LinkTreeEntry::Drive & drive : entry.drives)
        {
            if (!drive.isBlockDevice)
            {
                AddWriteToFloppyRow (drives, entry.name, drive, image);
            }
        }

        if (apples == 1)
        {
            rows = std::move (drives);
            break;
        }

        apple->label     = entry.name;
        apple->menuImage = m_context.shellIcons.GetForKind (IShellIcons::Kind::AppleMachine);

        rows.push_back (DxuiPopupMenuItem::ForSubmenu (apple, std::move (drives)));
        m_context.menus.GetMenuCommands().push_back (std::move (apple));
    }

    parent->label     = L"Write to CassoLink floppy";
    parent->menuImage = m_context.shellIcons.GetForKind (IShellIcons::Kind::AppleMachine);

    if (rows.empty())
    {
        parent->isEnabled = [] () { return false; };
        items.insert (items.begin() + (ptrdiff_t) std::min (at, items.size()), DxuiPopupMenuItem::ForCommand (parent));
    }
    else
    {
        items.insert (items.begin() + (ptrdiff_t) std::min (at, items.size()), DxuiPopupMenuItem::ForSubmenu (parent, std::move (rows)));
    }

    m_context.menus.GetMenuCommands().push_back (std::move (parent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::AddWriteToFloppyRow
//
//  The drive's label, with its disk's volume once its catalog is read.
//
////////////////////////////////////////////////////////////////////////////////

void CassoLinkDriveManager::AddWriteToFloppyRow (std::vector<DxuiPopupMenuItem> & rows, const std::wstring & link, const LinkTreeEntry::Drive & drive, const std::wstring & image)
{
    std::shared_ptr<DxuiCommand>  row    = std::make_shared<DxuiCommand>();
    std::string                   name   = TextEncoding::WideToNarrow (link);
    int                           number = drive.drive;
    Location                      target = Location::MakeAppleDrive (TreeModel::MakeLinkDriveId (link, drive.slot, drive.drive), std::string());



    row->label     = TreeModel::GetLinkDriveLabel (drive) + (drive.volume.empty() ? std::wstring() : std::format (L" {} {}", s_kpszMiddleDot, drive.volume));
    row->dispatch  = [this, target, image] () { StartImageWrite (target, image); };
    row->isEnabled = [this, name, number] () { CassoLinkConnections  * links = m_context.browser.GetLinks(); return links != nullptr && !links->IsDriveBusy (name, number); };

    rows.push_back (DxuiPopupMenuItem::ForCommand (row));
    m_context.menus.GetMenuCommands().push_back (std::move (row));
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkDriveManager::DescribeConnection
//
//  The connection's name, with its port or address after it in parentheses
//  when the name doesn't already include it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring CassoLinkDriveManager::DescribeConnection (const std::wstring & link) const
{
    CassoLinkConnections  * links  = m_context.browser.GetLinks();
    std::wstring            detail;



    if (links == nullptr)
    {
        return link;
    }

    for (const CassoLinkSettings & settings : links->GetLinks())
    {
        if (TextEncoding::NarrowToWide (settings.name) == link)
        {
            detail = TextEncoding::NarrowToWide (settings.transport == LinkTransport::Serial ? settings.port : settings.address);
        }
    }

    if (detail.empty() || CassoLinkConnectionText::NamesItsPort (link, detail))
    {
        return link;
    }

    return std::format (L"{} ({})", link, detail);
}

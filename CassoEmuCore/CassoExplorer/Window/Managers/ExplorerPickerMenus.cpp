#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerPickerMenus.h"
#include "CassoExplorer/Window/ExplorerContext.h"
#include "CassoExplorer/Window/IExplorerWindow.h"
#include "CassoExplorer/Window/Managers/ExplorerListMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerPickerMode.h"
#include "CassoExplorer/Window/Managers/ExplorerVerbs.h"
#include "CassoExplorer/Window/Managers/ExplorerClipboard.h"
#include "CassoExplorer/Window/Managers/ExplorerProperties.h"
#include "CassoExplorer/Window/Managers/ExplorerUndo.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Browser/BrowserRowPaths.h"
#include "CassoExplorer/Model/BrowserModel.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "Seams/IShellItemVerbs.h"
#include "Core/UnicodeSymbols.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus::ExplorerPickerMenus
//
////////////////////////////////////////////////////////////////////////////////

ExplorerPickerMenus::ExplorerPickerMenus (ExplorerListMenus & listMenus, ExplorerContext & context)
    : m_listMenus (listMenus),
      m_context   (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus::GetPickerVerbs
//
//  The clipboard, rename and delete verbs of a selection, and New folder and
//  Refresh of the background, as the standard Open and Save As dialogs offer
//  them. Open is the picker's confirm, which the menu adds itself.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<CassoExplorerActions::Verb> ExplorerPickerMenus::GetPickerVerbs (const std::vector<CassoExplorerActions::Verb> & verbs)
{
    using Verb = CassoExplorerActions::Verb;

    static constexpr Verb  kDialogVerbs[] = { Verb::Cut, Verb::Copy, Verb::Paste, Verb::Rename, Verb::Delete, Verb::NewFolder, Verb::Refresh };

    std::vector<Verb>  pickerVerbs;



    std::copy_if (verbs.begin(), verbs.end(), std::back_inserter (pickerVerbs), [] (Verb verb)
    {
        return std::find (std::begin (kDialogVerbs), std::end (kDialogVerbs), verb) != std::end (kDialogVerbs);
    });

    return pickerVerbs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus::GetListItems
//
//  A group's header has no menu in a picker.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerPickerMenus::GetListItems (int group)
{
    std::vector<DxuiPopupMenuItem>  items;



    m_context.menus.GetMenuCommands().clear();

    if (group >= 0)
    {
        return items;
    }

    if (m_context.browser.GetSelectedRows().empty())
    {
        AddBackgroundRows (items);
    }
    else
    {
        AddSelectionRows (items);
    }

    ExplorerListMenus::RemoveExtraSeparators (items);
    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus::AddSelectionRows
//
//  A file's first row is the picker's confirm and a folder's is Open, each
//  as Enter does. Explorer's icon row of Cut, Copy, Paste, Rename and Delete
//  goes above them, then Copy as path and Properties.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMenus::AddSelectionRows (std::vector<DxuiPopupMenuItem> & items)
{
    std::vector<std::shared_ptr<const DxuiCommand>>  iconCommands;
    std::vector<DxuiPopupMenuItem>                   primary;
    std::vector<int>                                 selected = m_context.browser.GetSelectedRows();
    std::wstring                                     file;
    Location                                         location;
    bool                                             isFile   = selected.size() == 1 && m_context.pickerMode.TryGetSelectedFile (file);
    bool                                             isFolder = !isFile && selected.size() == 1 && BrowserRowPaths::TryGetRowLocation (m_context.browser.GetRowListing(), selected[0], location);



    if (isFile)
    {
        m_context.menus.AddMenuCommand (items, m_context.pickerMode.GetConfirmLabel().c_str(), [this, row = selected[0]]()
        {
            bool  confirmed = m_context.pickerMode.HandleActivateRow (row);

            IGNORE_RETURN_VALUE (confirmed, false);
        }, L"Enter");
    }
    else if (isFolder)
    {
        m_context.menus.AddMenuCommand (items, L"&Open", [this, location]() { m_context.addressBar.GoToLocation (location); }, L"Enter");
    }

    for (CassoExplorerActions::Verb verb : GetPickerVerbs (m_context.actions.GetListVerbs()))
    {
        std::shared_ptr<DxuiCommand>  command;

        if (ExplorerMenus::GetVerbGlyph (verb) == nullptr || (verb == CassoExplorerActions::Verb::Paste && !m_context.shellVerbs.ClipboardHasFiles()))
        {
            continue;
        }

        command           = std::make_shared<DxuiCommand>();
        command->id       = (int) verb;
        command->label    = ExplorerMenus::GetVerbLabel (verb);
        command->glyph    = ExplorerMenus::GetVerbGlyph (verb);
        command->menuSvg  = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (verb));
        command->dispatch = [this, verb]() { m_context.verbs.RunVerb (verb); };

        iconCommands.push_back (command);
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    std::stable_sort (iconCommands.begin(), iconCommands.end(), [] (const auto & a, const auto & b)
    {
        return ExplorerMenus::GetIconOrder ((CassoExplorerActions::Verb) a->id) < ExplorerMenus::GetIconOrder ((CassoExplorerActions::Verb) b->id);
    });

    if (!iconCommands.empty())
    {
        primary = m_context.menus.GetPrimaryItems (std::move (iconCommands));

        items.insert (items.begin(), DxuiPopupMenuItem::ForSeparator());
        items.insert (items.begin(), std::make_move_iterator (primary.begin()), std::make_move_iterator (primary.end()));
    }

    ExplorerListMenus::AddSeparator (items);

    //  A stock disk has no file at its path until it is downloaded.
    if (!m_context.listPane.IsStockDiskToFetchSelected())
    {
        m_context.menus.AddMenuCommand (items, L"Copy as &path", [this]() { m_context.clipboard.CopySelectedPaths(); }, L"Ctrl+Shift+C", s_kpszMdl2CopyPath);
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"windows.copyaspath");
    }

    if (selected.size() == 1)
    {
        AddPropertiesRow (items, [this, row = selected[0]]() { m_context.properties.ShowRowProperties (row); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus::AddBackgroundRows
//
//  The view's choices, Undo, then Paste, New folder, Refresh and the
//  folder's Properties, as the standard dialogs offer them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMenus::AddBackgroundRows (std::vector<DxuiPopupMenuItem> & items)
{
    Location  current = m_context.browser.GetLocation();
    bool      host    = current.kind == Location::Kind::HostFolder && !SearchQuery::IsId (current.path);



    m_listMenus.AddViewMenus (items);
    ExplorerListMenus::AddSeparator (items);

    if (m_context.undo.CanUndo())
    {
        m_context.menus.AddMenuCommand (items, ExplorerUndo::GetLabel (m_context.undo.GetLast().kind), [this]() { m_context.undo.UndoLast(); }, L"Ctrl+Z");
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"windows.undo");
        ExplorerListMenus::AddSeparator (items);
    }

    if (host)
    {
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::Paste), [this]() { m_context.verbs.RunVerb (CassoExplorerActions::Verb::Paste); }, L"Ctrl+V");
        m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb::Paste));
        m_context.menus.GetMenuCommands().back()->isEnabled = [this]() { return m_context.shellVerbs.ClipboardHasFiles(); };

        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::NewFolder), [this]() { m_context.verbs.RunVerb (CassoExplorerActions::Verb::NewFolder); }, L"Ctrl+Shift+N");
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"newfolder");
    }

    m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::Refresh), [this]() { m_context.verbs.RunVerb (CassoExplorerActions::Verb::Refresh); }, L"F5");

    if (host)
    {
        ExplorerListMenus::AddSeparator (items);
        AddPropertiesRow (items, [this, current]() { m_context.properties.ShowLocationProperties (current); });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus::GetTreeItems
//
//  A folder on the disk: the icon row, Open, Copy as path and Properties. A
//  node that is not a folder on the disk has no menu in a picker.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerPickerMenus::GetTreeItems (const std::wstring & id)
{
    std::vector<DxuiPopupMenuItem>  items;
    Location                        location;
    bool                            isFolder = m_context.browser.TryGetNodeLocation (id, location) && location.kind == Location::Kind::HostFolder;
    bool                            isDrive  = isFolder && location.path.size() <= 3;



    m_context.menus.GetMenuCommands().clear();

    if (!isFolder)
    {
        return items;
    }

    m_context.treePane.AddTreeIconRow (items, location.path, id, isDrive);

    m_context.menus.AddMenuCommand (items, L"&Open", [this, location]() { m_context.addressBar.GoToLocation (location); });
    ExplorerListMenus::AddSeparator (items);

    //  File Explorer's drive has no Copy as path.
    if (!isDrive)
    {
        m_context.menus.AddMenuCommand (items, L"Copy as &path", [this, location]()
        {
            DxuiClipboard::SetText (m_context.window.GetHwnd(), L"\"" + BrowserModel::FormatAddress (location) + L"\"");
        }, L"Ctrl+Shift+C", s_kpszMdl2CopyPath);
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"windows.copyaspath");
    }

    AddPropertiesRow (items, [this, location]() { m_context.properties.ShowLocationProperties (location); });

    ExplorerListMenus::RemoveExtraSeparators (items);
    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus::AddPropertiesRow
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMenus::AddPropertiesRow (std::vector<DxuiPopupMenuItem> & items, std::function<void()> dispatch)
{
    m_context.menus.AddMenuCommand (items, L"P&roperties", std::move (dispatch), L"Alt+Enter");
    m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
    m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus::ShowListMenu
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMenus::ShowListMenu (int x, int y, int group)
{
    std::vector<DxuiPopupMenuItem>  items = GetListItems (group);



    if (items.empty())
    {
        return;
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerPickerMenus::ShowTreeMenu
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerPickerMenus::ShowTreeMenu (int x, int y, const std::wstring & id)
{
    std::vector<DxuiPopupMenuItem>  items = GetTreeItems (id);



    if (items.empty())
    {
        return;
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}






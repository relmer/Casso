#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerListMenus.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Model/SearchQuery.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "CassoExplorer/Window/Managers/ExplorerMenus.h"
#include "CassoExplorer/Window/Managers/ExplorerFileOps.h"
#include "CassoExplorer/Window/Panes/ExplorerListPane.h"
#include "CassoExplorer/Window/Managers/CassoLinkManager.h"
#include "Seams/Win32ShellItemVerbs.h"
#include "CassoExplorer/Window/Managers/ExplorerCommandManager.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/Window/Managers/ExplorerFolderSync.h"
#include "CassoExplorer/Window/Managers/ExplorerInput.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/CassoExplorerCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::ExplorerListMenus
//
////////////////////////////////////////////////////////////////////////////////

ExplorerListMenus::ExplorerListMenus (ExplorerListPane & pane, ExplorerContext & context)
    : m_pane   (pane),
      m_context (context)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::ShowListHeaderMenu
//
//  Explorer's header menu: fit the column under the pointer, fit them all,
//  then the columns that apply to the folder on display, with a check by the
//  visible ones. Name is always visible.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::ShowListHeaderMenu (int x, int y, int column)
{
    std::vector<DxuiPopupMenuItem>     items;
    std::vector<DxuiListView::Column>  columns   = BrowserCells::GetColumns();
    Location                           location  = m_context.browser.GetLocation();
    bool                               searching = SearchQuery::IsId (location.path);



    m_context.menus.GetMenuCommands().clear();

    if (column >= 0)
    {
        m_context.menus.AddMenuCommand (items, L"Size column to &fit", [this, column]()
        {
            m_pane.GetListView()->FitColumnToContent ((size_t) column);
            m_context.window.Invalidate();
        });
    }

    m_context.menus.AddMenuCommand (items, L"Size &all columns to fit", [this]()
    {
        m_pane.GetListView()->FitAllColumnsToContent();
        m_context.window.Invalidate();
    });

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (size_t c = 0; c < columns.size() && c < m_pane.GetColumns().GetChosen().size(); c++)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        if (!ExplorerListColumns::IsListColumnOffered (c, location.kind, searching))
        {
            continue;
        }

        command->label     = columns[c].title;
        command->isChecked = [this, c, location, searching]() { return ExplorerListColumns::IsListColumnShown (c, m_pane.GetColumns().GetChosen()[c], location.kind, searching); };
        command->isEnabled = [c]() { return c != (size_t) CatalogModel::Column::Name; };
        command->dispatch  = [this, c]()
        {
            m_pane.GetColumns().GetChosen()[c] = !m_pane.GetColumns().GetChosen()[c];

            //  This folder's choice, and the one others without their own follow.
            m_context.prefs.hiddenColumns.clear();

            for (size_t each = 0; each < m_pane.GetColumns().GetChosen().size(); each++)
            {
                if (!m_pane.GetColumns().GetChosen()[each])
                {
                    m_context.prefs.hiddenColumns.push_back ((int) each);
                }
            }

            m_pane.GetColumns().RememberFolderColumns();
            m_pane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::ShowListContextMenu
//
//  The rows come from the actions' verb list; each command runs its verb.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::ShowListContextMenu (int x, int y, int group)
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
    bool                                             background     = m_context.browser.GetSelectedRows().empty() && group < 0;
    Location                                         shown          = m_context.browser.GetLocation();
    size_t                                           undoRow        = 0;



    m_context.menus.GetMenuCommands().clear();
    m_context.fileOps.AskCassoToDescribe();

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
    if (background && m_context.fileOps.CanUndo())
    {
        m_context.menus.AddMenuCommand (items, ExplorerFileOps::GetUndoLabel (m_context.fileOps.GetLastUndo().kind), [this]() { m_context.fileOps.UndoLast(); }, L"Ctrl+Z");
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"windows.undo");
        undoRow                                 = items.size();
    }

    //  From a group's header: open or close it, and all of them, with the
    //  keys that do the same on a focused header.
    if (group >= 0 && m_pane.GetColumns().GetView() != DxuiListView::View::List)
    {
        bool  collapsed = m_pane.GetListView()->IsGroupCollapsed (group);

        m_context.menus.AddMenuCommand (items, collapsed ? L"E&xpand group" : L"C&ollapse group", [this, group, collapsed]()
        {
            m_pane.GetListView()->SetGroupCollapsed (group, !collapsed);
            m_context.window.Invalidate();
        }, collapsed ? L"Right" : L"Left");
        m_context.menus.AddMenuCommand (items, L"&Expand all groups",   [this]() { m_pane.GetListView()->SetAllGroupsCollapsed (false); m_context.window.Invalidate(); });
        m_context.menus.AddMenuCommand (items, L"Co&llapse all groups", [this]() { m_pane.GetListView()->SetAllGroupsCollapsed (true);  m_context.window.Invalidate(); });
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    //  A folder or a disk image opens in a new tab, as Explorer's items do.
    if (m_context.browser.GetSelectedRows().size() == 1 && m_context.browser.TryGetRowLocation (m_context.browser.GetSelectedRows()[0], location))
    {
        browsable = true;

        m_context.menus.AddOpenRows (items, location, true);

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

            m_context.cassoLinkManager.AddConnectionRows (items, true, location);

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
        m_context.cassoLinkManager.AddConnectionRows (items, true, shown);
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    for (CassoExplorerActions::Verb verb : m_context.actions.GetListVerbs())
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
        if (verb == CassoExplorerActions::Verb::Paste && !m_context.shellVerbs.ClipboardHasFiles())
        {
            continue;
        }

        if (ExplorerMenus::GetVerbGlyph (verb) != nullptr)
        {
            command           = std::make_shared<DxuiCommand>();
            command->id       = (int) verb;
            command->label    = ExplorerMenus::GetVerbLabel (verb);
            command->glyph    = ExplorerMenus::GetVerbGlyph (verb);
            command->menuSvg  = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (verb));
            command->dispatch = [this, verb]() { m_context.commandManager.RunVerb (verb); };

            iconCommands.push_back (command);
            m_context.menus.GetMenuCommands().push_back (std::move (command));
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
            command->label    = ExplorerMenus::GetVerbLabel (verb);
            command->menuSvg  = m_context.menus.GetMenuSvg ((verb == CassoExplorerActions::Verb::NewFolder) ? L"newfolder" : L"casso.floppy525");
            command->dispatch = [this, verb]() { m_context.commandManager.RunVerb (verb); };

            if (verb == CassoExplorerActions::Verb::NewFolder)
            {
                command->isEnabled = [this]() { return !m_context.browser.IsVolumeLocation() || m_context.browser.GetVolumeKind() == VolumeKind::ProDos; };
                newFolder          = command;
            }
            else
            {
                newDisk = command;
            }

            m_context.menus.GetMenuCommands().push_back (std::move (command));
            continue;
        }

        if (verb == CassoExplorerActions::Verb::Refresh && (newFolder != nullptr || newDisk != nullptr))
        {
            std::shared_ptr<DxuiCommand>  parent = std::make_shared<DxuiCommand>();
            bool                          dos33  = m_context.browser.IsVolumeLocation() && m_context.browser.GetVolumeKind() != VolumeKind::ProDos;

            m_context.menus.AddNewChoices (newChoices, m_context.browser.GetLocation(), newFolder, newDisk);

            parent->label     = L"Ne&w";
            parent->menuGlyph = s_kpszMdl2Add;
            parent->menuSvg   = m_context.menus.GetMenuSvg (L"windows.newitem");
            parent->isEnabled = [dos33]() { return !dos33; };

            if (!items.empty() && items.back().kind != DxuiPopupMenuItem::Kind::Separator && items.size() != undoRow)
            {
                items.push_back (DxuiPopupMenuItem::ForSeparator());
            }

            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (newChoices)));
            m_context.menus.GetMenuCommands().push_back (std::move (parent));
            newChoices.clear();
        }

        //  Refresh only places New; Explorer's menus leave it to F5.
        if (verb == CassoExplorerActions::Verb::Refresh)
        {
            continue;
        }

        command            = std::make_shared<DxuiCommand>();
        command->id        = (int) verb;
        command->label     = ExplorerMenus::GetVerbLabel (verb);
        command->menuGlyph = ExplorerMenus::GetVerbMenuGlyph (verb);
        command->menuSvg   = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (verb));
        command->dispatch  = [this, verb]() { m_context.commandManager.RunVerb (verb); };

        if (verb == CassoExplorerActions::Verb::Open)
        {
            command->menuImage   = m_context.menus.GetOpenMenuImage (browsable, m_context.fileOps.GetSelectedImagePath());
            command->accelerator = L"Enter";
        }

        //  A machine known to have one drive cannot take drive 2; one not
        //  described yet is given the benefit of the doubt.
        if (verb == CassoExplorerActions::Verb::InsertDrive2)
        {
            command->isEnabled = [this]() { return m_context.fileOps.GetCassoDriveCount() != 1; };
        }

        if (verb == CassoExplorerActions::Verb::Paste)
        {
            command->isEnabled = [this]() { return m_context.shellVerbs.ClipboardHasFiles(); };
        }

        //  Convert to's one choice for the file, labeled with what it makes.
        if (verb == CassoExplorerActions::Verb::ConvertTo)
        {
            AddConvertMenu (items);
            continue;
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
        m_context.menus.GetMenuCommands().push_back (std::move (command));

        if (verb == CassoExplorerActions::Verb::Format)
        {
            std::vector<DxuiPopupMenuItem>  advanced;
            std::shared_ptr<DxuiCommand>    parent = std::make_shared<DxuiCommand>();

            for (CassoExplorerActions::Verb raw : { CassoExplorerActions::Verb::ReadSectors, CassoExplorerActions::Verb::WriteSectors,
                                              CassoExplorerActions::Verb::ReadBlocks,  CassoExplorerActions::Verb::WriteBlocks })
            {
                std::shared_ptr<DxuiCommand>  child = std::make_shared<DxuiCommand>();

                child->id       = (int) raw;
                child->label    = ExplorerMenus::GetVerbLabel (raw);
                child->dispatch = [this, raw]() { m_context.commandManager.RunRawVerb (raw); };

                advanced.push_back (DxuiPopupMenuItem::ForCommand (child));
                m_context.menus.GetMenuCommands().push_back (std::move (child));
            }

            parent->label = L"&Advanced";
            parent->menuGlyph = s_kpszMdl2Setting;
            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (advanced)));
            m_context.menus.GetMenuCommands().push_back (std::move (parent));
        }
    }

    //  Casso's known folders, from a folder row or from the background of the
    //  folder being shown, as the tree offers them from a node.
    if (m_context.browser.GetSelectedRows().size() == 1)
    {
        folderForCasso = (m_context.browser.TryGetRowLocation (m_context.browser.GetSelectedRows()[0], location) &&
                          location.kind == Location::Kind::HostFolder) ? location.path : std::wstring();
    }
    else if (m_context.browser.GetSelectedRows().empty() && m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        folderForCasso = m_context.browser.GetLocation().path;
    }

    if (!folderForCasso.empty())
    {
        known = m_context.browser.IsKnownFolder (folderForCasso);

        m_context.menus.AddMenuCommand (items, known ? L"&Remove from Casso" : L"&Add to Casso", [this, folderForCasso, known]()
        {
            m_context.folderSync.ChangeKnownFolder (folderForCasso, !known);
        });
    }

    //  From the current folder's background: Properties, then the commands
    //  installed apps add there, such as Open in Terminal, then the shell's
    //  own background menu.
    if (background && (shown.kind == Location::Kind::HostFolder || m_context.browser.IsImageLocation()) && !SearchQuery::IsId (shown.path))
    {
        m_context.menus.AddMenuCommand (items, L"P&roperties", [this, shown]() { m_context.fileOps.ShowLocationProperties (shown); }, L"Alt+Enter");
        m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
        m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
    }

    if (background && shown.kind == Location::Kind::HostFolder && !SearchQuery::IsId (shown.path))
    {
        POINT  screen = { x, y };

        ClientToScreen (m_context.window.GetHwnd(), &screen);

        m_context.menus.AddPackagedCommandsFor (items, { shown.path }, Win32PackagedCommands::ItemKind::Background);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, shown, screen]()
        {
            HRESULT  hr = m_context.shellVerbs.ShowBackgroundMenu (m_context.window.GetHwnd(), shown.path, screen);

            IGNORE_RETURN_VALUE (hr, S_OK);
            m_context.folderSync.RefreshAfterHostChange();
        });
        m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
        m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));
    }

    //  A deleted item has no path to copy, and the bin itself describes it.
    if (!m_context.browser.GetSelectedRows().empty() && m_context.browser.GetLocation().kind == Location::Kind::RecycleBin)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_context.menus.AddMenuCommand (items, L"P&roperties", [this]() { m_context.commandManager.RunRecycleBinVerb (CassoExplorerActions::Verb::Open); });
        m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
        m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
    }
    else if (!m_context.browser.GetSelectedRows().empty())
    {
        std::vector<std::wstring>        selectedPaths;
        Win32PackagedCommands::ItemKind  kind    = Win32PackagedCommands::ItemKind::File;
        bool                             windows = false;

        //  Windows' own commands lead into Copy as path in one group, as in
        //  File Explorer; without them the group starts here.
        if (m_context.browser.GetLocation().kind == Location::Kind::HostFolder && m_context.menus.GetSelectionKind (selectedPaths, kind))
        {
            windows = m_context.menus.AddWindowsCommands (items, selectedPaths, kind);
        }

        if (!windows)
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        m_context.menus.AddMenuCommand (items, L"Copy as &path", [this]() { m_context.fileOps.CopySelectedPaths(); }, L"Ctrl+Shift+C", s_kpszMdl2CopyPath);
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"windows.copyaspath");

        if (m_context.browser.GetSelectedRows().size() == 1)
        {
            m_context.menus.AddMenuCommand (items, L"P&roperties", [this]()
            {
                if (!m_context.browser.GetSelectedRows().empty())
                {
                    m_context.fileOps.ShowRowProperties (m_context.browser.GetSelectedRows()[0]);
                }
            }, L"Alt+Enter");
            m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
            m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
        }
        else if (m_context.browser.IsVolumeLocation())
        {
            m_context.menus.AddMenuCommand (items, L"P&roperties", [this]() { m_context.fileOps.ShowAppleProperties(); }, L"Alt+Enter");
            m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
            m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
        }
        else if (!selectedPaths.empty())
        {
            m_context.menus.AddMenuCommand (items, L"P&roperties", [this, selectedPaths]() { m_context.fileOps.ShowHostProperties (selectedPaths); }, L"Alt+Enter");
            m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
            m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
        }
    }

    //  In Explorer's order, whatever order the verbs came in.
    std::stable_sort (iconCommands.begin(), iconCommands.end(), [] (const auto & a, const auto & b)
    {
        return ExplorerMenus::GetIconOrder ((CassoExplorerActions::Verb) a->id) < ExplorerMenus::GetIconOrder ((CassoExplorerActions::Verb) b->id);
    });

    if (!iconCommands.empty())
    {
        items.insert (items.begin(), DxuiPopupMenuItem::ForSeparator());
        items.insert (items.begin(), DxuiPopupMenuItem::ForIconRow (std::move (iconCommands)));
    }

    //  The commands installed apps add for these items, as Explorer lists
    //  them, above the shell's own menu.
    if (moreOptions && m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        m_context.menus.AddPackagedCommands (items);
    }

    //  Last, as Explorer has it: everything else Windows and other programs
    //  offer for these items, in the shell's own menu.
    if (moreOptions)
    {
        POINT  screen = { x, y };

        ClientToScreen (m_context.window.GetHwnd(), &screen);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, screen]()
        {
            std::vector<std::wstring>  paths;
            HRESULT                    hr = S_OK;

            if (m_context.browser.GetLocation().kind == Location::Kind::RecycleBin)
            {
                m_context.browser.GetSelectedRecycledIds (paths);
                hr = m_context.shellVerbs.ShowRecycledMenu (m_context.window.GetHwnd(), paths, screen);
                m_context.folderSync.RefreshAfterHostChange();
            }
            else if (m_context.fileOps.TryGetSelectedShellIds (paths))
            {
                hr = m_context.shellVerbs.ShowShellMenu (m_context.window.GetHwnd(), paths, screen, [this]() { m_context.commandManager.RunVerb (CassoExplorerActions::Verb::Rename); });
            }

            IGNORE_RETURN_VALUE (hr, S_OK);
        });
        m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
        m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));
    }

    //  The menu takes the focus look from the list while it is open, as
    //  Explorer's does: the item keeps its selection, drawn unfocused, and
    //  loses its focus border until the menu goes.
    listFocused = m_pane.GetListView()->IsListFocused();

    if (listFocused)
    {
        m_pane.GetListView()->SetListFocused (false);
        m_context.window.Invalidate();
    }

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items), [this, listFocused] (bool)
    {
        if (listFocused && m_context.input.GetFocusPane() == ExplorerPane::List)
        {
            m_pane.GetListView()->SetListFocused (true);
            m_context.window.Invalidate();
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::AddOpenWithMenu
//
//  The programs Windows recommends for the selected file, then its own
//  dialog for any other.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::AddOpenWithMenu (std::vector<DxuiPopupMenuItem> & items)
{
    std::wstring                          path   = m_context.fileOps.GetSelectedImagePath();
    std::vector<IShellItemVerbs::Handler> handlers;
    std::vector<DxuiPopupMenuItem>        children;
    std::shared_ptr<DxuiCommand>          parent = std::make_shared<DxuiCommand>();
    HRESULT                               hr     = S_OK;
    size_t                                i      = 0;



    if (path.empty())
    {
        return;
    }

    hr = m_context.shellVerbs.GetOpenWithHandlers (path, handlers);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (i = 0; i < handlers.size(); i++)
    {
        m_context.menus.AddMenuCommand (children, ExplorerAddressBar::EscapeMnemonics (handlers[i].name).c_str(), [this, path, i]()
        {
            HRESULT  opened = m_context.shellVerbs.OpenWith (m_context.window.GetHwnd(), path, i);

            IGNORE_RETURN_VALUE (opened, S_OK);
        });
    }

    if (!children.empty())
    {
        children.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    m_context.menus.AddMenuCommand (children, L"&Choose another app", [this, path]()
    {
        HRESULT  chosen = m_context.shellVerbs.ChooseOtherApp (m_context.window.GetHwnd(), path);

        IGNORE_RETURN_VALUE (chosen, S_OK);
    });

    parent->label = ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::OpenWith);
    parent->menuGlyph = s_kpszMdl2OpenWith;
    parent->menuSvg   = m_context.menus.GetMenuSvg (L"windows.openwith");
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (children)));
    m_context.menus.GetMenuCommands().push_back (std::move (parent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::IsListVerbOffered
//
//  Whether the selection's menu has the verb, which the command bar's
//  buttons and the list's keys follow.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListMenus::IsListVerbOffered (CassoExplorerActions::Verb verb) const
{
    std::vector<CassoExplorerActions::Verb>  verbs = m_context.actions.GetListVerbs();



    return std::find (verbs.begin(), verbs.end(), verb) != verbs.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetSortChoices
//
//  One row for each column in the list here, in Explorer's order, then the
//  sort direction.
//
//  The rows of the Sort, Group by and View menus, shared by the command
//  bar and the list's background menu.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetSortChoices()
{
    std::vector<DxuiPopupMenuItem>  items;
    size_t                          column = 0;



    for (column = 0; column < BrowserCells::GetColumns().size(); column++)
    {
        bool  chosen = column >= m_pane.GetColumns().GetChosen().size() || m_pane.GetColumns().GetChosen()[column];

        if (!ExplorerListColumns::IsListColumnShown (column, chosen, m_context.browser.GetLocation().kind, SearchQuery::IsId (m_context.browser.GetLocation().path)))
        {
            continue;
        }

        items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kSortByColumn + (int) column)));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kSortAscending)));
    items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kSortDescending)));

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetGroupChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetGroupChoices()
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int field : { 1, 2, 3, 4, 0 })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kGroupByField + field)));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kGroupAscending)));
    items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kGroupDescending)));

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetViewChoices
//
//  In Explorer's order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetViewChoices()
{
    std::vector<DxuiPopupMenuItem>  items;



    for (int view : { 1, 2, 3, 4, 5, 0, 6, 7 })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kViewFirst + view)));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::AddViewMenus
//
//  View, Sort by and Group by, as the first rows of Explorer's background
//  menu, with the command bar's rows.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::AddViewMenus (std::vector<DxuiPopupMenuItem> & items)
{
    std::shared_ptr<DxuiCommand>  view  = std::make_shared<DxuiCommand>();
    std::shared_ptr<DxuiCommand>  sort  = std::make_shared<DxuiCommand>();
    std::shared_ptr<DxuiCommand>  group = std::make_shared<DxuiCommand>();



    view->label    = L"&View";
    view->menuSvg  = m_context.menus.GetMenuSvg (L"View");
    sort->label    = L"S&ort by";
    sort->menuSvg  = m_context.menus.GetMenuSvg (L"SortBy");
    group->label   = L"Grou&p by";
    group->menuSvg = m_context.menus.GetMenuSvg (L"windows.groupbycolumn");

    items.push_back (DxuiPopupMenuItem::ForSubmenu (view,  GetViewChoices()));
    items.push_back (DxuiPopupMenuItem::ForSubmenu (sort,  GetSortChoices()));
    items.push_back (DxuiPopupMenuItem::ForSubmenu (group, GetGroupChoices()));

    m_context.menus.GetMenuCommands().push_back (std::move (view));
    m_context.menus.GetMenuCommands().push_back (std::move (sort));
    m_context.menus.GetMenuCommands().push_back (std::move (group));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::AddCopyAsMenu
//
//  The button row's Copy puts the selected files on the clipboard as real
//  files, in the style the Options dialog sets, and Copy as offers each style
//  for this copy alone. A paste into Explorer then writes them.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::AddCopyAsMenu (std::vector<DxuiPopupMenuItem> & items)
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
        child->dispatch = [this, style]() { m_context.fileOps.CopyEntriesToClipboard (style); };

        choices.push_back (DxuiPopupMenuItem::ForCommand (child));
        m_context.menus.GetMenuCommands().push_back (std::move (child));
    }

    parent->label = L"Cop&y as";
    parent->menuGlyph = s_kpszMdl2Copy;
    parent->menuSvg   = m_context.menus.GetMenuSvg (L"windows.copy");
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (choices)));
    m_context.menus.GetMenuCommands().push_back (std::move (parent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::AddConvertMenu
//
//  A BASIC program converts to its text listing, and a text file to an
//  Applesoft BASIC program. A file has one conversion, so it is a row of the
//  menu rather than a submenu.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::AddConvertMenu (std::vector<DxuiPopupMenuItem> & items)
{
    std::vector<FileEntry>          entries;
    std::shared_ptr<DxuiCommand>    command = std::make_shared<DxuiCommand>();
    BasicConversion::Kind           kind    = BasicConversion::Kind::None;



    m_context.browser.GetSelectedEntries (entries);

    if (entries.size() != 1)
    {
        return;
    }

    kind = BasicConversion::GetKind (m_context.browser.GetVolumeKind(), entries[0].type);

    command->label    = (kind == BasicConversion::Kind::ToListing) ? L"Con&vert to text listing" : L"Con&vert to Applesoft BASIC";
    command->dispatch = [this]() { m_context.commandManager.RunVerb (CassoExplorerActions::Verb::ConvertTo); };

    items.push_back (DxuiPopupMenuItem::ForCommand (command));
    m_context.menus.GetMenuCommands().push_back (std::move (command));
}
#include "Pch.h"

#include "CassoExplorer/Window/Managers/ExplorerListMenus.h"
#include "CassoExplorer/CassoExplorerWindow.h"
#include "CassoExplorer/Model/SearchQuery.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::ExplorerListMenus
//
////////////////////////////////////////////////////////////////////////////////

ExplorerListMenus::ExplorerListMenus (ExplorerListPane & pane, CassoExplorerWindow & window)
    : m_pane   (pane),
      m_window (window)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::ShowListHeaderMenu
//
//  Explorer's header menu: fit the column under the pointer, fit them all,
//  then every column with a check by those showing. Name always shows.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::ShowListHeaderMenu (int x, int y, int column)
{
    std::vector<DxuiPopupMenuItem>     items;
    std::vector<DxuiListView::Column>  columns = BrowserCells::GetColumns();



    m_window.m_menus.GetMenuCommands().clear();

    if (column >= 0)
    {
        m_window.m_menus.AddMenuCommand (items, L"Size column to &fit", [this, column]()
        {
            m_pane.m_list->FitColumnToContent ((size_t) column);
            m_window.Invalidate();
        });
    }

    m_window.m_menus.AddMenuCommand (items, L"Size &all columns to fit", [this]()
    {
        m_pane.m_list->FitAllColumnsToContent();
        m_window.Invalidate();
    });

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (size_t c = 0; c < columns.size() && c < m_pane.m_columns.m_chosen.size(); c++)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label     = columns[c].title;
        command->isChecked = [this, c]() { return m_pane.m_columns.m_chosen[c]; };
        command->isEnabled = [c]() { return c != (size_t) CatalogModel::Column::Name; };
        command->dispatch  = [this, c]()
        {
            m_pane.m_columns.m_chosen[c] = !m_pane.m_columns.m_chosen[c];

            //  This folder's choice, and the one others without their own follow.
            m_window.m_prefs.hiddenColumns.clear();

            for (size_t each = 0; each < m_pane.m_columns.m_chosen.size(); each++)
            {
                if (!m_pane.m_columns.m_chosen[each])
                {
                    m_window.m_prefs.hiddenColumns.push_back ((int) each);
                }
            }

            m_pane.m_columns.RememberFolderColumns();
            m_pane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_window.m_menus.GetMenuCommands().push_back (std::move (command));
    }

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items));
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
    bool                                             background     = m_window.m_browser.GetSelectedRows().empty() && group < 0;
    Location                                         shown          = m_window.m_browser.GetLocation();
    size_t                                           undoRow        = 0;



    m_window.m_menus.GetMenuCommands().clear();
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
    if (background && m_window.m_fileOps.CanUndo())
    {
        m_window.m_menus.AddMenuCommand (items, ExplorerFileOps::GetUndoLabel (m_window.m_fileOps.GetLastUndo().kind), [this]() { m_window.m_fileOps.UndoLast(); }, L"Ctrl+Z");
        m_window.m_menus.GetMenuCommands().back()->menuSvg = m_window.m_menus.GetMenuSvg (L"windows.undo");
        undoRow                                 = items.size();
    }

    //  From a group's header: open or close it, and all of them, with the
    //  keys that do the same on a focused header.
    if (group >= 0 && m_pane.m_columns.m_view != DxuiListView::View::List)
    {
        bool  collapsed = m_pane.m_list->IsGroupCollapsed (group);

        m_window.m_menus.AddMenuCommand (items, collapsed ? L"E&xpand group" : L"C&ollapse group", [this, group, collapsed]()
        {
            m_pane.m_list->SetGroupCollapsed (group, !collapsed);
            m_window.Invalidate();
        }, collapsed ? L"Right" : L"Left");
        m_window.m_menus.AddMenuCommand (items, L"&Expand all groups",   [this]() { m_pane.m_list->SetAllGroupsCollapsed (false); m_window.Invalidate(); });
        m_window.m_menus.AddMenuCommand (items, L"Co&llapse all groups", [this]() { m_pane.m_list->SetAllGroupsCollapsed (true);  m_window.Invalidate(); });
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    //  A folder or a disk image opens in a new tab, as Explorer's items do.
    if (m_window.m_browser.GetSelectedRows().size() == 1 && m_window.m_browser.TryGetRowLocation (m_window.m_browser.GetSelectedRows()[0], location))
    {
        browsable = true;

        m_window.m_menus.AddOpenRows (items, location, true);

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

            m_window.m_cassoLinkManager.AddConnectionRows (items, true, location);

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
        m_window.m_cassoLinkManager.AddConnectionRows (items, true, shown);
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

        if (ExplorerMenus::GetVerbGlyph (verb) != nullptr)
        {
            command           = std::make_shared<DxuiCommand>();
            command->id       = (int) verb;
            command->label    = ExplorerMenus::GetVerbLabel (verb);
            command->glyph    = ExplorerMenus::GetVerbGlyph (verb);
            command->menuSvg  = m_window.m_menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (verb));
            command->dispatch = [this, verb]() { m_window.m_commandManager.RunVerb (verb); };

            iconCommands.push_back (command);
            m_window.m_menus.GetMenuCommands().push_back (std::move (command));
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
            command->menuSvg  = m_window.m_menus.GetMenuSvg ((verb == CassoExplorerActions::Verb::NewFolder) ? L"newfolder" : L"casso.floppy525");
            command->dispatch = [this, verb]() { m_window.m_commandManager.RunVerb (verb); };

            if (verb == CassoExplorerActions::Verb::NewFolder)
            {
                command->isEnabled = [this]() { return !m_window.m_browser.IsVolumeLocation() || m_window.m_browser.GetVolumeKind() == VolumeKind::ProDos; };
                newFolder          = command;
            }
            else
            {
                newDisk = command;
            }

            m_window.m_menus.GetMenuCommands().push_back (std::move (command));
            continue;
        }

        if (verb == CassoExplorerActions::Verb::Refresh && (newFolder != nullptr || newDisk != nullptr))
        {
            std::shared_ptr<DxuiCommand>  parent = std::make_shared<DxuiCommand>();
            bool                          dos33  = m_window.m_browser.IsVolumeLocation() && m_window.m_browser.GetVolumeKind() != VolumeKind::ProDos;

            m_window.m_menus.AddNewChoices (newChoices, m_window.m_browser.GetLocation(), newFolder, newDisk);

            parent->label     = L"Ne&w";
            parent->menuGlyph = s_kpszMdl2Add;
            parent->menuSvg   = m_window.m_menus.GetMenuSvg (L"windows.newitem");
            parent->isEnabled = [dos33]() { return !dos33; };

            if (!items.empty() && items.back().kind != DxuiPopupMenuItem::Kind::Separator && items.size() != undoRow)
            {
                items.push_back (DxuiPopupMenuItem::ForSeparator());
            }

            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (newChoices)));
            m_window.m_menus.GetMenuCommands().push_back (std::move (parent));
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
        command->menuSvg   = m_window.m_menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (verb));
        command->dispatch  = [this, verb]() { m_window.m_commandManager.RunVerb (verb); };

        if (verb == CassoExplorerActions::Verb::Open)
        {
            command->menuImage   = m_window.m_menus.GetOpenMenuImage (browsable, m_window.m_fileOps.GetSelectedImagePath());
            command->accelerator = L"Enter";
        }

        //  A machine known to have one drive cannot take drive 2; one not
        //  described yet is given the benefit of the doubt.
        if (verb == CassoExplorerActions::Verb::InsertDrive2)
        {
            command->isEnabled = [this]() { return m_window.m_fileOps.GetCassoDriveCount() != 1; };
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
        m_window.m_menus.GetMenuCommands().push_back (std::move (command));

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
                child->dispatch = [this, raw]() { m_window.m_commandManager.RunRawVerb (raw); };

                advanced.push_back (DxuiPopupMenuItem::ForCommand (child));
                m_window.m_menus.GetMenuCommands().push_back (std::move (child));
            }

            parent->label = L"&Advanced";
            parent->menuGlyph = s_kpszMdl2Setting;
            items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (advanced)));
            m_window.m_menus.GetMenuCommands().push_back (std::move (parent));
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

        m_window.m_menus.AddMenuCommand (items, known ? L"&Remove from Casso" : L"&Add to Casso", [this, folderForCasso, known]()
        {
            m_window.m_folderSync.ChangeKnownFolder (folderForCasso, !known);
        });
    }

    //  From the current folder's background: Properties, then the commands
    //  installed apps add there, such as Open in Terminal, then the shell's
    //  own background menu.
    if (background && (shown.kind == Location::Kind::HostFolder || m_window.m_browser.IsImageLocation()) && !SearchQuery::IsId (shown.path))
    {
        m_window.m_menus.AddMenuCommand (items, L"P&roperties", [this, shown]() { m_window.m_fileOps.ShowLocationProperties (shown); }, L"Alt+Enter");
        m_window.m_menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
        m_window.m_menus.GetMenuCommands().back()->menuSvg   = m_window.m_menus.GetMenuSvg (L"windows.properties");
    }

    if (background && shown.kind == Location::Kind::HostFolder && !SearchQuery::IsId (shown.path))
    {
        POINT  screen = { x, y };

        ClientToScreen (m_window.GetHwnd(), &screen);

        m_window.m_menus.AddPackagedCommandsFor (items, { shown.path }, Win32PackagedCommands::ItemKind::Background);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_window.m_menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, shown, screen]()
        {
            HRESULT  hr = m_window.m_shellVerbs.ShowBackgroundMenu (m_window.GetHwnd(), shown.path, screen);

            IGNORE_RETURN_VALUE (hr, S_OK);
            m_window.m_folderSync.RefreshAfterHostChange();
        });
        m_window.m_menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
        m_window.m_menus.GetMenuCommands().back()->menuSvg   = m_window.m_menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));
    }

    //  A deleted item has no path to copy, and the bin itself describes it.
    if (!m_window.m_browser.GetSelectedRows().empty() && m_window.m_browser.GetLocation().kind == Location::Kind::RecycleBin)
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_window.m_menus.AddMenuCommand (items, L"P&roperties", [this]() { m_window.m_commandManager.RunRecycleBinVerb (CassoExplorerActions::Verb::Open); });
        m_window.m_menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
        m_window.m_menus.GetMenuCommands().back()->menuSvg   = m_window.m_menus.GetMenuSvg (L"windows.properties");
    }
    else if (!m_window.m_browser.GetSelectedRows().empty())
    {
        std::vector<std::wstring>        selectedPaths;
        Win32PackagedCommands::ItemKind  kind    = Win32PackagedCommands::ItemKind::File;
        bool                             windows = false;

        //  Windows' own commands lead into Copy as path in one group, as in
        //  File Explorer; without them the group starts here.
        if (m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder && m_window.m_menus.GetSelectionKind (selectedPaths, kind))
        {
            windows = m_window.m_menus.AddWindowsCommands (items, selectedPaths, kind);
        }

        if (!windows)
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        m_window.m_menus.AddMenuCommand (items, L"Copy as &path", [this]() { m_window.m_fileOps.CopySelectedPaths(); }, L"Ctrl+Shift+C", s_kpszMdl2CopyPath);
        m_window.m_menus.GetMenuCommands().back()->menuSvg = m_window.m_menus.GetMenuSvg (L"windows.copyaspath");

        if (m_window.m_browser.GetSelectedRows().size() == 1)
        {
            m_window.m_menus.AddMenuCommand (items, L"P&roperties", [this]()
            {
                if (!m_window.m_browser.GetSelectedRows().empty())
                {
                    m_window.m_fileOps.ShowRowProperties (m_window.m_browser.GetSelectedRows()[0]);
                }
            }, L"Alt+Enter");
            m_window.m_menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
            m_window.m_menus.GetMenuCommands().back()->menuSvg   = m_window.m_menus.GetMenuSvg (L"windows.properties");
        }
        else if (!selectedPaths.empty())
        {
            m_window.m_menus.AddMenuCommand (items, L"P&roperties", [this, selectedPaths]() { m_window.m_fileOps.ShowHostProperties (selectedPaths); }, L"Alt+Enter");
            m_window.m_menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
            m_window.m_menus.GetMenuCommands().back()->menuSvg   = m_window.m_menus.GetMenuSvg (L"windows.properties");
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
    if (moreOptions && m_window.m_browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        m_window.m_menus.AddPackagedCommands (items);
    }

    //  Last, as Explorer has it: everything else Windows and other programs
    //  offer for these items, in the shell's own menu.
    if (moreOptions)
    {
        POINT  screen = { x, y };

        ClientToScreen (m_window.GetHwnd(), &screen);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_window.m_menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, screen]()
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
                hr = m_window.m_shellVerbs.ShowShellMenu (m_window.GetHwnd(), paths, screen, [this]() { m_window.m_commandManager.RunVerb (CassoExplorerActions::Verb::Rename); });
            }

            IGNORE_RETURN_VALUE (hr, S_OK);
        });
        m_window.m_menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2OpenInNewWindow;
        m_window.m_menus.GetMenuCommands().back()->menuSvg   = m_window.m_menus.GetMenuSvg (ExplorerMenus::GetVerbMenuSvgName (CassoExplorerActions::Verb::MoreOptions));
    }

    //  The menu takes the focus look from the list while it is open, as
    //  Explorer's does: the item keeps its selection, drawn unfocused, and
    //  loses its focus border until the menu goes.
    listFocused = m_pane.m_list->IsListFocused();

    if (listFocused)
    {
        m_pane.m_list->SetListFocused (false);
        m_window.Invalidate();
    }

    DxuiContextMenu::Show (*m_window.GetPopupHost(), x, y, std::move (items), [this, listFocused] (bool)
    {
        if (listFocused && m_window.m_focus == ExplorerPane::List)
        {
            m_pane.m_list->SetListFocused (true);
            m_window.Invalidate();
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
        m_window.m_menus.AddMenuCommand (children, ExplorerAddressBar::EscapeMnemonics (handlers[i].name).c_str(), [this, path, i]()
        {
            HRESULT  opened = m_window.m_shellVerbs.OpenWith (m_window.GetHwnd(), path, i);

            IGNORE_RETURN_VALUE (opened, S_OK);
        });
    }

    if (!children.empty())
    {
        children.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    m_window.m_menus.AddMenuCommand (children, L"&Choose another app", [this, path]()
    {
        HRESULT  chosen = m_window.m_shellVerbs.ChooseOtherApp (m_window.GetHwnd(), path);

        IGNORE_RETURN_VALUE (chosen, S_OK);
    });

    parent->label = ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::OpenWith);
    parent->menuGlyph = s_kpszMdl2OpenWith;
    parent->menuSvg   = m_window.m_menus.GetMenuSvg (L"windows.openwith");
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (children)));
    m_window.m_menus.GetMenuCommands().push_back (std::move (parent));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::IsListVerbOffered
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListMenus::IsListVerbOffered (CassoExplorerActions::Verb verb) const
{
    std::vector<CassoExplorerActions::Verb>  verbs = m_window.m_actions.GetListVerbs();



    return std::find (verbs.begin(), verbs.end(), verb) != verbs.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetSortChoices
//
//  One row for each column in the list here, in Explorer's order, then the
//  sort direction.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetSortChoices()
{
    std::vector<DxuiPopupMenuItem>  items;
    size_t                          column = 0;



    for (column = 0; column < BrowserCells::GetColumns().size(); column++)
    {
        bool  chosen = column >= m_pane.m_columns.m_chosen.size() || m_pane.m_columns.m_chosen[column];

        if (!ExplorerListColumns::IsListColumnShown (column, chosen, m_window.m_browser.GetLocation().kind, SearchQuery::IsId (m_window.m_browser.GetLocation().path)))
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
//  ExplorerListMenus::GetGroupChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetGroupChoices()
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
        items.push_back (DxuiPopupMenuItem::ForCommand (m_window.m_commands.Find (CassoExplorerCommands::kViewFirst + view)));
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
    view->menuSvg  = m_window.m_menus.GetMenuSvg (L"View");
    sort->label    = L"S&ort by";
    sort->menuSvg  = m_window.m_menus.GetMenuSvg (L"SortBy");
    group->label   = L"Grou&p by";
    group->menuSvg = m_window.m_menus.GetMenuSvg (L"windows.groupbycolumn");

    items.push_back (DxuiPopupMenuItem::ForSubmenu (view,  GetViewChoices()));
    items.push_back (DxuiPopupMenuItem::ForSubmenu (sort,  GetSortChoices()));
    items.push_back (DxuiPopupMenuItem::ForSubmenu (group, GetGroupChoices()));

    m_window.m_menus.GetMenuCommands().push_back (std::move (view));
    m_window.m_menus.GetMenuCommands().push_back (std::move (sort));
    m_window.m_menus.GetMenuCommands().push_back (std::move (group));
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
        child->dispatch = [this, style]() { m_window.m_fileOps.CopyEntriesToClipboard (style); };

        choices.push_back (DxuiPopupMenuItem::ForCommand (child));
        m_window.m_menus.GetMenuCommands().push_back (std::move (child));
    }

    parent->label = L"Cop&y as";
    parent->menuGlyph = s_kpszMdl2Copy;
    parent->menuSvg   = m_window.m_menus.GetMenuSvg (L"windows.copy");
    items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (choices)));
    m_window.m_menus.GetMenuCommands().push_back (std::move (parent));
}

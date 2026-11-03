#include "Pch.h"
#include "CassoExplorer/Window/Managers/ExplorerPreviewManager.h"
#include "Seams/ShellItemId.h"

#include "CassoExplorer/Window/Managers/ExplorerListMenus.h"
#include "CassoExplorer/Window/Panes/ExplorerAddressBar.h"
#include "CassoExplorer/Window/Panes/ExplorerToolbars.h"
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
//  visible ones, and More... for Choose Details. Name is always visible.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::ShowListHeaderMenu (int x, int y, int column)
{
    std::vector<DxuiPopupMenuItem>     items;
    std::vector<DxuiListView::Column>  columns   = m_pane.GetColumns().GetListColumns();
    Location                           location  = m_context.browser.GetLocation();
    bool                               searching = SearchQuery::IsId (location.path);



    m_context.menus.GetMenuCommands().clear();

    if (column >= 0)
    {
        m_context.menus.AddMenuCommand (items, L"Size Column to &Fit", [this, column]()
        {
            m_pane.GetListView()->FitColumnToContent ((size_t) column);
            m_context.window.Invalidate();
        });
    }

    m_context.menus.AddMenuCommand (items, L"Size &All Columns to Fit", [this]()
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
            m_context.toolbars.SetCommandBarDropDowns();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    //  File Explorer's Choose Details, for the shell's other properties.
    m_context.menus.AddMenuCommand (items, L"&More...", [this]() { m_pane.GetColumns().ChooseDetails(); });

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::ShowHomeItemMenu
//
//  File Explorer's menu for an item on Home: Share across the top, then the
//  commands the shell has for the item, in its order, each run by its label.
//  A file on a disk also has the full menu.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::ShowHomeItemMenu (int x, int y, int row)
{
    struct Entry
    {
        const wchar_t *  label;
        const wchar_t *  shown;
        const wchar_t *  svg;
        const wchar_t *  accelerator;
    };

    static constexpr Entry  s_kEntries[] =
    {
        { L"Open",                 L"Open",                 nullptr,                             L"Enter" },
        { L"Share",                L"Share",                L"Windows.ModernShare",              L""      },
        { L"Open file location",   L"Open file location",   L"windows.shortcut.opencontaining",  L""      },
        { L"Remove from list",     L"Remove from list",     L"windows.close",                    L""      },
        { L"Add to Favorites",     L"Add to Favorites",     nullptr,                             L""      },
        { L"Remove from Favorites", L"Remove from Favorites", nullptr,                           L""      },
        { L"Copy as path",         L"Copy as path",         L"windows.copyaspath",               L"Ctrl+Shift+C" },
        { L"Properties",           L"Properties",           nullptr,                             L"Alt+Enter" },
    };

    std::vector<DxuiPopupMenuItem>  items;
    std::vector<std::wstring>       labels;
    std::wstring                    id;
    HRESULT                         hr   = S_OK;
    HWND                            hwnd = m_context.window.GetHwnd();



    if (row < 0 || (size_t) row >= m_context.browser.GetRows().size())
    {
        return;
    }

    {
        const CatalogRow &  item = m_context.browser.GetRows()[(size_t) row];

        id = (item.hostPath.empty() || ShellItemId::IsEncoded (item.shellId)) ? item.shellId : item.hostPath;
    }

    hr = m_context.shellVerbs.GetItemVerbLabels (hwnd, { id }, labels);
    IGNORE_RETURN_VALUE (hr, S_OK);

    for (const Entry & entry : s_kEntries)
    {
        if (std::find (labels.begin(), labels.end(), entry.label) == labels.end())
        {
            continue;
        }

        m_context.menus.AddMenuCommand (items, entry.shown, [this, id, label = std::wstring (entry.label)]()
        {
            HRESULT  ran = m_context.shellVerbs.RunItemVerbByLabel (m_context.window.GetHwnd(), { id }, label);

            IGNORE_RETURN_VALUE (ran, S_OK);

            //  The row goes by itself, as in File Explorer.
            if (label == L"Remove from list" && SUCCEEDED (ran))
            {
                m_context.browser.RemoveHomeRow (id);
                m_pane.FillList();
                m_pane.GetListView()->SetSelectedRows ({}, -1);
                m_context.preview.FillPreview();
                m_context.window.Invalidate();
            }

            //  Favorites and the list change with these.
            else if (label != L"Open" && label != L"Share" && label != L"Properties" && label != L"Copy as path")
            {
                ran = m_context.browser.Refresh();
                IGNORE_RETURN_VALUE (ran, S_OK);
                m_pane.FillList();
                m_context.window.Invalidate();
            }
        }, entry.accelerator);

        if (entry.svg != nullptr)
        {
            m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (entry.svg);
        }
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    m_context.menus.AddMenuCommand (items, L"Show more options", [this, id]()
    {
        POINT    screen = {};
        BOOL     got    = GetCursorPos (&screen);
        HRESULT  shown  = m_context.shellVerbs.ShowShellMenu (m_context.window.GetHwnd(), { id }, screen, {});

        IGNORE_RETURN_VALUE (got, TRUE);
        IGNORE_RETURN_VALUE (shown, S_OK);
    }, L"Shift+F10");

    DxuiContextMenu::Show (*m_context.window.GetPopupHost(), x, y, std::move (items));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::AddHomeItemRows
//
//  The commands an item has only on Home, each run by its label.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::AddHomeItemRows (std::vector<DxuiPopupMenuItem> & items, const std::wstring & id)
{
    static constexpr std::pair<const wchar_t *, const wchar_t *>  s_kRows[] =
    {
        { L"Open file location",    L"windows.shortcut.opencontaining" },
        { L"Remove from list",      L"windows.close" },
        { L"Add to Favorites",      nullptr },
        { L"Remove from Favorites", nullptr },
    };



    std::vector<std::wstring>       labels;
    std::vector<DxuiPopupMenuItem>  rows;
    HRESULT                         hr           = m_context.shellVerbs.GetItemVerbLabels (m_context.window.GetHwnd(), { id }, labels);
    size_t                          place        = items.size();
    bool                            hasFavorites = false;



    IGNORE_RETURN_VALUE (hr, S_OK);

    //  Windows' commands may have Favorites already, which these rows go above.
    for (size_t index = 0; index < items.size() && !hasFavorites; index++)
    {
        if (items[index].command != nullptr && items[index].command->label.find (L"Favorites") != std::wstring::npos)
        {
            place        = index;
            hasFavorites = true;
        }
    }

    if (hasFavorites && IsFavorite (labels))
    {
        items.erase (items.begin() + (std::ptrdiff_t) place);
        hasFavorites = false;
    }

    for (const auto & [label, svg] : s_kRows)
    {
        if (std::find (labels.begin(), labels.end(), label) == labels.end() || (hasFavorites && svg == nullptr))
        {
            continue;
        }

        m_context.menus.AddMenuCommand (rows, label, [this, id, name = std::wstring (label)]()
        {
            HRESULT  ran = m_context.shellVerbs.RunItemVerbByLabel (m_context.window.GetHwnd(), { id }, name);

            IGNORE_RETURN_VALUE (ran, S_OK);

            //  The row goes by itself, as in File Explorer.
            if (name == L"Remove from list" && SUCCEEDED (ran))
            {
                m_context.browser.RemoveHomeRow (id);
                m_pane.FillList();
                m_pane.GetListView()->SetSelectedRows ({}, -1);
                m_context.preview.FillPreview();
                m_context.window.Invalidate();
            }
            else if (name != L"Open file location")
            {
                ran = m_context.browser.Refresh();
                IGNORE_RETURN_VALUE (ran, S_OK);
                m_pane.FillList();
                m_context.window.Invalidate();
            }
        });
        m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (svg);
        m_context.menus.GetMenuCommands().back()->menuGlyph = (svg == nullptr) ? L"\xE734" : nullptr;
    }

    items.insert (items.begin() + (std::ptrdiff_t) place, std::make_move_iterator (rows.begin()), std::make_move_iterator (rows.end()));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::AddOpenLocationRow
//
//  Open file location, ahead of Add to Favorites and Compress to, where File
//  Explorer lists it among Windows' commands.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::AddOpenLocationRow (std::vector<DxuiPopupMenuItem> & items, int row)
{
    std::vector<DxuiPopupMenuItem>  rows;
    size_t                          place = items.size();



    for (size_t index = items.size(); index > 0; index--)
    {
        const DxuiPopupMenuItem &  item = items[index - 1];

        if (item.command != nullptr && (item.command->label.find (L"Favorites") != std::wstring::npos || item.command->label.starts_with (L"Compress")))
        {
            place = index - 1;
        }
    }

    m_context.menus.AddMenuCommand (rows, L"Open file &location", [this, row]() { m_pane.HandleHomeRowAction (row, 0); });
    m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"windows.shortcut.opencontaining");

    items.insert (items.begin() + (std::ptrdiff_t) place, std::make_move_iterator (rows.begin()), std::make_move_iterator (rows.end()));
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



    //  Home's items have the shell's own menu: Add to Favorites, Remove from
    //  list and the rest are its commands.
    if (m_context.browser.IsHomeLocation() && group < 0 && m_context.browser.GetSelectedRows().size() == 1 && m_context.browser.GetRows()[(size_t) m_context.browser.GetSelectedRows()[0]].hostPath.empty())
    {
        ShowHomeItemMenu (x, y, m_context.browser.GetSelectedRows()[0]);
        return;
    }

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

    if (background)
    {
        AddBackgroundRows (items, shown);
    }

    if (background && !GetBackgroundShellId (shown).empty())
    {
        POINT         screen  = { x, y };
        std::wstring  shellId = GetBackgroundShellId (shown);
        bool          onDisk  = shown.kind == Location::Kind::HostFolder || m_pane.GetColumns().GetGroupFolder() == GroupChoices::Folder::Zip;

        ClientToScreen (m_context.window.GetHwnd(), &screen);

        //  A folder that isn't on the disk has no item for the commands to act
        //  on, as File Explorer's menu there asks them with none.
        m_context.menus.AddPackagedCommandsFor (items, onDisk ? std::vector<std::wstring> { shellId } : std::vector<std::wstring>(), Win32PackagedCommands::ItemKind::Background);

        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_context.menus.AddMenuCommand (items, ExplorerMenus::GetVerbLabel (CassoExplorerActions::Verb::MoreOptions), [this, shellId, screen]()
        {
            HRESULT  hr = m_context.shellVerbs.ShowBackgroundMenu (m_context.window.GetHwnd(), shellId, screen);

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
        if ((m_context.browser.GetLocation().kind == Location::Kind::HostFolder || m_context.browser.IsHomeLocation() || m_context.browser.AreSelectedRowsSearchFiles())
            && m_context.menus.GetSelectionKind (selectedPaths, kind))
        {
            std::vector<std::wstring>  homeIds;

            if (m_context.browser.IsHomeLocation() && m_context.browser.GetSelectedRows().size() == 1)
            {
                homeIds.push_back (m_context.browser.GetRows()[(size_t) m_context.browser.GetSelectedRows()[0]].shellId);
            }

            m_context.menus.SetCommandItemIds (homeIds);
            windows = m_context.menus.AddWindowsCommands (items, selectedPaths, kind);
        }

        if (!windows)
        {
            items.push_back (DxuiPopupMenuItem::ForSeparator());
        }

        //  Home's commands for a file, which its place in Home gives it.
        if (m_context.browser.IsHomeLocation() && m_context.browser.GetSelectedRows().size() == 1)
        {
            AddHomeItemRows (items, m_context.browser.GetRows()[(size_t) m_context.browser.GetSelectedRows()[0]].shellId);
        }

        //  A search's file opens its folder with it selected, as File
        //  Explorer's Open file location does there.
        if (m_context.browser.AreSelectedRowsSearchFiles() && m_context.browser.GetSelectedRows().size() == 1)
        {
            AddOpenLocationRow (items, m_context.browser.GetSelectedRows()[0]);
        }

        //  Drives, from This PC or wherever they are listed, by each row's path.
        {
            std::vector<std::wstring>  rowPaths;
            std::wstring               rowPath;

            for (int row : m_context.browser.GetSelectedRows())
            {
                if (m_context.browser.TryGetRowPath (row, rowPath))
                {
                    rowPaths.push_back (rowPath);
                }
            }

            m_context.menus.AddShellMenuCommands (items, rowPaths);
        }

        m_context.menus.AddMenuCommand (items, L"Copy as &path", [this]() { m_context.fileOps.CopySelectedPaths(); }, L"Ctrl+Shift+C", s_kpszMdl2CopyPath);
        m_context.menus.GetMenuCommands().back()->menuSvg = m_context.menus.GetMenuSvg (L"windows.copyaspath");

        if (m_context.browser.GetSelectedRows().size() == 1)
        {
            m_context.menus.AddMenuCommand (items, L"P&roperties", [this]()
            {
                if (!m_context.browser.GetSelectedRows().empty())
                {
                    const CatalogRow &  row = m_context.browser.GetRows()[(size_t) m_context.browser.GetSelectedRows()[0]];

                    //  A cloud item on Home has no path, only its place in the shell.
                    if (row.homeGroup > 0 && row.hostPath.empty())
                    {
                        HRESULT  ran = m_context.shellVerbs.RunItemVerbByLabel (m_context.window.GetHwnd(), { row.shellId }, L"Properties");

                        IGNORE_RETURN_VALUE (ran, S_OK);
                        return;
                    }

                    m_context.fileOps.ShowRowProperties (m_context.browser.GetSelectedRows()[0]);
                }
            }, L"Alt+Enter");
            m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
            m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
        }

        //  An Apple file on the PC has its Apple properties beside Windows'.
        if (selectedPaths.size() == 1 && ExplorerFileOps::IsHostAppleFile (selectedPaths[0]))
        {
            std::wstring  path = selectedPaths[0];

            m_context.menus.AddMenuCommand (items, L"&Apple properties", [this, path]() { m_context.fileOps.ShowHostAppleProperties (path); });
            m_context.menus.GetMenuCommands().back()->menuImage = m_context.shellIcons.GetForKind (IShellIcons::Kind::AppleMachine);
        }

        if (m_context.browser.GetSelectedRows().size() > 1 && m_context.browser.IsVolumeLocation())
        {
            m_context.menus.AddMenuCommand (items, L"P&roperties", [this]() { m_context.fileOps.ShowAppleProperties(); }, L"Alt+Enter");
            m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
            m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
        }
        else if (m_context.browser.GetSelectedRows().size() > 1 && !selectedPaths.empty())
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
    if (moreOptions && (m_context.browser.GetLocation().kind == Location::Kind::HostFolder || m_context.browser.IsHomeLocation() || m_context.browser.AreSelectedRowsSearchFiles()))
    {
        m_context.menus.AddPackagedCommands (items);
        m_context.menus.SetCommandItemIds ({});
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
//  ExplorerListMenus::GetBackgroundShellId
//
//  The shell's id for the folder on display, for the commands of its
//  background; empty where the shell has no such folder.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ExplorerListMenus::GetBackgroundShellId (const Location & shown) const
{
    if (SearchQuery::IsId (shown.path))
    {
        return std::wstring();
    }

    if (m_context.browser.IsHomeLocation())
    {
        return IShellItemVerbs::kHomeId;
    }

    switch (shown.kind)
    {
        case Location::Kind::HostFolder:
        case Location::Kind::ShellFolder:
            return shown.path;

        case Location::Kind::RecycleBin:
            return L"::{645FF040-5081-101B-9F08-00AA002F954E}";

        case Location::Kind::Root:
            return (shown.path == TreeModel::kThisPcRootId) ? L"::{20D04FE0-3AEA-1069-A2D8-08002B30309D}" : std::wstring();

        default:
            return std::wstring();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::AddBackgroundRows
//
//  The rows File Explorer has on the background of these folders alone.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::AddBackgroundRows (std::vector<DxuiPopupMenuItem> & items, const Location & shown)
{
    std::wstring  shellId = GetBackgroundShellId (shown);
    std::wstring  lower   = shown.path;
    HWND          owner   = m_context.window.GetHwnd();
    auto          addProperties = [this, owner, shellId, &items]()
    {
        m_context.menus.AddMenuCommand (items, L"P&roperties", [owner, shellId]()
        {
            SHELLEXECUTEINFOW  info = { sizeof (info) };
            std::wstring       file = L"shell:" + shellId;
            BOOL               ran  = FALSE;

            info.fMask  = SEE_MASK_INVOKEIDLIST;
            info.hwnd   = owner;
            info.lpVerb = L"properties";
            info.lpFile = file.c_str();
            info.nShow  = SW_SHOWNORMAL;
            ran         = ShellExecuteExW (&info);

            IGNORE_RETURN_VALUE (ran, TRUE);
        }, L"Alt+Enter");
        m_context.menus.GetMenuCommands().back()->menuGlyph = s_kpszMdl2Repair;
        m_context.menus.GetMenuCommands().back()->menuSvg   = m_context.menus.GetMenuSvg (L"windows.properties");
    };



    std::transform (lower.begin(), lower.end(), lower.begin(), towlower);

    if (shown.kind == Location::Kind::Root && shown.path == TreeModel::kThisPcRootId)
    {
        m_context.menus.AddMenuCommand (items, L"Add a &network location", []()
        {
            ShellExecuteW (nullptr, L"open", L"rundll32.exe", L"shwebsvc.dll,AddNetPlaceRunDll", nullptr, SW_SHOWNORMAL);
        });
        addProperties();
    }
    else if (shown.kind == Location::Kind::RecycleBin)
    {
        addProperties();
    }
    else if (m_pane.GetColumns().GetGroupFolder() == GroupChoices::Folder::Zip)
    {
        m_context.menus.AddMenuCommand (items, L"E&xtract All...", [this]() { m_context.commandManager.Dispatch (CassoExplorerCommands::kExtractAll); });
    }
    else if (lower.find (L"shell:libraries") != std::wstring::npos || lower.find (L"031e4825-7b94-4dc3-b131-e946b44c8dd5") != std::wstring::npos)
    {
        std::shared_ptr<DxuiCommand>    parent = std::make_shared<DxuiCommand>();
        std::vector<DxuiPopupMenuItem>  rows;

        m_context.menus.AddMenuCommand (rows, L"&Library", [this]()
        {
            HRESULT  hr = CreateLibrary();

            IGNORE_RETURN_VALUE (hr, S_OK);
            m_context.folderSync.RefreshAfterHostChange();
        });

        parent->label     = L"Ne&w";
        parent->menuGlyph = m_context.commands.Find (CassoExplorerCommands::kNew)->glyph;
        items.push_back (DxuiPopupMenuItem::ForSubmenu (parent, std::move (rows)));
        m_context.menus.GetMenuCommands().push_back (std::move (parent));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::CreateLibrary
//
//  A new, empty library under the name File Explorer gives one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ExplorerListMenus::CreateLibrary()
{
    HRESULT                 hr      = S_OK;
    ComPtr<IShellLibrary>   library;
    ComPtr<IShellItem>      saved;
    PWSTR                   folder  = nullptr;



    hr = SHCreateLibrary (IID_PPV_ARGS (&library));
    CHR (hr);

    hr = SHGetKnownFolderPath (FOLDERID_Libraries, 0, nullptr, &folder);
    CHR (hr);

    hr = SHSaveLibraryInFolderPath (library.Get(), folder, L"New Library", LSF_MAKEUNIQUENAME, nullptr);
    CHR (hr);

Error:
    CoTaskMemFree (folder);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetSortChoices
//
//  One row for each column in the list here, in Explorer's order, then the
//  sort direction. The command bar's for a host folder or a search is File
//  Explorer's instead: three choices, then More.
//
//  The rows of the Sort, Group by and View menus, shared by the command
//  bar and the list's background menu.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetSortChoices (bool commandBar)
{
    std::vector<DxuiPopupMenuItem>  items;
    size_t                          column = 0;



    if (SearchQuery::IsId (m_context.browser.GetLocation().path))
    {
        return GetGroupedSortChoices (GroupChoices::Folder::Search);
    }

    if (commandBar && m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        return GetGroupedSortChoices (m_pane.GetColumns().GetGroupFolder());
    }

    for (column = 0; column < (size_t) m_context.commandManager.GetSortColumnCount(); column++)
    {
        bool                          chosen  = column >= m_pane.GetColumns().GetChosen().size() || m_pane.GetColumns().GetChosen()[column];
        int                           id      = CassoExplorerCommands::kSortByColumn + (int) column;
        std::shared_ptr<DxuiCommand>  command;

        if (!ExplorerListColumns::IsListColumnShown (column, chosen, m_context.browser.GetLocation().kind, SearchQuery::IsId (m_context.browser.GetLocation().path)))
        {
            continue;
        }

        if (column < CatalogModel::kColumnCount)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (id)));
            continue;
        }

        //  A shell property's column has its command made here, by its title.
        command            = std::make_shared<DxuiCommand>();
        command->label     = m_pane.GetColumns().GetListColumns()[column].title;
        command->radio     = true;
        command->isChecked = [this, id]() { return m_context.commandManager.IsChecked (id); };
        command->dispatch  = [this, id]() { m_context.commandManager.Dispatch (id); };

        items.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kSortAscending)));
    items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kSortDescending)));

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetSeeMoreRows
//
//  File Explorer's See more rows: Undo; Windows' Compress and Add to
//  favorites with Copy path for a selection, or Pin to Quick access for the
//  folder; the selection commands; and Properties.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetSeeMoreRows()
{
    std::vector<DxuiPopupMenuItem>   items;
    std::vector<DxuiPopupMenuItem>   windows;
    std::vector<std::wstring>        paths;
    Win32PackagedCommands::ItemKind  kind     = Win32PackagedCommands::ItemKind::File;
    Location                         location = m_context.browser.GetLocation();
    bool                             selected = !m_context.browser.GetSelectedRows().empty();
    bool                             search   = SearchQuery::IsId (location.path);
    bool                             onHost   = location.kind == Location::Kind::HostFolder && !search;
    auto                             row      = [this, &items] (const wchar_t * label, const wchar_t * svg, std::function<bool()> enabled, std::function<void()> dispatch)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label     = label;
        command->menuSvg   = m_context.menus.GetMenuSvg (svg);
        command->isEnabled = std::move (enabled);
        command->dispatch  = std::move (dispatch);

        items.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
    };



    row (L"Undo", L"windows.undo", [this]() { return m_context.commandManager.IsEnabled (CassoExplorerCommands::kUndo); },
         [this]() { m_context.commandManager.Dispatch (CassoExplorerCommands::kUndo); });
    items.push_back (DxuiPopupMenuItem::ForSeparator());

    if (selected && m_context.menus.GetSelectionKind (paths, kind))
    {
        if (m_context.menus.AddWindowsCommands (windows, paths, kind))
        {
            for (DxuiPopupMenuItem & command : windows)
            {
                const std::wstring  label = (command.command != nullptr) ? command.command->label : std::wstring();

                if (label.find (L"Compress") != std::wstring::npos || label.find (L"favorites") != std::wstring::npos)
                {
                    items.push_back (std::move (command));
                }
            }
        }

        row (L"Copy path", L"windows.copyaspath", nullptr, [this]() { m_context.fileOps.CopySelectedPaths(); });
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }
    else if (onHost)
    {
        m_context.menus.AddPinMenuCommand (items, location.path);
        items.push_back (DxuiPopupMenuItem::ForSeparator());
    }

    row (L"Select all", L"windows.selectall", nullptr, [this]()
    {
        m_pane.GetListView()->SelectAllRows();
        m_pane.OnSelectionChanged();
        m_context.window.Invalidate();
    });

    row (L"Select none", L"windows.selectnone", nullptr, [this]() { m_pane.SetListSelection ({}); });

    row (L"Invert selection", L"windows.invertselection", nullptr, [this]()
    {
        const std::vector<int> &  current = m_pane.GetListView()->GetSelectedRows();
        std::vector<int>          rows;

        for (int at = 0; at < (int) m_pane.GetListView()->GetRowCount(); at++)
        {
            if (std::find (current.begin(), current.end(), at) == current.end())
            {
                rows.push_back (at);
            }
        }

        m_pane.SetListSelection (std::move (rows));
    });

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    row (L"Properties", L"windows.properties", [this, selected, onHost]() { return selected || onHost || m_context.browser.IsImageLocation(); }, [this, selected, location]()
    {
        if (selected)
        {
            m_context.fileOps.ShowRowProperties (m_context.browser.GetSelectedRows()[0]);
        }
        else
        {
            m_context.fileOps.ShowLocationProperties (location);
        }
    });

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetGroupedSortChoices
//
//  File Explorer's command bar Sort menu: the first three of its Group by
//  choices, then up to five more under More. A property the list has no
//  column for gets one only when chosen.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetGroupedSortChoices (GroupChoices::Folder folder)
{
    static constexpr size_t  s_kTopChoices  = 3;
    static constexpr size_t  s_kMoreChoices = 5;



    std::vector<DxuiPopupMenuItem>     items;
    std::vector<DxuiPopupMenuItem>     more;
    std::vector<int>                   moreIds;
    std::vector<GroupChoices::Choice>  choices = GroupChoices::GetChoices (folder);
    std::shared_ptr<DxuiCommand>       parent  = std::make_shared<DxuiCommand>();
    auto                               idFor   = [this] (const GroupChoices::Choice & choice) -> int
    {
        const std::vector<std::wstring> &  names = m_context.prefs.propertyColumns;
        auto                               found = (choice.property != nullptr) ? std::find (names.begin(), names.end(), choice.property) : names.end();

        switch (choice.field)
        {
            case RowGrouping::Field::Name:         return CassoExplorerCommands::kSortByColumn + (int) CatalogModel::Column::Name;
            case RowGrouping::Field::DateModified: return CassoExplorerCommands::kSortByColumn + (int) CatalogModel::Column::Modified;
            case RowGrouping::Field::Type:         return CassoExplorerCommands::kSortByColumn + (int) CatalogModel::Column::Type;
            case RowGrouping::Field::Size:         return CassoExplorerCommands::kSortByColumn + (int) CatalogModel::Column::Size;
            case RowGrouping::Field::Folder:       return CassoExplorerCommands::kSortByColumn + (int) CatalogModel::Column::Folder;
            case RowGrouping::Field::Relevance:    return CassoExplorerCommands::kSortByRelevance;
            default:                               return (found != names.end()) ? CassoExplorerCommands::kSortByColumn + (int) (CatalogModel::kColumnCount + (size_t) (found - names.begin())) : 0;
        }
    };



    for (size_t index = 0; index < choices.size() && index < s_kTopChoices + s_kMoreChoices; index++)
    {
        GroupChoices::Choice          choice  = choices[index];
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        int                           id      = idFor (choice);

        command->label     = choice.label;
        command->radio     = true;
        command->isChecked = [this, id]() { return id != 0 && m_context.commandManager.IsChecked (id); };
        command->dispatch  = [this, choice, idFor]()
        {
            int  sortId = idFor (choice);

            //  A property with no column yet is given one, hidden, to sort by.
            if (sortId == 0 && choice.property != nullptr)
            {
                RowGrouping::Field  field = m_pane.GetColumns().GetPropertyField (choice.property);

                sortId = CassoExplorerCommands::kSortByColumn + (int) CatalogModel::kColumnCount + ((int) field - RowGrouping::kFirstPropertyField);
            }

            m_context.commandManager.Dispatch (sortId);
        };

        if (index < s_kTopChoices)
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
            continue;
        }

        moreIds.push_back (id);
        more.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
    }

    //  More is marked while the sort is one of its choices, as in File Explorer.
    parent->label     = L"More";
    parent->radio     = true;
    parent->isChecked = [this, moreIds]()
    {
        return std::any_of (moreIds.begin(), moreIds.end(), [this] (int id) { return id != 0 && m_context.commandManager.IsChecked (id); });
    };

    items.push_back (DxuiPopupMenuItem::ForSubmenu (std::move (parent), std::move (more)));
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

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetGroupChoices (bool commandBar)
{
    std::vector<DxuiPopupMenuItem>     items;
    std::vector<GroupChoices::Choice>  choices = GroupChoices::GetChoices (m_pane.GetColumns().GetGroupFolder());
    std::vector<std::wstring>          listed;
    std::shared_ptr<DxuiCommand>       more    = std::make_shared<DxuiCommand>();
    bool                               search  = SearchQuery::IsId (m_context.browser.GetLocation().path);
    bool                               brief   = commandBar || search;
    bool                               grouped = m_context.browser.GetBrowserModel().HasTabs() && m_context.browser.GetBrowserModel().GetActiveTab().groupBy != RowGrouping::Field::None;
    auto                               isGroup = [this] (RowGrouping::Field field)
    {
        return m_context.browser.GetBrowserModel().HasTabs() && m_context.browser.GetBrowserModel().GetActiveTab().groupBy == field;
    };
    auto                               add     = [&items] (const std::wstring & label, std::function<bool()> isChecked, std::function<void()> dispatch)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label     = label;
        command->radio     = true;
        command->isChecked = std::move (isChecked);
        command->dispatch  = std::move (dispatch);

        items.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
    };



    for (const GroupChoices::Choice & choice : choices)
    {
        std::wstring        property = (choice.property != nullptr) ? choice.property : L"";
        RowGrouping::Field  field    = choice.field;

        listed.push_back (property);

        //  A property the list has no column for gets one only when chosen.
        add (choice.label,
             [this, isGroup, property, field]()
             {
                 const std::vector<std::wstring> &  names = m_context.prefs.propertyColumns;
                 auto                               found = std::find (names.begin(), names.end(), property);

                 return property.empty() ? isGroup (field) : (found != names.end() && isGroup (RowGrouping::MakePropertyField ((size_t) (found - names.begin()))));
             },
             [this, property, field]()
             {
                 m_context.commandManager.Dispatch (CassoExplorerCommands::kGroupByField + (int) (property.empty() ? field : m_pane.GetColumns().GetPropertyField (property)));
             });
    }

    //  File Explorer's background menu in a folder has the columns on display.
    if (!brief && m_context.browser.GetLocation().kind == Location::Kind::HostFolder)
    {
        return GetColumnGroupChoices (grouped);
    }

    //  A column on display can be grouped by too, as Explorer's Path can. The
    //  command bar's menu and a search's have File Explorer's choices only.
    for (size_t index = 0; !brief && index < m_context.prefs.propertyColumns.size(); index++)
    {
        size_t              column = CatalogModel::kColumnCount + index;
        bool                shown  = column < m_pane.GetColumns().GetChosen().size() && m_pane.GetColumns().GetChosen()[column];
        RowGrouping::Field  field  = RowGrouping::MakePropertyField (index);

        if (shown && std::find (listed.begin(), listed.end(), m_context.prefs.propertyColumns[index]) == listed.end())
        {
            add (m_pane.GetColumns().GetListColumns()[column].title,
                 [isGroup, field]() { return isGroup (field); },
                 [this, field]() { m_context.commandManager.Dispatch (CassoExplorerCommands::kGroupByField + (int) field); });
        }
    }

    //  (None) only while grouped, as File Explorer's.
    if (grouped)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kGroupByField)));
    }

    if (!brief)
    {
        more->label    = L"More";
        more->dispatch = [this]() { m_pane.GetColumns().ChooseDetails(); };
        items.push_back (DxuiPopupMenuItem::ForCommand (std::move (more)));
    }

    //  These have a direction to group in only while grouped.
    if (brief && !grouped)
    {
        return items;
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kGroupAscending)));
    items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kGroupDescending)));

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetColumnGroupChoices
//
//  File Explorer's background Group by in a folder: a row for each column on
//  display, the direction, grayed while ungrouped, and More..., which opens
//  Choose Details.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetColumnGroupChoices (bool grouped)
{
    static constexpr std::pair<CatalogModel::Column, RowGrouping::Field>  s_kFixed[] =
    {
        { CatalogModel::Column::Name,     RowGrouping::Field::Name         },
        { CatalogModel::Column::Modified, RowGrouping::Field::DateModified },
        { CatalogModel::Column::Type,     RowGrouping::Field::Type         },
        { CatalogModel::Column::Size,     RowGrouping::Field::Size         },
    };

    std::vector<DxuiPopupMenuItem>  items;
    std::vector<bool> &             chosen  = m_pane.GetColumns().GetChosen();
    std::shared_ptr<DxuiCommand>    more    = std::make_shared<DxuiCommand>();
    auto                            isGroup = [this] (RowGrouping::Field field)
    {
        return m_context.browser.GetBrowserModel().HasTabs() && m_context.browser.GetBrowserModel().GetActiveTab().groupBy == field;
    };
    auto                            add     = [this, &items, isGroup] (const std::wstring & label, RowGrouping::Field field)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label     = label;
        command->radio     = true;
        command->isChecked = [isGroup, field]() { return isGroup (field); };
        command->dispatch  = [this, field]() { m_context.commandManager.Dispatch (CassoExplorerCommands::kGroupByField + (int) field); };

        items.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
    };



    for (const auto & [column, field] : s_kFixed)
    {
        if ((size_t) column >= chosen.size() || chosen[(size_t) column])
        {
            add (m_pane.GetColumns().GetListColumns()[(size_t) column].title, field);
        }
    }

    for (size_t index = 0; index < m_context.prefs.propertyColumns.size(); index++)
    {
        size_t  column = CatalogModel::kColumnCount + index;

        if (column < chosen.size() && chosen[column])
        {
            add (m_pane.GetColumns().GetListColumns()[column].title, RowGrouping::MakePropertyField (index));
        }
    }

    if (grouped)
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (m_context.commands.Find (CassoExplorerCommands::kGroupByField)));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (int id : { (int) CassoExplorerCommands::kGroupAscending, (int) CassoExplorerCommands::kGroupDescending })
    {
        std::shared_ptr<const DxuiCommand>  base    = m_context.commands.Find (id);
        std::shared_ptr<DxuiCommand>        command = (base != nullptr) ? std::make_shared<DxuiCommand> (*base) : nullptr;

        if (command != nullptr)
        {
            command->isEnabled = [grouped]() { return grouped; };
            items.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
        }
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    more->label    = L"More...";
    more->dispatch = [this]() { m_pane.GetColumns().ChooseDetails(); };
    items.push_back (DxuiPopupMenuItem::ForCommand (std::move (more)));

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetSearchOptionChoices
//
//  File Explorer's Search options. A date, a kind or a size goes in the query
//  as a word of the box's syntax, and choosing it again takes it out.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetSearchOptionChoices()
{
    struct Value { const wchar_t * label; const wchar_t * value; };

    static constexpr Value  s_kDates[] = { { L"Today", L"today" }, { L"Yesterday", L"yesterday" }, { L"This week", L"this week" }, { L"Last week", L"last week" },
                                           { L"This month", L"this month" }, { L"Last month", L"last month" }, { L"This year", L"this year" }, { L"Last year", L"last year" } };
    static constexpr Value  s_kKinds[] = { { L"Calendar", L"calendar" }, { L"Communication", L"communication" }, { L"Contact", L"contact" }, { L"Document", L"document" },
                                           { L"E-mail", L"email" }, { L"Feed", L"feed" }, { L"Folder", L"folder" }, { L"Game", L"game" }, { L"Instant Message", L"instantmessage" },
                                           { L"Journal", L"journal" }, { L"Link", L"link" }, { L"Movie", L"movie" }, { L"Music", L"music" }, { L"Note", L"note" }, { L"Picture", L"picture" },
                                           { L"Playlist", L"playlist" }, { L"Program", L"program" }, { L"Recorded TV", L"recordedtv" }, { L"Saved Search", L"searchfolder" }, { L"Task", L"task" },
                                           { L"Video", L"video" }, { L"Web History", L"webhistory" }, { L"Unknown", L"unknown" } };
    static constexpr Value  s_kSizes[] = { { L"Empty (0 KB)", L"empty" }, { L"Tiny (0 - 16 KB)", L"tiny" }, { L"Small (16 KB - 1 MB)", L"small" }, { L"Medium (1 - 128 MB)", L"medium" },
                                           { L"Large (128 MB - 1 GB)", L"large" }, { L"Huge (1 - 4 GB)", L"huge" }, { L"Gigantic (>4 GB)", L"gigantic" } };

    std::vector<DxuiPopupMenuItem>  items;
    ExplorerAddressBar *            bar     = &m_context.addressBar;
    bool                            indexed = IsSearchIndexed();
    auto                            option = [this] (const wchar_t * label, const wchar_t * svg, bool radio, std::function<bool()> checked, std::function<void()> dispatch)
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label     = label;
        command->menuSvg   = m_context.menus.GetMenuSvg (svg);
        command->radio     = radio;
        command->isChecked = std::move (checked);
        command->dispatch  = std::move (dispatch);

        return command;
    };
    auto                            submenu = [&] (const wchar_t * label, const wchar_t * svg, const wchar_t * prefix, const Value * values, size_t count)
    {
        std::vector<DxuiPopupMenuItem>  rows;
        std::shared_ptr<DxuiCommand>    parent = std::make_shared<DxuiCommand>();

        for (size_t at = 0; at < count; at++)
        {
            std::wstring  value = values[at].value;

            rows.push_back (DxuiPopupMenuItem::ForCommand (option (values[at].label, nullptr, true,
                [bar, prefix, value]() { return SearchQuery::HasToken (bar->GetSearchQuery(), prefix, value); },
                [bar, prefix, value]() { bar->SetSearchQuery (SearchQuery::ToggleToken (bar->GetSearchQuery(), prefix, value)); })));
        }

        //  File Explorer's filters are grayed in a folder the index doesn't cover.
        parent->label     = label;
        parent->menuSvg   = m_context.menus.GetMenuSvg (svg);
        parent->isEnabled = [indexed]() { return indexed; };
        items.push_back (DxuiPopupMenuItem::ForSubmenu (std::move (parent), std::move (rows)));
    };
    std::shared_ptr<DxuiCommand>    current = option (L"Current folder", L"windows.searchoptionshallow", true, [bar]() { return bar->GetSearchOptions().currentFolder; },
                                                      [bar]() { bar->SetSearchFolderOnly (true); });



    current->isEnabled = [bar]() { return bar->CanSearchCurrentFolderOnly(); };

    items.push_back (DxuiPopupMenuItem::ForCommand (option (L"All subfolders", L"windows.searchoptiondeep", true, [bar]() { return !bar->GetSearchOptions().currentFolder; }, [bar]() { bar->SetSearchFolderOnly (false); })));
    items.push_back (DxuiPopupMenuItem::ForCommand (std::move (current)));
    items.push_back (DxuiPopupMenuItem::ForSeparator());

    submenu (L"Date modified", L"windows.searchfilterdate", SearchQuery::kDateToken, s_kDates, std::size (s_kDates));
    submenu (L"Kind",          L"windows.searchfilterkind", SearchQuery::kKindToken, s_kKinds, std::size (s_kKinds));
    submenu (L"Size",          L"windows.searchfiltersize", SearchQuery::kSizeToken, s_kSizes, std::size (s_kSizes));

    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (option (L"System files", L"windows.searchoptionsystem", false, [this]() { return m_context.prefs.searchSystemFiles; },
        [this, bar]() { m_context.prefs.searchSystemFiles = !m_context.prefs.searchSystemFiles; bar->ApplySearchPrefs(); })));
    items.push_back (DxuiPopupMenuItem::ForCommand (option (L"Zipped (compressed) folders", L"windows.searchoptioncompressed", false, [this]() { return m_context.prefs.searchZipped; },
        [this, bar]() { m_context.prefs.searchZipped = !m_context.prefs.searchZipped; bar->ApplySearchPrefs(); })));
    items.push_back (DxuiPopupMenuItem::ForCommand (option (L"File contents", L"windows.searchoptioncontents", false, [this]() { return m_context.prefs.searchContents; },
        [this, bar]() { m_context.prefs.searchContents = !m_context.prefs.searchContents; bar->ApplySearchPrefs(); })));
    items.push_back (DxuiPopupMenuItem::ForSeparator());
    items.push_back (DxuiPopupMenuItem::ForCommand (option (L"Change indexed locations", L"windows.changeindexedlocations", false, nullptr, []()
    {
        ShellExecuteW (nullptr, L"open", L"control.exe", L"srchadmin.dll", nullptr, SW_SHOWNORMAL);
    })));

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::IsSearchIndexed
//
//  Whether the index covers the folder a search covers, or one of Casso's
//  folders for a search from its root. Home's and This PC's are indexed.
//
////////////////////////////////////////////////////////////////////////////////

bool ExplorerListMenus::IsSearchIndexed()
{
    std::wstring  scope;
    std::wstring  query;
    bool          parsed = SearchQuery::TryParseId (m_context.browser.GetLocation().path, scope, query);



    if (!parsed)
    {
        return true;
    }

    for (const std::wstring & folder : SearchQuery::SplitScopes (scope))
    {
        if (folder == SearchQuery::kThisPc || folder == SearchQuery::kEverywhere || m_context.shellVerbs.IsIndexed (folder))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::GetViewChoices
//
//  In Explorer's order. The background menu's rows have their keys, as File
//  Explorer's do, and the command bar's don't.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> ExplorerListMenus::GetViewChoices (bool withKeys)
{
    static constexpr const wchar_t *  s_kKeys[] = { L"Ctrl+Shift+1", L"Ctrl+Shift+2", L"Ctrl+Shift+3", L"Ctrl+Shift+4", L"Ctrl+Shift+5", L"Ctrl+Shift+6", L"Ctrl+Shift+7", L"Ctrl+Shift+8" };
    std::vector<DxuiPopupMenuItem>    items;
    size_t                            at        = 0;



    for (int view : { 1, 2, 3, 4, 5, 0, 6, 7 })
    {
        std::shared_ptr<const DxuiCommand>  base    = m_context.commands.Find (CassoExplorerCommands::kViewFirst + view);
        std::shared_ptr<DxuiCommand>        command = (base != nullptr && withKeys) ? std::make_shared<DxuiCommand> (*base) : nullptr;

        if (command != nullptr)
        {
            command->accelerator = s_kKeys[at];
            items.push_back (DxuiPopupMenuItem::ForCommand (std::move (command)));
        }
        else
        {
            items.push_back (DxuiPopupMenuItem::ForCommand (base));
        }

        at++;
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

    items.push_back (DxuiPopupMenuItem::ForSubmenu (view,  GetViewChoices (true)));
    items.push_back (DxuiPopupMenuItem::ForSubmenu (sort,  GetSortChoices (false)));
    items.push_back (DxuiPopupMenuItem::ForSubmenu (group, GetGroupChoices (false)));

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





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerListMenus::ShowFilterMenu
//
//  The column's groups, as Group by names them, each checked while the list
//  shows it; a check filters the list at once and leaves the menu open, as
//  File Explorer's does. Clear filter ends the column's filter.
//
////////////////////////////////////////////////////////////////////////////////

void ExplorerListMenus::ShowFilterMenu (size_t column, RECT chevron)
{
    std::vector<DxuiPopupMenuItem>  items;
    RowGrouping::Field              field  = CassoExplorerBrowser::GetFilterField (column);



    if (field == RowGrouping::Field::None)
    {
        return;
    }

    m_context.menus.GetMenuCommands().clear();

    for (const RowGrouping::Group & group : m_context.browser.GetFilterChoices (field))
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
        std::wstring                  label   = group.label;

        command->label         = ExplorerAddressBar::EscapeMnemonics (label);
        command->keepsMenuOpen = true;
        command->checkBox      = true;
        command->menuImage     = m_context.shellIcons.GetForIconResource (s_kFilterGroupIcon);
        command->isChecked     = [this, field, label]() { return m_context.browser.IsFilterChecked (field, label); };
        command->dispatch      = [this, field, label]()
        {
            HRESULT  hr = S_OK;

            m_context.browser.ToggleFilter (field, label);
            hr = m_context.browser.Reload (true);
            IGNORE_RETURN_VALUE (hr, S_OK);
            m_pane.FillList();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_context.menus.GetMenuCommands().push_back (std::move (command));
    }

    if (m_context.browser.IsFiltering (field))
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_context.menus.AddMenuCommand (items, L"&Clear filter", [this, field]()
        {
            HRESULT  hr = S_OK;

            m_context.browser.ClearFilter (field);
            hr = m_context.browser.Reload (true);
            IGNORE_RETURN_VALUE (hr, S_OK);
            m_pane.FillList();
        });
    }

    if (items.empty())
    {
        return;
    }

    DxuiContextMenu::ShowUnder (*m_context.window.GetPopupHost(), chevron, std::move (items));
}
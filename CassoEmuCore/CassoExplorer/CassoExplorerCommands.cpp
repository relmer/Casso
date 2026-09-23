#include "Pch.h"

#include "CassoExplorer/CassoExplorerIcons.h"
#include "CassoExplorer/CassoExplorerCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::CassoExplorerCommands
//
//  One command per row, each forwarding to the handlers with its own id. A
//  missing handler leaves a row enabled, unchecked and inert.
//
////////////////////////////////////////////////////////////////////////////////

CassoExplorerCommands::CassoExplorerCommands (Handlers handlers)
    : m_handlers (std::move (handlers))
{
    for (const Row & row : kRows)
    {
        std::shared_ptr<DxuiCommand>  command;
        int                           id = row.id;

        if (row.id == kSeparator)
        {
            continue;
        }

        command = std::make_shared<DxuiCommand>();

        command->id          = row.id;
        command->label       = row.label;
        command->accelerator = (row.accelerator != nullptr) ? row.accelerator : L"";

        command->dispatch = [this, id]()
        {
            if (m_handlers.dispatch)
            {
                m_handlers.dispatch (id);
            }
        };

        command->isEnabled = [this, id]()
        {
            return m_handlers.isEnabled ? m_handlers.isEnabled (id) : true;
        };

        if (row.checkable)
        {
            command->isChecked = [this, id]()
            {
                return m_handlers.isChecked ? m_handlers.isChecked (id) : false;
            };
        }

        m_commands.push_back (std::move (command));
    }

    //  The toolbar shows some of the same commands; they use the glyph,
    //  short label and tip the strip draws.
    ApplyToolbarRows (kToolbarRows);
    ApplyToolbarRows (kCommandBarRows);
    ApplyToolbarRows (kPreviewToolbarRows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::ApplyToolbarRows
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCommands::ApplyToolbarRows (std::span<const ToolbarRow> rows)
{
    for (const ToolbarRow & row : rows)
    {
        for (std::shared_ptr<DxuiCommand> & command : m_commands)
        {
            if (command->id == row.id)
            {
                command->glyph      = row.glyph;
                command->shortLabel = row.shortLabel;
                command->tip        = row.tip;
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::GetMenuTitle
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassoExplorerCommands::GetMenuTitle (Menu menu)
{
    switch (menu)
    {
        case Menu::File: return L"&File";
        case Menu::Edit: return L"&Edit";
        case Menu::View: return L"&View";
        case Menu::Go:   return L"&Go";
        case Menu::Help: return L"&Help";
        default:         return L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::GetStandardCommand
//
////////////////////////////////////////////////////////////////////////////////

DxuiStandardCommand CassoExplorerCommands::GetStandardCommand (int id)
{
    for (const Row & row : kRows)
    {
        if (row.id == id)
        {
            return row.standard;
        }
    }

    return DxuiStandardCommand::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::GetToolbarEntryCount
//
////////////////////////////////////////////////////////////////////////////////

size_t CassoExplorerCommands::GetToolbarEntryCount()
{
    return std::size (kToolbarRows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::GetToolbarCommandId
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerCommands::GetToolbarCommandId (size_t index)
{
    return (index < std::size (kToolbarRows)) ? kToolbarRows[index].id : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::Find
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DxuiCommand> CassoExplorerCommands::Find (int id) const
{
    for (const std::shared_ptr<DxuiCommand> & command : m_commands)
    {
        if (command->id == id)
        {
            return command;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::BuildMenuItems
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiMenuBarItem> CassoExplorerCommands::BuildMenuItems() const
{
    std::vector<DxuiMenuBarItem>  items;
    int                           menu = 0;



    for (menu = 0; menu < (int) Menu::Count; menu++)
    {
        DxuiMenuBarItem  item;

        item.label = GetMenuTitle ((Menu) menu);

        for (const Row & row : kRows)
        {
            if (row.menu != (Menu) menu)
            {
                continue;
            }

            if (row.id == kSeparator)
            {
                item.submenu.push_back (DxuiPopupMenuItem::ForSeparator());
            }
            else
            {
                item.submenu.push_back (DxuiPopupMenuItem::ForCommand (Find (row.id)));
            }
        }

        items.push_back (std::move (item));
    }

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::TranslateKey
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerCommands::TranslateKey (WPARAM vk, bool ctrl, bool alt, bool shift)
{
    for (const Key & key : kKeys)
    {
        if (key.vk == vk && key.ctrl == ctrl && key.alt == alt && key.shift == shift)
        {
            return key.id;
        }
    }

    return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::BuildToolbarEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> CassoExplorerCommands::BuildToolbarEntries() const
{
    return BuildEntries (kToolbarRows);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::BuildCommandBarEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> CassoExplorerCommands::BuildCommandBarEntries() const
{
    std::vector<DxuiToolbar::Entry>  entries = BuildEntries (kCommandBarRows);



    for (DxuiToolbar::Entry & entry : entries)
    {
        if (entry.command != nullptr)
        {
            entry.vectorIcon = GetCommandBarIcon (entry.command->id);
        }
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::GetCommandBarIcon
//
//  The two-tone Fluent icon File Explorer shows for the same command, or none
//  for a command Explorer has no button for, which keeps its glyph.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiVectorIcon * CassoExplorerCommands::GetCommandBarIcon (int id)
{
    switch (id)
    {
        case kNew:           return &CassoExplorerIcons::s_kNew;
        case kCutItems:      return &CassoExplorerIcons::s_kCut;
        case kCopyItems:     return &CassoExplorerIcons::s_kCopy;
        case kPasteItems:    return &CassoExplorerIcons::s_kPaste;
        case kRenameItem:    return &CassoExplorerIcons::s_kRename;
        case kDeleteItems:   return &CassoExplorerIcons::s_kDelete;
        case kSort:          return &CassoExplorerIcons::s_kSort;
        case kView:          return &CassoExplorerIcons::s_kView;
        case kTogglePreview: return &CassoExplorerIcons::s_kPreview;
        case kTheme:         return &CassoExplorerIcons::s_kTheme;
        default:             return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::BuildEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> CassoExplorerCommands::BuildEntries (std::span<const ToolbarRow> rows) const
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (const ToolbarRow & row : rows)
    {
        DxuiToolbar::Entry  entry;

        entry.command  = Find (row.id);
        entry.kind     = row.kind;
        entry.group    = row.group;
        entry.iconOnly    = row.iconOnly;
        entry.trailing    = row.trailing;
        entry.seeMoreOnly = row.seeMoreOnly;

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::BuildPreviewToolbarEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> CassoExplorerCommands::BuildPreviewToolbarEntries (bool hex, IDxuiToolbarCustomEntry * search, IDxuiToolbarCustomEntry * goTo) const
{
    std::vector<DxuiToolbar::Entry>  entries;



    for (const ToolbarRow & row : kPreviewToolbarRows)
    {
        DxuiToolbar::Entry  entry;

        if ((row.id == kLineAddresses) == hex)
        {
            continue;
        }

        entry.command  = Find (row.id);
        entry.kind     = row.kind;
        entry.group    = row.group;
        entry.trailing = row.trailing;
        entry.custom   = (row.id == kFind)       ? search
                       : (row.id == kGoToOffset) ? goTo
                                                 : nullptr;

        entries.push_back (std::move (entry));
    }

    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::GetPreviewToolbarCommandIds
//
//  The preview toolbar's commands in strip order, for the Tab order.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> CassoExplorerCommands::GetPreviewToolbarCommandIds (bool hex)
{
    std::vector<int>  ids;



    for (const ToolbarRow & row : kPreviewToolbarRows)
    {
        if ((row.id == kLineAddresses) != hex)
        {
            ids.push_back (row.id);
        }
    }

    return ids;
}

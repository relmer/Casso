#include "Pch.h"

#include "Cassque/CassqueCommands.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueCommands::CassqueCommands
//
//  One command per row, each forwarding to the handlers with its own id. A
//  missing handler leaves a row enabled, unchecked and inert.
//
////////////////////////////////////////////////////////////////////////////////

CassqueCommands::CassqueCommands (Handlers handlers)
    : m_handlers (std::move (handlers))
{
    for (const Row & row : kRows)
    {
        std::unique_ptr<DxuiCommand>  command;
        int                           id = row.id;

        if (row.id == kSeparator)
        {
            continue;
        }

        command = std::make_unique<DxuiCommand>();

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
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueCommands::GetMenuTitle
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * CassqueCommands::GetMenuTitle (Menu menu)
{
    switch (menu)
    {
        case Menu::File: return L"&File";
        case Menu::View: return L"&View";
        case Menu::Go:   return L"&Go";
        case Menu::Help: return L"&Help";
        default:         return L"";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueCommands::Find
//
////////////////////////////////////////////////////////////////////////////////

const DxuiCommand * CassqueCommands::Find (int id) const
{
    for (const std::unique_ptr<DxuiCommand> & command : m_commands)
    {
        if (command->id == id)
        {
            return command.get();
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueCommands::BuildMenuItems
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiMenuBarItem> CassqueCommands::BuildMenuItems() const
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
//  CassqueCommands::TranslateKey
//
////////////////////////////////////////////////////////////////////////////////

int CassqueCommands::TranslateKey (WPARAM vk, bool ctrl, bool alt, bool shift)
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

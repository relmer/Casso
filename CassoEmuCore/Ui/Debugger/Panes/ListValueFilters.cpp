#include "Pch.h"

#include "Ui/Debugger/Panes/ListValueFilters.h"

#include "Ui/Debugger/DebuggerMenuCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::ListValueFilters
//
////////////////////////////////////////////////////////////////////////////////

ListValueFilters::ListValueFilters (IDebuggerPaneHost & host) :
    m_host (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::Attach
//
////////////////////////////////////////////////////////////////////////////////

void ListValueFilters::Attach (DxuiListView * list, std::vector<size_t> columns, std::function<void()> onChanged)
{
    m_list      = list;
    m_columns   = std::move (columns);
    m_onChanged = std::move (onChanged);

    m_list->SetOnHeaderFilter ([this] (int column, RECT chevron)
    {
        RECT  bounds = m_list->GetBounds();



        ShowMenu ((size_t) column, RECT { bounds.left + chevron.left, bounds.top + chevron.top, bounds.left + chevron.right, bounds.top + chevron.bottom });
    });

    ShowState();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::SetValues
//
////////////////////////////////////////////////////////////////////////////////

void ListValueFilters::SetValues (size_t column, const std::vector<std::wstring> & values)
{
    m_present[column] = std::set<std::wstring> (values.begin(), values.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::IsShown
//
////////////////////////////////////////////////////////////////////////////////

bool ListValueFilters::IsShown (size_t column, const std::wstring & value) const
{
    auto  hidden = m_hidden.find (column);



    return hidden == m_hidden.end() || !hidden->second.contains (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::IsFiltered
//
////////////////////////////////////////////////////////////////////////////////

bool ListValueFilters::IsFiltered (size_t column) const
{
    auto  hidden = m_hidden.find (column);



    return hidden != m_hidden.end() && !hidden->second.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::IsAnyFiltered
//
////////////////////////////////////////////////////////////////////////////////

bool ListValueFilters::IsAnyFiltered() const
{
    return std::ranges::any_of (m_columns, [this] (size_t column) { return IsFiltered (column); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::Toggle
//
////////////////////////////////////////////////////////////////////////////////

void ListValueFilters::Toggle (size_t column, const std::wstring & value)
{
    std::set<std::wstring>  & hidden = m_hidden[column];



    if (hidden.erase (value) == 0)
    {
        hidden.insert (value);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::Clear
//
////////////////////////////////////////////////////////////////////////////////

void ListValueFilters::Clear (size_t column)
{
    m_hidden.erase (column);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::ShowState
//
////////////////////////////////////////////////////////////////////////////////

void ListValueFilters::ShowState()
{
    for (size_t column : m_columns)
    {
        if (m_list != nullptr && column < m_list->GetColumnCount())
        {
            m_list->SetColumnFilter (column, true, IsFiltered (column));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::GetMenuValues
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ListValueFilters::GetMenuValues (size_t column) const
{
    auto  present = m_present.find (column);
    auto  hidden  = m_hidden.find (column);



    return MergeValues (present != m_present.end() ? present->second : std::set<std::wstring>(),
                        hidden  != m_hidden.end()  ? hidden->second  : std::set<std::wstring>());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::MergeValues
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> ListValueFilters::MergeValues (const std::set<std::wstring> & present, const std::set<std::wstring> & hidden)
{
    std::set<std::wstring>     all = present;
    std::vector<std::wstring>  values;



    all.insert (hidden.begin(), hidden.end());
    values.assign (all.begin(), all.end());

    std::ranges::stable_sort (values, [] (const std::wstring & left, const std::wstring & right) { return _wcsicmp (left.c_str(), right.c_str()) < 0; });
    return values;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::GetLabel
//
//  The menu's text for a value; a row with nothing in the column is listed
//  as blank.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ListValueFilters::GetLabel (const std::wstring & value)
{
    return value.empty() ? std::wstring (L"(Blank)") : value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters::ShowMenu
//
////////////////////////////////////////////////////////////////////////////////

void ListValueFilters::ShowMenu (size_t column, RECT chevronPx)
{
    std::vector<DxuiPopupMenuItem>  items;



    if (m_host.GetPanePopupHost() == nullptr || std::ranges::find (m_columns, column) == m_columns.end())
    {
        return;
    }

    m_commands.clear();

    for (const std::wstring & value : GetMenuValues (column))
    {
        std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();

        command->label         = GetLabel (value);
        command->keepsMenuOpen = true;
        command->checkBox      = true;
        command->isChecked     = [this, column, value]() { return IsShown (column, value); };
        command->dispatch      = [this, column, value]()
        {
            Toggle (column, value);
            ShowState();
            m_onChanged();
            m_host.Repaint();
        };

        items.push_back (DxuiPopupMenuItem::ForCommand (command));
        m_commands.push_back (std::move (command));
    }

    if (IsFiltered (column))
    {
        items.push_back (DxuiPopupMenuItem::ForSeparator());
        m_commands.push_back (DebuggerMenuCommand::Make (L"Clear filter", [this, column]
        {
            Clear (column);
            ShowState();
            m_onChanged();
            m_host.Repaint();
        }));
        items.push_back (DxuiPopupMenuItem::ForCommand (m_commands.back()));
    }

    if (!items.empty())
    {
        DxuiContextMenu::ShowUnder (*m_host.GetPanePopupHost(), chevronPx, std::move (items));
    }
}

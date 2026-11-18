#pragma once

#include "Ui/Debugger/Panes/IDebuggerPane.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ListValueFilters
//
//  The filters of a list's columns of values. A chevron on a column's heading
//  opens a menu of the values the column has, each checked while its rows are
//  listed; a check filters at once and the menu stays open, as File
//  Explorer's does. A heading with a hidden value has the funnel. Each
//  column's values are set by the pane before its rows are built, and a row
//  whose value is hidden is not listed. The filters last as long as the
//  window.
//
////////////////////////////////////////////////////////////////////////////////

class ListValueFilters
{
public:
    explicit ListValueFilters (IDebuggerPaneHost & host);

    //  The list, the columns that filter, and the pane's call to build its
    //  rows again after a change.
    void  Attach        (DxuiListView * list, std::vector<size_t> columns, std::function<void()> onChanged);

    //  The values a column's rows have, before any is left out.
    void  SetValues     (size_t column, const std::vector<std::wstring> & values);

    bool  IsShown       (size_t column, const std::wstring & value) const;
    bool  IsFiltered    (size_t column) const;
    bool  IsAnyFiltered () const;
    void  Toggle        (size_t column, const std::wstring & value);
    void  Clear         (size_t column);

    //  A chevron on each heading that filters, and the funnel on each with a
    //  hidden value. A list's SetColumns ends both, so call this after it.
    void  ShowState     ();

    //  The menu's values for a column: those its rows have and those hidden,
    //  each once, in order ignoring case.
    std::vector<std::wstring>  GetMenuValues (size_t column) const;

    static std::vector<std::wstring>  MergeValues (const std::set<std::wstring> & present, const std::set<std::wstring> & hidden);
    static std::wstring               GetLabel    (const std::wstring & value);

private:
    void  ShowMenu (size_t column, RECT chevronPx);

    IDebuggerPaneHost                          & m_host;
    DxuiListView                               * m_list = nullptr;
    std::vector<size_t>                          m_columns;
    std::map<size_t, std::set<std::wstring>>     m_present;
    std::map<size_t, std::set<std::wstring>>     m_hidden;
    std::function<void()>                        m_onChanged;
    std::vector<std::shared_ptr<DxuiCommand>>    m_commands;   // the open menu's rows
};

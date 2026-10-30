#include "Pch.h"

#include "CassoExplorer/Model/ChooseDetailsModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModel::Build
//
//  A column that doesn't apply to the folder on display isn't listed, as
//  in File Explorer.
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsModel::Build (const std::vector<Column>                         & columns,
                                const std::vector<size_t>                         & order,
                                const std::vector<std::wstring>                   & columnNames,
                                const std::vector<Win32PropertyColumns::Property> & properties)
{
    m_columns = columns;
    m_entries.clear();

    for (size_t column : order)
    {
        Entry  entry;

        if (column >= columns.size() || !columns[column].offered)
        {
            continue;
        }

        entry.title    = columns[column].title;
        entry.column   = (int) column;
        entry.checked  = columns[column].chosen || columns[column].fixed;
        entry.fixed    = columns[column].fixed;
        entry.widthDip = columns[column].widthDip;

        m_entries.push_back (std::move (entry));
    }

    for (const Win32PropertyColumns::Property & property : properties)
    {
        Entry  entry;

        if (std::find (columnNames.begin(), columnNames.end(), property.canonicalName) != columnNames.end())
        {
            continue;
        }

        entry.title         = property.title;
        entry.canonicalName = property.canonicalName;
        entry.widthDip      = property.widthDip;

        m_entries.push_back (std::move (entry));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModel::Toggle
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsModel::Toggle (size_t entry)
{
    if (entry < m_entries.size() && !m_entries[entry].fixed)
    {
        m_entries[entry].checked = !m_entries[entry].checked;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModel::CanMoveUp
//
//  Name stays first, as in File Explorer.
//
////////////////////////////////////////////////////////////////////////////////

bool ChooseDetailsModel::CanMoveUp (size_t entry) const
{
    return entry > 0 && entry < m_entries.size() && !m_entries[entry].fixed && !m_entries[entry - 1].fixed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModel::CanMoveDown
//
////////////////////////////////////////////////////////////////////////////////

bool ChooseDetailsModel::CanMoveDown (size_t entry) const
{
    return entry + 1 < m_entries.size() && CanMoveUp (entry + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModel::MoveUp
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsModel::MoveUp (size_t entry)
{
    if (CanMoveUp (entry))
    {
        std::swap (m_entries[entry], m_entries[entry - 1]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModel::MoveDown
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsModel::MoveDown (size_t entry)
{
    if (CanMoveDown (entry))
    {
        std::swap (m_entries[entry], m_entries[entry + 1]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModel::SetWidth
//
////////////////////////////////////////////////////////////////////////////////

void ChooseDetailsModel::SetWidth (size_t entry, int widthDip)
{
    if (entry < m_entries.size() && widthDip > 0)
    {
        m_entries[entry].widthDip = widthDip;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModel::GetResult
//
//  A checked property becomes a column after the last. A column not listed,
//  as a catalog's inside a host folder, goes after the listed ones, and its
//  choice is copied from the column.
//
////////////////////////////////////////////////////////////////////////////////

ChooseDetailsModel::Result ChooseDetailsModel::GetResult() const
{
    Result  result;
    int     next   = (int) m_columns.size();



    for (const Column & column : m_columns)
    {
        result.chosen.push_back (column.chosen);
    }

    for (const Entry & entry : m_entries)
    {
        int  column = entry.column;

        if (column < 0 && !entry.checked)
        {
            continue;
        }

        if (column < 0)
        {
            column = next++;
            result.added.push_back (entry.canonicalName);
            result.chosen.push_back (true);
            result.widths.emplace_back (column, entry.widthDip);
        }
        else
        {
            result.chosen[(size_t) column] = entry.checked;

            if (entry.widthDip != m_columns[(size_t) column].widthDip)
            {
                result.widths.emplace_back (column, entry.widthDip);
            }
        }

        result.order.push_back (column);
    }

    for (int column = 0; column < (int) m_columns.size(); column++)
    {
        if (std::find (result.order.begin(), result.order.end(), column) == result.order.end())
        {
            result.order.push_back (column);
        }
    }

    return result;
}

#pragma once

#include "Pch.h"

#include "Seams/Win32PropertyColumns.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModel
//
//  File Explorer's Choose Details list: the list's columns in their order
//  on screen, each checked when visible, then every other property the
//  shell offers, by title. An entry is checked, moved and given a width
//  here, and the result is what the list applies on OK: the properties
//  added as columns, the order, which columns are visible, and the widths
//  changed.
//
////////////////////////////////////////////////////////////////////////////////

class ChooseDetailsModel
{
public:
    //  A column of the list as it is now.
    struct Column
    {
        std::wstring  title;
        bool          offered  = true;    // applies to the folder on display
        bool          chosen   = true;
        bool          fixed    = false;   // Name, which is always visible
        int           widthDip = 0;
    };

    struct Entry
    {
        std::wstring  title;
        std::wstring  canonicalName;      // a property not yet a column
        int           column   = -1;      // the list's column, or -1
        bool          checked  = false;
        bool          fixed    = false;
        int           widthDip = 0;
    };

    struct Result
    {
        std::vector<std::wstring>         added;    // properties to add as columns, in order
        std::vector<int>                  order;    // every column, the added ones too
        std::vector<bool>                 chosen;   // by column
        std::vector<std::pair<int, int>>  widths;   // a column and its new width in DIPs
    };

    //  `columns` by index, `order` as on screen, and the shell's
    //  properties; a property already a column, by `columnNames`, isn't
    //  listed twice.
    void  Build (const std::vector<Column>                         & columns,
                 const std::vector<size_t>                         & order,
                 const std::vector<std::wstring>                   & columnNames,
                 const std::vector<Win32PropertyColumns::Property> & properties);

    const std::vector<Entry> &  GetEntries () const { return m_entries; }

    void  Toggle      (size_t entry);
    bool  CanMoveUp   (size_t entry) const;
    bool  CanMoveDown (size_t entry) const;
    void  MoveUp      (size_t entry);
    void  MoveDown    (size_t entry);
    void  SetWidth    (size_t entry, int widthDip);

    Result  GetResult () const;

private:
    std::vector<Entry>   m_entries;
    std::vector<Column>  m_columns;
};

#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/RowGrouping.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GroupChoices
//
//  The rows of File Explorer's Group by menu for each type of folder, as read
//  from its menus, in its order. A row is a fixed column or a shell property.
//
////////////////////////////////////////////////////////////////////////////////

class GroupChoices
{
public:
    //  Results is Disk list's and Find duplicates'.
    enum class Folder { Generic, Documents, OneDrive, Search, Drives, Pictures, Music, Videos, RecycleBin, Home, Gallery, Network, Zip, Results };

    struct Choice
    {
        const wchar_t       * label    = nullptr;
        RowGrouping::Field    field    = RowGrouping::Field::None;
        const wchar_t       * property = nullptr;   // a canonical name, when not a fixed column
    };

    static std::vector<Choice>  GetChoices (Folder folder);
};

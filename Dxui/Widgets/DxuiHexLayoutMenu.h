#pragma once

#include "Pch.h"
#include "DxuiHexView.h"
#include "DxuiPopupMenu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayoutMenu
//
//  A hex view's layout choices as one block of context menu rows: text only,
//  1-, 2- or 4-byte values, hexadecimal, signed or unsigned, and a Columns
//  submenu. Every host of a hex view offers the same rows from here, so their
//  labels and checks cannot drift apart.
//
//  THE HOST APPLIES A CHOICE. The menu reads the view for its checks, and a
//  choice goes back to the host, which applies GetChange to the view and to
//  whatever it keeps alongside it: its preferences, its toolbar's labels.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexLayoutMenu
{
public:
    enum class Choice
    {
        TextOnly,
        Group1,
        Group2,
        Group4,
        Hex,
        Signed,
        Unsigned,
        ColumnsAuto,
        Columns1,
        Columns2,
        Columns4,
        Columns8,
        Columns16,
    };

    //  What a choice changes; a part left empty stays as it is.
    struct Change
    {
        std::optional<bool>                      showValues;
        std::optional<int>                       grouping;
        std::optional<DxuiHexView::ValueFormat>  format;
        std::optional<int>                       columns;
    };

    using ChooseFn = std::function<void (Choice choice)>;

    //  The rows, in menu order with their separators, checked as the view
    //  stands each time the menu paints. The view must outlive the menu.
    static std::vector<DxuiPopupMenuItem>  BuildItems (const DxuiHexView & view, ChooseFn choose);

    static const wchar_t *  GetLabel  (Choice choice);
    static bool             IsChecked (Choice choice, const DxuiHexView & view);

    //  Choosing a grouping or a format shows the values it applies to;
    //  text only turns the values off, or back on when they are off.
    static Change           GetChange (Choice choice, const DxuiHexView & view);

    //  The change made to the view itself, for a host that keeps nothing else.
    static void             ApplyTo   (Choice choice, DxuiHexView & view);

private:
    static std::shared_ptr<DxuiCommand>  MakeCommand    (Choice choice, const DxuiHexView & view, const ChooseFn & choose);
    static int                           GetColumnCount (Choice choice);
};

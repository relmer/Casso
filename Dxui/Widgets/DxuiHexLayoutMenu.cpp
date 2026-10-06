#include "Pch.h"

#include "DxuiHexLayoutMenu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayoutMenu::BuildItems
//
//  Text only and the three groupings, then the three formats, then Columns.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPopupMenuItem> DxuiHexLayoutMenu::BuildItems (const DxuiHexView & view, ChooseFn choose)
{
    static constexpr Choice  kColumnChoices[] = { Choice::ColumnsAuto, Choice::Columns1, Choice::Columns2,
                                                  Choice::Columns4,    Choice::Columns8, Choice::Columns16 };
    std::vector<DxuiPopupMenuItem>  items;
    std::vector<DxuiPopupMenuItem>  columns;
    std::shared_ptr<DxuiCommand>    submenu = std::make_shared<DxuiCommand>();



    for (Choice choice : { Choice::TextOnly, Choice::Group1, Choice::Group2, Choice::Group4 })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (MakeCommand (choice, view, choose)));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (Choice choice : { Choice::Hex, Choice::Signed, Choice::Unsigned })
    {
        items.push_back (DxuiPopupMenuItem::ForCommand (MakeCommand (choice, view, choose)));
    }

    items.push_back (DxuiPopupMenuItem::ForSeparator());

    for (Choice choice : kColumnChoices)
    {
        columns.push_back (DxuiPopupMenuItem::ForCommand (MakeCommand (choice, view, choose)));
    }

    submenu->label = L"Columns";
    items.push_back (DxuiPopupMenuItem::ForSubmenu (submenu, std::move (columns)));

    return items;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayoutMenu::MakeCommand
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<DxuiCommand> DxuiHexLayoutMenu::MakeCommand (Choice choice, const DxuiHexView & view, const ChooseFn & choose)
{
    std::shared_ptr<DxuiCommand>  command = std::make_shared<DxuiCommand>();
    const DxuiHexView           * at      = &view;



    command->label     = GetLabel (choice);
    command->isChecked = [choice, at] { return IsChecked (choice, *at); };
    command->dispatch  = [choice, choose]
    {
        if (choose)
        {
            choose (choice);
        }
    };

    return command;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayoutMenu::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * DxuiHexLayoutMenu::GetLabel (Choice choice)
{
    switch (choice)
    {
    case Choice::TextOnly:    return L"Show &text only";
    case Choice::Group1:      return L"&1-byte integer";
    case Choice::Group2:      return L"&2-byte integer";
    case Choice::Group4:      return L"&4-byte integer";
    case Choice::Hex:         return L"&Hexadecimal";
    case Choice::Signed:      return L"&Signed";
    case Choice::Unsigned:    return L"&Unsigned";
    case Choice::ColumnsAuto: return L"&Auto";
    case Choice::Columns1:    return L"&1";
    case Choice::Columns2:    return L"&2";
    case Choice::Columns4:    return L"&4";
    case Choice::Columns8:    return L"&8";
    case Choice::Columns16:   return L"1&6";
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayoutMenu::GetColumnCount
//
//  Values a row for a Columns choice, 0 for Auto, or -1 for any other
//  choice.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiHexLayoutMenu::GetColumnCount (Choice choice)
{
    static constexpr int  kTwo     = 2;
    static constexpr int  kFour    = 4;
    static constexpr int  kEight   = 8;
    static constexpr int  kSixteen = 16;



    switch (choice)
    {
    case Choice::ColumnsAuto: return 0;
    case Choice::Columns1:    return 1;
    case Choice::Columns2:    return kTwo;
    case Choice::Columns4:    return kFour;
    case Choice::Columns8:    return kEight;
    case Choice::Columns16:   return kSixteen;
    default:                  return -1;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayoutMenu::IsChecked
//
//  A grouping is checked only while values show, since text only shows none.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiHexLayoutMenu::IsChecked (Choice choice, const DxuiHexView & view)
{
    static constexpr int  kTwo  = 2;
    static constexpr int  kFour = 4;
    bool                  shows = view.IsShowingValues();
    int                   count = GetColumnCount (choice);



    if (count >= 0)
    {
        return view.GetColumns() == count;
    }

    switch (choice)
    {
    case Choice::TextOnly: return !shows;
    case Choice::Group1:   return shows && view.GetGrouping() == 1;
    case Choice::Group2:   return shows && view.GetGrouping() == kTwo;
    case Choice::Group4:   return shows && view.GetGrouping() == kFour;
    case Choice::Hex:      return view.GetValueFormat() == DxuiHexView::ValueFormat::Hex;
    case Choice::Signed:   return view.GetValueFormat() == DxuiHexView::ValueFormat::Signed;
    case Choice::Unsigned: return view.GetValueFormat() == DxuiHexView::ValueFormat::Unsigned;
    default:               return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayoutMenu::GetChange
//
////////////////////////////////////////////////////////////////////////////////

DxuiHexLayoutMenu::Change DxuiHexLayoutMenu::GetChange (Choice choice, const DxuiHexView & view)
{
    static constexpr int  kTwo   = 2;
    static constexpr int  kFour  = 4;
    Change                change;
    int                   count  = GetColumnCount (choice);



    if (count >= 0)
    {
        change.columns = count;
        return change;
    }

    switch (choice)
    {
    case Choice::TextOnly: change.showValues = !view.IsShowingValues();                                   break;
    case Choice::Group1:   change.showValues = true;  change.grouping = 1;                                break;
    case Choice::Group2:   change.showValues = true;  change.grouping = kTwo;                             break;
    case Choice::Group4:   change.showValues = true;  change.grouping = kFour;                            break;
    case Choice::Hex:      change.showValues = true;  change.format   = DxuiHexView::ValueFormat::Hex;      break;
    case Choice::Signed:   change.showValues = true;  change.format   = DxuiHexView::ValueFormat::Signed;   break;
    case Choice::Unsigned: change.showValues = true;  change.format   = DxuiHexView::ValueFormat::Unsigned; break;
    default:                                                                                              break;
    }

    return change;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexLayoutMenu::ApplyTo
//
////////////////////////////////////////////////////////////////////////////////

void DxuiHexLayoutMenu::ApplyTo (Choice choice, DxuiHexView & view)
{
    Change  change = GetChange (choice, view);



    if (change.grouping.has_value())
    {
        (void) view.SetGrouping (*change.grouping);
    }

    if (change.format.has_value())
    {
        view.SetValueFormat (*change.format);
    }

    if (change.columns.has_value())
    {
        view.SetColumns (*change.columns);
    }

    if (change.showValues.has_value())
    {
        view.SetShowValues (*change.showValues);
    }
}

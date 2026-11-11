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
        command->vectorIcon  = GetMenuIcon (row.id);

        //  A choice of one among several -- a sort, a view, a theme -- is marked
        //  with a dot, as Explorer marks it.
        command->radio       = (row.id >= kSortByColumn) || (row.id == kSortByRelevance) || (row.id >= kSortAscending && row.id <= kSortDescending)
                            || (row.id >= kGroupAscending && row.id <= kGroupDescending)
                            || (row.id >= kThemeLight && row.id <= kThemeRetroTerminal)
                            || (row.id >= kPictureMonochrome && row.id <= kPictureColor)
                            || (row.id >= kCharsetAppleText && row.id <= kCharsetIso8859)
                            || row.id == kSheetValues || row.id == kSheetFormulas
                            || row.id == kShowDisassembly || row.id == kShowBytes
                            || (row.id >= kViewAsDefault && row.id <= kViewAsResourceFork);

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

        if (id == kRestoreItems || id == kViewAsDefault)
        {
            command->labelText = [this, id, label = command->label]()
            {
                std::wstring  text = m_handlers.getLabel ? m_handlers.getLabel (id) : std::wstring();

                return text.empty() ? label : text;
            };
        }

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
    ApplyToolbarRows (kRecycleBinRows);
    ApplyToolbarRows (kSearchRows);
    ApplyToolbarRows (kResultsRows);
    ApplyToolbarRows (kArchiveRows);
    ApplyToolbarRows (kAppleDriveRows);
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

                //  A command only See more lists is a menu row, which has its
                //  icon beside it, as File Explorer's See more rows do.
                command->menuGlyph  = row.seeMoreOnly ? row.glyph : command->menuGlyph;
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
//  CassoExplorerCommands::SetMenuSvg
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCommands::SetMenuSvg (int id, const std::string * svg)
{
    for (std::shared_ptr<DxuiCommand> & command : m_commands)
    {
        if (command->id == id)
        {
            command->menuSvg = svg;
        }
    }
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
//  CassoExplorerCommands::SetSvg
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCommands::SetSvg (int id, const std::string * svg)
{
    for (const std::shared_ptr<DxuiCommand> & command : m_commands)
    {
        if (command->id == id)
        {
            command->menuSvg = svg;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::SetTip
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCommands::SetTip (int id, const std::wstring & tip)
{
    for (const std::shared_ptr<DxuiCommand> & command : m_commands)
    {
        if (command->id == id)
        {
            command->tip = tip;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::SetGlyph
//
//  The glyph a command's button draws, as Refresh draws Stop while a search
//  runs. The glyph must outlive the command.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerCommands::SetGlyph (int id, const wchar_t * glyph)
{
    for (const std::shared_ptr<DxuiCommand> & command : m_commands)
    {
        if (command->id == id)
        {
            command->glyph = glyph;
        }
    }
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

std::vector<DxuiToolbar::Entry> CassoExplorerCommands::BuildCommandBarEntries (CommandBarExtra extra) const
{
    std::vector<DxuiToolbar::Entry>  entries = BuildEntries (kCommandBarRows);
    std::vector<DxuiToolbar::Entry>  added;
    auto                             after   = entries.end();



    switch (extra)
    {
        case CommandBarExtra::RecycleBin: added = BuildEntries (kRecycleBinRows); break;
        case CommandBarExtra::Archive:    added = BuildEntries (kArchiveRows);    break;
        case CommandBarExtra::AppleDrive: added = BuildEntries (kAppleDriveRows); break;
        case CommandBarExtra::Search:     added = BuildEntries (kSearchRows);     break;
        case CommandBarExtra::Results:    added = BuildEntries (kResultsRows);    break;
        case CommandBarExtra::DiskImage:     added = BuildEntries (kDiskImageRows);     break;
        case CommandBarExtra::DiskImagePair: added = BuildEntries (kDiskImagePairRows); break;
        default:                                                                  break;
    }

    //  The location's buttons follow View, ahead of what lives in See more.
    if (!added.empty())
    {
        after = std::find_if (entries.begin(), entries.end(), [] (const DxuiToolbar::Entry & entry)
        {
            return entry.command != nullptr && entry.command->id == kView;
        });

        after = (after == entries.end()) ? after : after + 1;
        entries.insert (after, added.begin(), added.end());
    }

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
        case kShareItems:    return &CassoExplorerIcons::s_kShare;
        case kDeleteItems:   return &CassoExplorerIcons::s_kDelete;
        case kSort:          return &CassoExplorerIcons::s_kSort;
        case kView:          return &CassoExplorerIcons::s_kView;
        case kTogglePreview: return &CassoExplorerIcons::s_kPreview;
        case kTheme:         return &CassoExplorerIcons::s_kTheme;
        case kInspectDiskImage:  return &CassoExplorerIcons::s_kInspect;
        case kCompareDiskImages: return &CassoExplorerIcons::s_kCompare;
        default:             return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::GetMenuIcon
//
//  The View drop-down's rows have the icons Explorer's own View menu has.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiVectorIcon * CassoExplorerCommands::GetMenuIcon (int id)
{
    switch (id - kViewFirst)
    {
        case (int) DxuiListView::View::Details:              return &CassoExplorerIcons::s_kViewDetails;
        case (int) DxuiListView::View::ExtraLargeIcons:      return &CassoExplorerIcons::s_kViewExtraLargeIcons;
        case (int) DxuiListView::View::LargeIcons:           return &CassoExplorerIcons::s_kViewLargeIcons;
        case (int) DxuiListView::View::MediumIcons:          return &CassoExplorerIcons::s_kViewMediumIcons;
        case (int) DxuiListView::View::SmallIcons:           return &CassoExplorerIcons::s_kViewSmallIcons;
        case (int) DxuiListView::View::List:                 return &CassoExplorerIcons::s_kViewList;
        case (int) DxuiListView::View::Tiles:                return &CassoExplorerIcons::s_kViewTiles;
        case (int) DxuiListView::View::Content:              return &CassoExplorerIcons::s_kViewContent;
        default:                                             return nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::GetViewButtonIcon
//
////////////////////////////////////////////////////////////////////////////////

const DxuiVectorIcon * CassoExplorerCommands::GetViewButtonIcon (DxuiListView::View view)
{
    //  Details keeps the bars drawn to Explorer's button, closer than the menu's.
    return (view == DxuiListView::View::Details) ? &CassoExplorerIcons::s_kView : GetMenuIcon (kViewFirst + (int) view);
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

std::vector<DxuiToolbar::Entry> CassoExplorerCommands::BuildPreviewToolbarEntries (int bar, bool expanded, IDxuiToolbarCustomEntry * search, IDxuiToolbarCustomEntry * goTo) const
{
    std::vector<DxuiToolbar::Entry>  entries;
    std::vector<int>                 ids = GetPreviewToolbarCommandIds (bar, expanded);



    for (const ToolbarRow & row : kPreviewToolbarRows)
    {
        DxuiToolbar::Entry  entry;

        if (std::find (ids.begin(), ids.end(), row.id) == ids.end())
        {
            continue;
        }

        entry.command    = Find (row.id);
        entry.kind       = row.kind;
        entry.group      = row.group;
        entry.trailing   = row.trailing;
        entry.iconOnly   = row.iconOnly;
        entry.vectorIcon = GetCommandBarIcon (row.id);

        //  The two forms, Expand and View as stay on the strip at any width.
        //  The forms' faces are code and the bytes it assembles to.
        entry.pinned   = row.id == kShowDisassembly || row.id == kShowBytes || row.id == kExpandPreview || row.id == kViewAs;
        entry.faceText = (row.id == kShowDisassembly) ? s_kpszDisassemblyFace
                       : (row.id == kShowBytes)       ? s_kpszBytesFace
                                                      : nullptr;
        entry.custom     = (row.id == kFind)       ? search
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

std::vector<int> CassoExplorerCommands::GetPreviewToolbarCommandIds (int bar, bool expanded)
{
    std::vector<int>  ids;



    for (const ToolbarRow & row : kPreviewToolbarRows)
    {
        bool  isStep = row.id == kPreviousFile || row.id == kNextFile;

        if (IsOnPreviewBar (row.id, bar) && (expanded || !isStep))
        {
            ids.push_back (row.id);
        }
    }

    return ids;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::GetPreviewBar
//
//  The preview toolbar a command of it is on.
//
////////////////////////////////////////////////////////////////////////////////

int CassoExplorerCommands::GetPreviewBar (int id)
{
    return (id == kLineAddresses || id == kCrossReference)                         ? kPreviewBarListing
         : (id == kPictureMonochrome || id == kPictureColor || id == kSavePicture) ? kPreviewBarPicture
         : (id == kEditWozMetadata)                                                ? kPreviewBarWoz
         : (id >= kCharsetAppleText && id <= kCharsetIso8859)                      ? kPreviewBarText
         : (id == kRecordLength)                                                   ? kPreviewBarRecords
         : (id == kSheetValues || id == kSheetFormulas)                            ? kPreviewBarSheet
                                                                                   : kPreviewBarHex;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerCommands::IsOnPreviewBar
//
//  Save as PNG is on both picture bars, Search on the variables' bar as
//  well as the hex view's, and the character sets on a random-access text
//  file's bar as well as the text bar. Expand, View as, Copy, Save as,
//  previous and next are on every bar.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerCommands::IsOnPreviewBar (int id, int bar)
{
    if (id == kExpandPreview || id == kPreviousFile || id == kNextFile || id == kViewAs || id == kCopyPreview || id == kSavePreviewAs)
    {
        return true;
    }

    return GetPreviewBar (id) == bar || (id == kSavePicture && bar == kPreviewBarSave) || (id == kFind && bar == kPreviewBarVariables)
        || (GetPreviewBar (id) == kPreviewBarText && bar == kPreviewBarRecords);
}

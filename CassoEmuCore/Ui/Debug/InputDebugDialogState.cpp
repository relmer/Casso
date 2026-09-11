#include "Pch.h"

#include "InputDebugDialogState.h"



static const wchar_t * const  s_kpszInputColumnHeaders[kInputColumnCount] =
{
    L"Wall",
    L"Uptime",
    L"Cycle",
    L"Source",
    L"Address",
    L"Value",
    L"Meaning",
};

static const int              s_kInputColumnDefaultWidths[kInputColumnCount] =
{
    kInputColWallWidth,
    kInputColUptimeWidth,
    kInputColCycleWidth,
    kInputColSourceWidth,
    kInputColAddressWidth,
    kInputColValueWidth,
    kInputColMeaningWidth,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SeedDefaultColumns
//
////////////////////////////////////////////////////////////////////////////////

void SeedDefaultColumns (std::array<InputLogicalColumn, kInputColumnCount> & columns) noexcept
{
    int  i = 0;



    for (i = 0; i < kInputColumnCount; i++)
    {
        columns[i].id           = i;
        columns[i].headerText   = s_kpszInputColumnHeaders[i];
        columns[i].defaultWidth = s_kInputColumnDefaultWidths[i];
        columns[i].savedWidth   = s_kInputColumnDefaultWidths[i];
        columns[i].visible      = true;
        columns[i].autoSizedYet = false;
        columns[i].userResized  = false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchesGamePort
//
//  The game-port half of the filter. Host and Guest rows apply the SAME
//  rule and differ only in which keyboard toggle governs a non-game-port
//  event, so that toggle is the parameter rather than a duplicated switch.
//
////////////////////////////////////////////////////////////////////////////////

static bool MatchesGamePort (
    InputGamePortClass        gamePort,
    bool                      showKeyboard,
    const InputFilterState &  f) noexcept
{
    bool  shown = true;



    switch (gamePort)
    {
        case InputGamePortClass::None:
            shown = showKeyboard;
            break;

        case InputGamePortClass::Global:
            // A whole-port event (the PTRIG strobe) belongs to whichever
            // device the user is watching, so either toggle reveals it.
            shown = f.showJoystick || f.showPaddle;
            break;

        case InputGamePortClass::Pair0:
        case InputGamePortClass::Pair1:
            // An axis pair is a joystick or a pair of paddles depending on how
            // the user mapped it; the row follows that mapping's toggle.
            shown = f.pairIsJoystick[(gamePort == InputGamePortClass::Pair0) ? 0 : 1]
                        ? f.showJoystick
                        : f.showPaddle;
            break;
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MatchesFilter
//
////////////////////////////////////////////////////////////////////////////////

bool MatchesFilter (const InputEventDisplay & e, const InputFilterState & f) noexcept
{
    bool  shown = true;



    switch (e.category)
    {
        case InputEventCategory::Host:
            shown = MatchesGamePort (e.gamePort, f.showHostKeyboard, f);
            break;

        case InputEventCategory::Guest:
            shown = MatchesGamePort (e.gamePort, f.showEmuKeyboard, f);
            break;

        case InputEventCategory::System:
            // System rows have no toggle of their own and are always shown.
            break;
    }

    return shown;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendColumnText
//
////////////////////////////////////////////////////////////////////////////////

void AppendColumnText (std::wstring & out, const InputEventDisplay & e, int logicalId)
{
    switch (logicalId)
    {
        case 0: out.append (e.wallStr.data());   break;
        case 1: out.append (e.uptimeStr.data()); break;
        case 2: out.append (e.cycleStr.data());  break;
        case 3: out.append (e.source);           break;
        case 4: out.append (e.address);          break;
        case 5: out.append (e.value);            break;
        case 6: out.append (e.meaning);          break;
        default: break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildClipboardText
//
//  Renders the selected event rows as tab-separated text for the clipboard.
//
//  TABS, so a paste into a spreadsheet or a Markdown table lands in columns.
//  Aligning with spaces would look right only in a monospace font and would
//  make the data unparseable.
//
//  Only VISIBLE columns are emitted, matching what the user is looking at. A
//  copy that included hidden columns would paste data they deliberately
//  filtered away.
//
//  Rows end CRLF rather than LF, because Windows clipboard consumers --
//  Notepad included -- expect it, and a bug report pasted as one long line is
//  useless.
//
//  The first-column flag emits separators BETWEEN fields rather than after
//  each, so no row carries a trailing tab into an empty final cell.
//
//  Null rows are skipped defensively; the caller builds this list from indices
//  that could have been invalidated by a trim.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring BuildClipboardText (
    const std::vector<const InputEventDisplay *> &             selected,
    const std::array<InputLogicalColumn, kInputColumnCount> &  columns)
{
    std::wstring  out;
    int           colIdx      = 0;
    bool          firstColumn = true;



    for (const InputEventDisplay * row : selected)
    {
        if (row == nullptr)
        {
            continue;
        }

        firstColumn = true;

        for (colIdx = 0; colIdx < kInputColumnCount; colIdx++)
        {
            if (!columns[colIdx].visible)
            {
                continue;
            }

            if (!firstColumn)
            {
                out.push_back (L'\t');
            }

            AppendColumnText (out, *row, columns[colIdx].id);
            firstColumn = false;
        }

        out.append (L"\r\n");
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlanVisibleColumns
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiListView::Column> PlanVisibleColumns (
    const std::array<InputLogicalColumn, kInputColumnCount> & model) noexcept
{
    std::vector<DxuiListView::Column>  out;
    int                                i   = 0;



    out.reserve (kInputColumnCount);

    for (i = 0; i < kInputColumnCount; i++)
    {
        DxuiListView::Column  spec = {};

        spec.title   = model[i].headerText;
        // Auto-fit a column to its content until the user drag-resizes
        // it; once resized, the user's width wins and auto-fit stops
        // touching it (widthDip > 0 overrides the auto path).
        spec.widthDip = model[i].userResized ? model[i].savedWidth : 0;
        spec.visible = model[i].visible;
        spec.stretch = (model[i].id == kInputMeaningColumnId);

        out.push_back (std::move (spec));
    }

    return out;
}

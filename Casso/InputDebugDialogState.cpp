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
//  MatchesFilter
//
////////////////////////////////////////////////////////////////////////////////

bool MatchesFilter (const InputEventDisplay & e, const InputFilterState & f) noexcept
{
    bool  pairIsJoystick = true;

    switch (e.category)
    {
        case InputEventCategory::Host:
            switch (e.gamePort)
            {
                case InputGamePortClass::None:
                    return f.showHostKeyboard;

                case InputGamePortClass::Global:
                    return f.showJoystick || f.showPaddle;

                case InputGamePortClass::Pair0:
                case InputGamePortClass::Pair1:
                    pairIsJoystick = f.pairIsJoystick[(e.gamePort == InputGamePortClass::Pair0) ? 0 : 1];
                    return pairIsJoystick ? f.showJoystick : f.showPaddle;
            }
            return true;

        case InputEventCategory::System:
            return true;

        case InputEventCategory::Guest:
            switch (e.gamePort)
            {
                case InputGamePortClass::None:
                    return f.showEmuKeyboard;

                case InputGamePortClass::Global:
                    return f.showJoystick || f.showPaddle;

                case InputGamePortClass::Pair0:
                case InputGamePortClass::Pair1:
                    pairIsJoystick = f.pairIsJoystick[(e.gamePort == InputGamePortClass::Pair0) ? 0 : 1];
                    return pairIsJoystick ? f.showJoystick : f.showPaddle;
            }
            return true;
    }

    return true;
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
////////////////////////////////////////////////////////////////////////////////

std::wstring BuildClipboardText (
    const std::vector<const InputEventDisplay *> &             selected,
    const std::array<InputLogicalColumn, kInputColumnCount> &  columns)
{
    std::wstring  out;
    size_t        rowIdx      = 0;
    int           colIdx      = 0;
    bool          firstColumn = true;


    for (rowIdx = 0; rowIdx < selected.size(); rowIdx++)
    {
        const InputEventDisplay *  row = selected[rowIdx];

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
    int                            i = 0;


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

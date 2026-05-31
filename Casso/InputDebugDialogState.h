#pragma once

#include "Pch.h"

#include "../CassoEmuCore/Devices/InputEvent.h"
#include "InputEventDisplay.h"
#include "Ui/Widgets/ListView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LogicalColumn / FilterState
//
////////////////////////////////////////////////////////////////////////////////

struct InputLogicalColumn
{
    int        id;
    LPCWSTR   headerText;
    int        defaultWidth;
    int        savedWidth;
    bool       visible;
    bool       autoSizedYet;
    bool       userResized;
};



struct InputFilterState
{
    bool  showHost   = true;
    bool  showGuest  = true;
    bool  showSystem = true;
};



constexpr int  kInputColWallWidth    = 110;
constexpr int  kInputColUptimeWidth  = 90;
constexpr int  kInputColCycleWidth   = 110;
constexpr int  kInputColSourceWidth  = 82;
constexpr int  kInputColAddressWidth = 80;
constexpr int  kInputColValueWidth   = 96;
constexpr int  kInputColMeaningWidth = 420;
constexpr int  kInputColumnCount     = 7;
constexpr int  kInputMeaningColumnId = kInputColumnCount - 1;



void  SeedDefaultColumns (std::array<InputLogicalColumn, kInputColumnCount> & columns) noexcept;
bool  MatchesFilter      (const InputEventDisplay & e, const InputFilterState & f) noexcept;



std::vector<ListView::Column> PlanVisibleColumns (
    const std::array<InputLogicalColumn, kInputColumnCount> & model) noexcept;

void          AppendColumnText (
    std::wstring &                                      out,
    const InputEventDisplay &                           e,
    int                                                 logicalId);

std::wstring  BuildClipboardText (
    const std::vector<const InputEventDisplay *> &       selected,
    const std::array<InputLogicalColumn, kInputColumnCount> & columns);

#pragma once

#include "Pch.h"

#include "DialogDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DialogLayout
//
//  Pure layout math for the themed dialog primitive. Headless: all
//  text-width measurements come through the injected
//  `measureBodyTextRun` / `measureButtonLabel` callbacks. Unit tests
//  supply deterministic stubs (e.g. constant pixels-per-character) so
//  layout assertions don't require DirectWrite.
//
////////////////////////////////////////////////////////////////////////////////



struct DialogLayoutMetrics
{
    float                                       dpiScale            = 1.0f;
    float                                       maxBodyWidthPx      = 360.0f;
    float                                       buttonHeightPx      = 28.0f;
    float                                       buttonPaddingPx     = 16.0f;
    float                                       buttonSpacingPx     = 8.0f;
    float                                       iconSizePx          = 32.0f;
    float                                       bodyLineHeightPx    = 18.0f;
    float                                       outerPaddingPx      = 16.0f;
    float                                       iconBodyGapPx       = 12.0f;
    float                                       bodyButtonsGapPx    = 16.0f;
    float                                       minButtonWidthPx    = 72.0f;
    std::function<float (std::wstring_view)>    measureBodyTextRun;
    std::function<float (std::wstring_view)>    measureButtonLabel;
};



struct DialogLayoutResult
{
    SIZE                totalSizePx        = {};
    RECT                iconRectPx         = {};   // zero when icon == None
    std::vector<RECT>   bodyRunRectsPx;             // 1:1 with definition.body
    std::vector<RECT>   hyperlinkHitRectsPx;        // subset where isHyperlink
    std::vector<RECT>   buttonRectsPx;              // 1:1 with definition.buttons
    RECT                customBodyRectPx   = {};   // zero when no onPaintCustomBody
};



DialogLayoutResult LayoutDialog (
    const DialogDefinition     & def,
    const DialogLayoutMetrics  & metrics);

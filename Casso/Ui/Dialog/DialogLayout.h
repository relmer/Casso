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
    SIZE                                        customBodyOverridePx = {};   // when non-zero, overrides def.customBodyMinSizePx
    SIZE                                        fillToSizePx         = {};   // when both dims > 0, the custom body fills this client size
};



struct DialogWrappedPiece
{
    size_t  runIndex = 0;
    size_t  start    = 0;
    size_t  count    = 0;
    float   xPx      = 0.0f;    // absolute, includes body origin
    float   yPx      = 0.0f;    // absolute, includes body origin
    float   widthPx  = 0.0f;
};



struct DialogLayoutResult
{
    SIZE                              totalSizePx        = {};
    RECT                              iconRectPx         = {};   // zero when icon == None
    std::vector<RECT>                 bodyRunRectsPx;             // 1:1 with definition.body
    std::vector<RECT>                 hyperlinkHitRectsPx;        // subset where isHyperlink
    std::vector<RECT>                 buttonRectsPx;              // 1:1 with definition.buttons
    RECT                              customBodyRectPx   = {};   // zero when no onPaintCustomBody
    std::vector<DialogWrappedPiece>   wrappedPiecesPx;            // per-line slices for renderer
    float                             bodyLineHeightPx   = 0.0f;
};



class DialogLayout
{
public:
    static DialogLayoutResult  Compute (
        const DialogDefinition     & def,
        const DialogLayoutMetrics  & metrics);

private:
    using WrappedRun = DialogWrappedPiece;

    struct LayoutState
    {
        const DialogDefinition     * def              = nullptr;
        const DialogLayoutMetrics  * metrics          = nullptr;
        DialogLayoutResult         * result           = nullptr;
        std::vector<WrappedRun>      wrapped;
        float                        bodyOriginXPx    = 0.0f;
        float                        bodyOriginYPx    = 0.0f;
        float                        bodyTotalHeightPx = 0.0f;
        float                        contentBottomPx  = 0.0f;
        float                        contentRightPx   = 0.0f;
        bool                         hasIcon          = false;
        bool                         hasCustomBody    = false;
    };

    static size_t  FindWrapBoundary        (std::wstring_view                                 text,
                                            size_t                                            start,
                                            float                                             maxWidthPx,
                                            const std::function<float (std::wstring_view)>  & measure);
    static void    WrapBody                (const std::vector<DialogTextRun>                & runs,
                                            float                                             maxBodyWidthPx,
                                            float                                             lineHeightPx,
                                            const std::function<float (std::wstring_view)>  & measure,
                                            std::vector<WrappedRun>                         & outWrapped,
                                            float                                           & outTotalHeightPx);
    static void    BeginLayout             (LayoutState & s);
    static void    PerformBodyWrap         (LayoutState & s);
    static void    BuildBodyRunRects       (LayoutState & s);
    static void    BuildHyperlinkHitRects  (LayoutState & s);
    static void    BuildIconRect           (LayoutState & s);
    static void    BuildCustomBodyRect     (LayoutState & s);
    static void    BuildButtonRects        (LayoutState & s);
    static void    ComputeTotalSize        (LayoutState & s);
};

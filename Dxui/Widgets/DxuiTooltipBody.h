#pragma once

#include "Pch.h"


class DxuiDpiScaler;
class IDxuiPainter;
class IDxuiTextRenderer;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipLook
//
//  What a tip draws besides its text: line colors, swatches and scales, a
//  gradient with its mark, a labeled frame, a corner tag, the face and the
//  wrap width. A tooltip has two: the look set for its next request, and the
//  look of the tip that is up.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTooltipLook
{
    std::vector<uint32_t>               lineArgbs;
    std::vector<uint32_t>               lineSwatches;
    std::vector<std::vector<uint32_t>>  lineScales;
    std::vector<uint32_t>               gradient;
    float                               gradientMark    = -1.0f;
    std::wstring                        frameLabel;
    uint32_t                            frameArgb       = 0;
    std::wstring                        cornerTag;
    uint32_t                            cornerTagArgb   = 0;
    bool                                isMonospace     = false;
    float                               maxTextWidthDip = 0.0f;   // 0 for the default

    bool  operator== (const DxuiTooltipLook & other) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTooltipBody
//
//  A tip's contents for one look, text and DPI: how much room they take and
//  how they are drawn inside the balloon. Made by the tip for each
//  measurement and each paint.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTooltipBody
{
public:
    //  The tip's colors, from its theme.
    struct Ink
    {
        uint32_t  backArgb   = 0;
        uint32_t  borderArgb = 0;
        uint32_t  textArgb   = 0;
    };

    //  The room around the content on each side: the tip's padding, or in a
    //  frame its inset and gap, with its label's line above.
    struct Margins
    {
        float  left   = 0.0f;
        float  top    = 0.0f;
        float  right  = 0.0f;
        float  bottom = 0.0f;
    };

    //  A tip with a tab in its text is a table: the tab-separated parts of
    //  each line start at the same columns, one em apart at the widest, so
    //  they line up in a proportional face as they would in a fixed one. A
    //  line with no tab spans the table and adds to no column.
    struct TableLayout
    {
        std::vector<std::vector<std::wstring>>  lines;
        std::vector<float>                      columnX;
        std::vector<float>                      columnW;
        float                                   width      = 0.0f;
        float                                   lineHeight = 0.0f;
    };

    using MeasureFn   = std::function<HRESULT (const std::wstring & part, float & widthPx, float & heightPx)>;
    using TextMeasure = std::function<HRESULT (const wchar_t * what, float sizePx, float wrapPx, float & widthPx, float & heightPx)>;

    static constexpr float  kDefaultMaxTextWidthDip = 340.0f;
    static constexpr float  kLineScaleWidthDip      = 64.0f;
    static constexpr float  kNoWrapPx               = 100000.0f;
    static constexpr float  kEstCharWidthEm         = 0.62f;
    static constexpr float  kEstLineHeightEm        = 1.4f;

    DxuiTooltipBody (const DxuiTooltipLook  & look,
                     const std::wstring     & text,
                     const DxuiDpiScaler    & scaler,
                     float                    fontDip,
                     const Ink              & ink);

    static bool             IsTable            (const std::wstring & text) { return text.find (L'\t') != std::wstring::npos; }
    static HRESULT          LayOutTable        (const std::wstring & text, float gapPx, const MeasureFn & measure, TableLayout & table);
    static uint32_t         GetGradientColor   (const std::vector<uint32_t> & stops, float at);
    static const wchar_t *  GetFace            (const DxuiTooltipLook & look);
    static float            GetMaxTextWidthDip (const DxuiTooltipLook & look);

    bool             IsTableTip          () const;
    bool             HasFrame            () const { return !m_look.frameLabel.empty(); }
    bool             HasGradient         () const { return !m_look.gradient.empty(); }
    const wchar_t  * GetFace             () const { return GetFace (m_look); }
    float            GetMaxTextWidthDip  () const { return GetMaxTextWidthDip (m_look); }
    float            GetFontPx           () const;
    Margins          GetContentMargins   (float padXPx, float padYPx) const;
    float            GetGradientExtraPx  () const;
    float            GetGradientWidthPx  () const;
    float            GetFrameWidthPx     (const TextMeasure & measure) const;
    float            GetCornerTagPx      (const TextMeasure & measure) const;
    float            GetSwatchIndentPx   (float lineHeight) const;
    float            GetLineIndentPx     (float lineHeight) const;
    uint32_t         GetEdgeArgb         () const;
    HRESULT          LayOutTableWith     (IDxuiTextRenderer & text, TableLayout & table) const;

    void             DrawTable           (IDxuiPainter & painter, IDxuiTextRenderer & text, const TableLayout & table, float left, float top) const;
    void             DrawCornerTag       (IDxuiTextRenderer & text, float right, float top, float lineHeight) const;
    void             DrawFrame           (IDxuiPainter & painter, IDxuiTextRenderer & text, float width, float height) const;
    void             DrawGradient        (IDxuiPainter & painter, float left, float width, float bottom) const;

private:
    //  A fixed-width tip's lines are columns, broken where the caller broke
    //  them, so they run far wider before they wrap: wrapping one moves the
    //  end of a line, a cycle count, say, under the start of the next.
    static constexpr float  kMaxMonoWidthDip   = 720.0f;

    static constexpr float  kGradientGapDip    = 6.0f;
    static constexpr float  kGradientHeightDip = 10.0f;
    static constexpr float  kGradientArrowDip  = 6.0f;
    static constexpr float  kGradientWidthDip  = 200.0f;
    static constexpr int    kGradientSlices    = 64;

    //  The frame: its label's size, its inset from the tip's edge on every
    //  side, the gap between it and the content, and the lowest contrast of
    //  its label and line, WCAG's minimums for text and a control's border.
    static constexpr float  kFrameLabelScale   = 0.85f;
    static constexpr float  kFrameInsetDip     = 4.0f;
    static constexpr float  kFrameGapDip       = 6.0f;
    static constexpr float  kFrameCornerDip    = 4.0f;
    static constexpr float  kFrameMix          = 0.6f;
    static constexpr float  kFramedEdgeMix     = 0.15f;   // the tip's edge around a frame, close to its fill
    static constexpr float  kFrameLineEm       = 1.4f;
    static constexpr float  kMinTextContrast   = 4.5f;
    static constexpr float  kMinLineContrast   = 3.0f;

    //  The label's line is drawn centered on the frame's top edge; its
    //  letters extend about kFrameCapEm of the label's size above and below
    //  the middle. The frame's top edge is kFrameInsetDip below the top of
    //  the letters, and its other edges are as far from the tip's edge as
    //  its top edge is.
    static constexpr float  kFrameCapEm        = 0.36f;

    float  GetFrameLabelPx  () const;
    float  GetFrameLabelHPx () const;
    float  GetFrameEdgePx   () const;
    void   DrawLineScale    (IDxuiPainter & painter, const std::vector<uint32_t> & stops, float left, float top, float heightPx) const;

    const DxuiTooltipLook  & m_look;
    const std::wstring     & m_text;
    const DxuiDpiScaler    & m_scaler;
    float                    m_fontDip = 0.0f;
    Ink                      m_ink;
};

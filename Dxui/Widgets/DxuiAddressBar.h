#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "DxuiTextInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar
//
//  The current location as a row of segments, as in File Explorer's address
//  bar. Each segment is followed by a separator. A click on a segment reports
//  its index, and a click on a separator reports the index of the segment
//  before it with the separator's rect, for the host to hang a menu from. A
//  click past the last separator, or Enter or F4 while the bar has focus,
//  turns the bar into a text field holding the location's path, all selected.
//  Enter reports what was typed, an X at the right end empties the field, and
//  Escape or a loss of focus puts the segments back.
//
//  Segments and separators light on hover and press with Windows' subtle fills
//  in rounded cards inset from the bar's edges, as Explorer's do.
//
//  Segments that do not fit collapse from the start behind an overflow button,
//  which reports its rect for the host's menu of them, so the location's own
//  name stays in view.
//
//  The host decides what a typed path means and ends the edit once it has
//  gone there; a path it cannot use leaves the field open.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiAddressBar : public IDxuiControl
{
public:
    using SegmentFn   = std::function<void (int index)>;
    using SeparatorFn = std::function<void (int index, const RECT & anchor)>;
    using OverflowFn  = std::function<void (const RECT & anchor)>;
    using SubmitFn    = std::function<void (const std::wstring & text)>;

    //  What lies under a point.
    enum class Part { None, Overflow, Segment, Separator, Blank, Clear };

    struct Hit
    {
        Part  part  = Part::None;
        int   index = -1;

        bool operator== (const Hit & other) const { return part == other.part && index == other.index; }
    };

    //  Explorer's address bar text: Windows 11's text face at its body size.
    static constexpr const wchar_t *  kVariableTextFace = L"Segoe UI Variable Text";
    static constexpr float            kFontDip          = 14.0f;

    //  A typed or pasted path is as long as Windows allows one to be, not the
    //  text input's short default.
    static constexpr size_t  kMaxPathChars = 32767;

    DxuiAddressBar() { m_focusable = true; m_input.SetChromeless (true); m_input.SetMaxLength (kMaxPathChars); }
    ~DxuiAddressBar() override = default;

    void  SetSegments     (std::vector<std::wstring> labels);
    void  SetPath         (const std::wstring & path)             { m_path = path; }
    void  SetOnSegment    (SegmentFn fn)                          { m_onSegment   = std::move (fn); }
    void  SetOnSeparator  (SeparatorFn fn)                        { m_onSeparator = std::move (fn); }
    void  SetOnOverflow   (OverflowFn fn)                         { m_onOverflow  = std::move (fn); }
    void  SetOnSubmit     (SubmitFn fn)                           { m_onSubmit    = std::move (fn); }
    void  SetTextRenderer (IDxuiTextRenderer * text)              { m_renderer = text; m_input.SetTextRenderer (text); }
    void  SetFont         (const wchar_t * face, float sizeDip)   { m_face = face; m_fontDip = sizeDip; m_input.SetFont (face, sizeDip); LayoutSegments(); }
    void  SetIconFace     (const wchar_t * face)                  { m_iconFace = face; }

    //  The chevron of the separator whose menu is open turns to point down,
    //  and back when it closes (-1); the host ticks the turn.
    void  SetOpenSeparator (int index);

    void  BeginEdit ();
    void  EndEdit   ();

    bool                 IsEditing     () const { return m_editing; }

    //  Whether focus shows a ring. Focus from the keyboard does; a click gives
    //  the bar focus without one, as Explorer's does.
    void                 SetFocusCue   (bool show) { m_focusCue = show; }
    bool                 IsInteracting () const { return m_pressed.part != Part::None || m_clearPressed; }
    bool                 WantsTick     () const { return m_chevronAngle != m_chevronTarget; }
    const std::wstring & GetEditText   () const { return m_input.GetText(); }
    int                  GetFirstShown () const { return m_firstShown; }
    Hit                  HitTest       (int x, int y) const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged    (bool focused) override;
    void                Tick              (int64_t nowMs) override;
    bool                QueryCommand      (DxuiStandardCommand command, bool & outEnabled) const override { return m_editing && m_input.QueryCommand (command, outEnabled); }
    bool                InvokeCommand     (DxuiStandardCommand command) override                      { return m_editing && m_input.InvokeCommand (command); }
    std::wstring        GetAccessibleName () const override { return m_path; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::TextInput; }

private:
    static constexpr float  s_kChevronDip       = 12.0f;
    static constexpr int    s_kPadXDip          = 4;
    static constexpr int    s_kSegmentPadDip    = 8;
    static constexpr int    s_kSeparatorDip     = 28;
    static constexpr int    s_kHoverInsetDip    = 4;
    static constexpr int    s_kOverflowDip      = 32;
    static constexpr int    s_kClearDip         = 32;
    static constexpr float  s_kCancelDip        = 10.0f;
    static constexpr float  s_kTurnMs           = 150.0f;   // a chevron's quarter turn
    static constexpr float  s_kChevronHalfDip   = 4.5f;   // half the chevron's height, measured off Explorer
    static constexpr float  s_kChevronDepthDip  = 4.5f;
    static constexpr float  s_kChevronStrokeDip = 1.25f;

    void  LayoutSegments ();
    int   MeasurePx      (const std::wstring & label) const;
    void  PaintHover     (IDxuiPainter & painter, const IDxuiTheme & theme, const RECT & rc, const Hit & hit) const;
    void  PaintChevron   (IDxuiPainter & painter, const RECT & rc, float angleDegrees, uint32_t argb) const;
    RECT  GetClearRect   () const;

    std::vector<std::wstring>    m_labels;
    std::vector<RECT>            m_rects;
    std::vector<RECT>            m_separators;
    RECT                         m_overflow    = {};
    RECT                         m_overflowSep = {};
    std::wstring                 m_path;
    DxuiTextInput                m_input;
    SegmentFn                    m_onSegment;
    SeparatorFn                  m_onSeparator;
    OverflowFn                   m_onOverflow;
    SubmitFn                     m_onSubmit;
    IDxuiTextRenderer          * m_renderer    = nullptr;
    const wchar_t              * m_face        = nullptr;
    const wchar_t            * m_iconFace   = L"Segoe MDL2 Assets";
    float          m_fontDip       = kFontDip;
    DxuiDpiScaler  m_scaler;
    int            m_firstShown    = 0;
    Hit            m_hover;
    Hit            m_pressed;
    bool           m_editing       = false;
    bool           m_focused       = false;
    bool           m_focusCue      = true;
    int            m_chevronIndex  = -1;
    float          m_chevronAngle  = 0.0f;
    float          m_chevronTarget = 0.0f;
    int64_t        m_lastTickMs    = 0;
    bool           m_clearHover    = false;
    bool           m_clearPressed  = false;
};

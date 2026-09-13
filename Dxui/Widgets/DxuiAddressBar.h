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
//  Enter reports what was typed, and Escape or a loss of focus puts the
//  segments back.
//
//  Segments and separators light on hover and press with Windows' subtle fills
//  in rounded cards inset from the bar's edges, as Explorer's do.
//
//  Segments that do not fit are dropped from the start, so the location's own
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
    using SubmitFn    = std::function<void (const std::wstring & text)>;

    //  What lies under a point.
    enum class Part { None, Segment, Separator, Blank };

    struct Hit
    {
        Part  part  = Part::None;
        int   index = -1;

        bool operator== (const Hit & other) const { return part == other.part && index == other.index; }
    };

    //  Explorer's address bar text: Windows 11's text face at its body size.
    static constexpr const wchar_t *  kVariableTextFace = L"Segoe UI Variable Text";
    static constexpr float            kFontDip          = 14.0f;

    DxuiAddressBar() { m_focusable = true; m_input.SetChromeless (true); }
    ~DxuiAddressBar() override = default;

    void  SetSegments     (std::vector<std::wstring> labels);
    void  SetPath         (const std::wstring & path)             { m_path = path; }
    void  SetOnSegment    (SegmentFn fn)                          { m_onSegment   = std::move (fn); }
    void  SetOnSeparator  (SeparatorFn fn)                        { m_onSeparator = std::move (fn); }
    void  SetOnSubmit     (SubmitFn fn)                           { m_onSubmit    = std::move (fn); }
    void  SetTextRenderer (IDxuiTextRenderer * text)              { m_renderer = text; m_input.SetTextRenderer (text); }
    void  SetFont         (const wchar_t * face, float sizeDip)   { m_face = face; m_fontDip = sizeDip; LayoutSegments(); }
    void  SetIconFace     (const wchar_t * face)                  { m_iconFace = face; }

    void  BeginEdit ();
    void  EndEdit   ();

    bool                 IsEditing     () const { return m_editing; }
    bool                 IsInteracting () const { return m_pressed.part != Part::None; }
    const std::wstring & GetEditText   () const { return m_input.GetText(); }
    int                  GetFirstShown () const { return m_firstShown; }
    Hit                  HitTest       (int x, int y) const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent   & ev) override;
    void                OnFocusChanged    (bool focused) override;
    bool                QueryCommand      (DxuiStandardCommand command, bool & outEnabled) const override { return m_editing && m_input.QueryCommand (command, outEnabled); }
    bool                InvokeCommand     (DxuiStandardCommand command) override                      { return m_editing && m_input.InvokeCommand (command); }
    std::wstring        GetAccessibleName () const override { return m_path; }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::TextInput; }

private:
    static constexpr float  s_kChevronDip    = 12.0f;
    static constexpr int    s_kPadXDip       = 4;
    static constexpr int    s_kSegmentPadDip = 8;
    static constexpr int    s_kSeparatorDip  = 28;
    static constexpr int    s_kHoverInsetDip = 4;

    void  LayoutSegments ();
    int   MeasurePx      (const std::wstring & label) const;
    void  PaintHover     (IDxuiPainter & painter, const IDxuiTheme & theme, const RECT & rc, const Hit & hit) const;

    std::vector<std::wstring>  m_labels;
    std::vector<RECT>          m_rects;
    std::vector<RECT>          m_separators;
    std::wstring               m_path;
    DxuiTextInput              m_input;
    SegmentFn                  m_onSegment;
    SeparatorFn                m_onSeparator;
    SubmitFn                   m_onSubmit;
    IDxuiTextRenderer        * m_renderer   = nullptr;
    const wchar_t            * m_face       = nullptr;
    const wchar_t            * m_iconFace   = L"Segoe MDL2 Assets";
    float                      m_fontDip    = kFontDip;
    DxuiDpiScaler              m_scaler;
    int                        m_firstShown = 0;
    Hit                        m_hover;
    Hit                        m_pressed;
    bool                       m_editing    = false;
    bool                       m_focused    = false;
};

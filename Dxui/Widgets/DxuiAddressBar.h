#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "DxuiTextInput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBar
//
//  The current location as a row of segments, as in File Explorer's address
//  bar. A click on a segment reports its index; a click past the last one, or
//  Enter or F4 while the bar has focus, turns the bar into a text field
//  holding the location's path, all selected. Enter reports what was typed,
//  and Escape or a loss of focus puts the segments back.
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
    using SegmentFn = std::function<void (int index)>;
    using SubmitFn  = std::function<void (const std::wstring & text)>;

    DxuiAddressBar() { m_focusable = true; m_input.SetChromeless (true); }
    ~DxuiAddressBar() override = default;

    void  SetSegments     (std::vector<std::wstring> labels);
    void  SetPath         (const std::wstring & path)   { m_path = path; }
    void  SetOnSegment    (SegmentFn fn)                { m_onSegment = std::move (fn); }
    void  SetOnSubmit     (SubmitFn fn)                 { m_onSubmit  = std::move (fn); }
    void  SetTextRenderer (IDxuiTextRenderer * text)    { m_renderer = text; m_input.SetTextRenderer (text); }

    void  BeginEdit ();
    void  EndEdit   ();

    bool                 IsEditing      () const { return m_editing; }
    bool                 IsInteracting  () const { return m_pressed != s_kNoSegment; }
    const std::wstring & GetEditText    () const { return m_input.GetText(); }
    int                  GetFirstShown  () const { return m_firstShown; }

    //  The segment at a point, or one of the negative values below.
    int                  HitTestSegment (int x, int y) const;

    static constexpr int  s_kNoSegment = -1;   // outside the bar
    static constexpr int  s_kBlank     = -2;   // past the last segment
    static constexpr int  s_kSeparator = -3;   // between two segments

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
    static constexpr float  s_kFontDip       = 13.0f;
    static constexpr float  s_kRadiusDip     = 4.0f;
    static constexpr int    s_kPadXDip       = 8;
    static constexpr int    s_kSegmentPadDip = 6;
    static constexpr int    s_kSeparatorDip  = 18;

    void  LayoutSegments ();
    int   MeasurePx      (const std::wstring & label) const;

    std::vector<std::wstring>  m_labels;
    std::vector<RECT>          m_rects;
    std::wstring               m_path;
    DxuiTextInput              m_input;
    SegmentFn                  m_onSegment;
    SubmitFn                   m_onSubmit;
    IDxuiTextRenderer        * m_renderer      = nullptr;
    DxuiDpiScaler              m_scaler;
    int                        m_firstShown    = 0;
    int                        m_segmentsRight = 0;
    int                        m_hover         = s_kNoSegment;
    int                        m_pressed       = s_kNoSegment;
    bool                       m_editing       = false;
    bool                       m_focused       = false;
};

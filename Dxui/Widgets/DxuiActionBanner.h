#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"
#include "Widgets/DxuiInfoBanner.h"
#include "Widgets/DxuiButton.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiActionBanner
//
//  A notice strip that carries actions: the bordered banner, and a row of
//  buttons along its trailing edge.
//
//  DxuiInfoBanner CANNOT DO THIS ALONE, and its own header says so -- "not
//  clickable, no raised surface". So this composes one rather than replacing
//  it: the notice keeps its themed fill, border and glyph, and the buttons are
//  ordinary DxuiButtons laid into the space reserved for them.
//
//  NOT DxuiButtonRow, WHICH IS DIALOG CHROME. That row lives in Dxui/Window/,
//  aligns to a dialog's margins and owns a dialog's default/cancel semantics. A
//  banner floating over a running machine has none of those things.
//
//  A WIDGET RATHER THAN A HOST IN THE EXE. Laying the buttons out beside
//  wrapped text is geometry, and geometry belongs somewhere a test can measure
//  it -- which the emulator's shell is not.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiActionBanner : public IDxuiControl
{
public:

    using ActionFn = std::function<void (size_t index)>;

    DxuiActionBanner  () = default;
    ~DxuiActionBanner () override = default;

    void  SetText     (const std::wstring & text) { m_banner.SetText (text); }
    const std::wstring &  GetText () const { return m_banner.GetText(); }

    void  SetSeverity (DxuiInfoBanner::Severity severity) { m_banner.SetSeverity (severity); }

    //  Replaces the whole set. The banner is rebuilt rather than added to,
    //  because a report that absorbs later changes must not accumulate a second
    //  copy of the same action.
    void  SetActions  (const std::vector<std::wstring> & labels);

    size_t  GetActionCount () const { return m_actions.size(); }

    //  Called with the index of the action that was pressed.
    void  SetOnAction (ActionFn action) { m_onAction = std::move (action); }

    //  The button, for a test that wants to press it or read its label.
    DxuiButton *  GetAction (size_t index);

    //  Where the bordered strip was laid out. It spans the whole bounds: the
    //  actions are inside it, and what keeps the text off them is the notice's
    //  trailing reserve rather than a narrower box.
    RECT  GetNoticeBounds () const { return m_banner.GetBounds(); }

    //  How much of the trailing edge the actions occupy, which is what the
    //  notice reserves out of its text.
    float  GetActionReservePx (const DxuiDpiScaler & scaler) const { return GetActionColumnPx (scaler); }

    //  The height the banner needs at this width, with room for its actions.
    //  Never shorter than one button, since a notice whose action is clipped
    //  offers nothing.
    float  GetPreferredHeightPx (float widthPx, const DxuiDpiScaler & scaler) const;

    void  Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    bool  OnMouse (const DxuiMouseEvent & ev) override;
    void  Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    std::wstring        GetAccessibleName () const override { return m_banner.GetText(); }
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Label; }

private:

    //  Geometry, in DIP. The action column is reserved out of the banner's
    //  width before the text wraps into what is left, so a long message cannot
    //  push a button off the edge.
    static constexpr float  s_kActionWidthDip  = 96.0f;
    static constexpr float  s_kActionHeightDip = 26.0f;
    static constexpr float  s_kActionGapDip    = 8.0f;
    static constexpr float  s_kEdgePadDip      = 10.0f;

    //  How much width the actions take, including the gap to the text.
    float  GetActionColumnPx (const DxuiDpiScaler & scaler) const;

    //  Mutable because measuring the height has to tell the notice how much of
    //  its trailing edge is spoken for, and measuring is const to every caller.
    mutable DxuiInfoBanner                      m_banner;
    std::vector<std::unique_ptr<DxuiButton>>    m_actions;
    ActionFn                                    m_onAction;
    DxuiDpiScaler                               m_scaler;
};

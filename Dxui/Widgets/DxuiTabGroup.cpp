#include "Pch.h"

#include "Widgets/DxuiTabGroup.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiColor.h"
#include "Core/DxuiPaneMetrics.h"
#include "Core/DxuiUnicodeSymbols.h"
#include "Core/DxuiEvents.h"
#include "Widgets/DxuiPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::DxuiTabGroup
//
//  The strip's handlers are set once, here, never while one of them may be
//  running: selecting a tab lays the group out again, which would otherwise
//  replace the handler that is selecting it. A close or a + press only notes
//  what was asked, and the group acts on it once the strip has returned.
//
////////////////////////////////////////////////////////////////////////////////

DxuiTabGroup::DxuiTabGroup()
{
    m_focusable = true;

    m_strip.SetOnChange  ([this] (int index) { SetActive (index); });
    m_strip.SetOnDragOut ([this] (int index, POINT pointDip)
    {
        if (m_onDragStart)
        {
            m_onDragStart (index, pointDip);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::AddTab
//
//  The first tab added becomes the active one; later ones start hidden.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::AddTab (const std::wstring & title, IDxuiControl * content)
{
    Tab  tab;



    if (content == nullptr || IndexOf (content) >= 0)
    {
        return;
    }

    tab.title   = title;
    tab.content = content;
    m_tabs.push_back (tab);

    if (m_active < 0)
    {
        m_active = 0;
    }

    LayoutStrip();
    LayoutContent();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::RemoveTab
//
//  Removing the active tab shows the one that took its place, or the new
//  last one.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::RemoveTab (IDxuiControl * content)
{
    int  index = IndexOf (content);



    if (index < 0)
    {
        return false;
    }

    m_tabs.erase (m_tabs.begin() + index);

    if (m_tabs.empty())
    {
        m_active = -1;
    }
    else if (index < m_active || m_active >= (int) m_tabs.size())
    {
        m_active = std::max (0, m_active - 1);
    }

    LayoutStrip();
    LayoutContent();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetTitle
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetTitle (IDxuiControl * content, const std::wstring & title)
{
    int  index = IndexOf (content);



    if (index >= 0)
    {
        m_tabs[(size_t) index].title = title;
        SyncStrip();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetIndicator
//
//  The active tab's content is on screen, so it never carries one.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetIndicator (IDxuiControl * content, bool on)
{
    int  index = IndexOf (content);



    if (index >= 0)
    {
        m_tabs[(size_t) index].indicator = on && index != m_active;
        SyncStrip();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetTabTip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetTabTip (IDxuiControl * content, const std::wstring & tip)
{
    int  index = IndexOf (content);



    if (index >= 0)
    {
        m_tabs[(size_t) index].tip = tip;
        SyncStrip();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetTitleExtra
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetTitleExtra (IDxuiControl * content, int widthDip)
{
    int  index = IndexOf (content);



    if (index >= 0)
    {
        m_tabs[(size_t) index].titleExtraDip = std::max (0, widthDip);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetTitleExtraRect
//
//  Just ahead of the menu button, as tall as the buttons; empty where the
//  group has no title bar or its active tab keeps no room.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetTitleExtraRect() const
{
    RECT  menu  = GetTitleButtonRect (TitleButton::Menu);
    int   extra = (m_active >= 0 && m_active < (int) m_tabs.size()) ? m_tabs[(size_t) m_active].titleExtraDip : 0;



    if (extra <= 0 || menu.right <= menu.left)
    {
        return RECT {};
    }

    return RECT { menu.left - m_scaler.ToPx (extra), menu.top, menu.left, menu.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetLeadingMark
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetLeadingMark (IDxuiControl * content, const LeadingMark & mark)
{
    int  index = IndexOf (content);



    if (index >= 0)
    {
        m_tabs[(size_t) index].leadMark = mark;
        SyncStrip();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetKind
//
//  The kind moves the tabs and the body, so the group lays itself out again.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetKind (Kind kind)
{
    if (kind == m_kind)
    {
        return;
    }

    m_kind = kind;
    LayoutStrip();
    LayoutContent();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetNewTab
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetNewTab (NewTabShownFn shown, NewTabFn add)
{
    m_newTabShown = std::move (shown);
    m_newTab      = std::move (add);

    LayoutStrip();
    LayoutContent();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetContent
//
////////////////////////////////////////////////////////////////////////////////

IDxuiControl * DxuiTabGroup::GetContent (int index) const
{
    return (index >= 0 && index < (int) m_tabs.size()) ? m_tabs[(size_t) index].content : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::HasIndicator
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::HasIndicator (int index) const
{
    return index >= 0 && index < (int) m_tabs.size() && m_tabs[(size_t) index].indicator;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::IndexOf
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabGroup::IndexOf (IDxuiControl * content) const
{
    for (size_t i = 0; i < m_tabs.size(); i++)
    {
        if (m_tabs[i].content == content)
        {
            return (int) i;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetActive
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetActive (int index)
{
    if (index < 0 || index >= (int) m_tabs.size() || index == m_active)
    {
        return;
    }

    m_active                         = index;
    m_tabs[(size_t) index].indicator = false;

    SyncStrip();
    LayoutContent();

    if (m_onActivated)
    {
        m_onActivated (index);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetGrabOffset
//
////////////////////////////////////////////////////////////////////////////////

POINT DxuiTabGroup::GetGrabOffset (bool tab) const
{
    RECT  title = GetTitleRect();



    if (tab)
    {
        return m_strip.GetGrabOffset();
    }

    return POINT { m_pressedAt.x - title.left, m_pressedAt.y - title.top };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::HasStrip
//
//  A document group always shows its tabs. A tool window shows them only
//  when there is a choice to make or a + to offer: a single pane's title bar
//  says all a tab would. A pane being torn off keeps its tab while carried.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::HasStrip() const
{
    if (m_kind == Kind::Document)
    {
        return true;
    }

    return m_stripForced || m_tabs.size() > 1 || (m_newTab && m_newTabShown && m_newTabShown (*this));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetTitleRect
//
//  A tool window's title bar along its top; a document group has none.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetTitleRect() const
{
    long  height = (m_kind == Kind::ToolWindow) ? m_scaler.ToPx (kTitleDip) : 0;



    return RECT { m_boundsDip.left, m_boundsDip.top, m_boundsDip.right, std::min (m_boundsDip.bottom, m_boundsDip.top + height) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetStripRect
//
//  The tab band: along a document group's top, or along a tool window's
//  bottom, below its title bar and the line under the pane; empty along the
//  bottom when the group shows no tabs. The line between the band and the
//  pane is not part of it. A document's band is kStripDip deep and the line
//  lies below it; a tool window's band and the line over it are kStripDip
//  deep together, as Visual Studio draws them: 25, 31 and 38 px at 100%,
//  125% and 150%.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetStripRect() const
{
    long  height = m_scaler.ToPx (kStripDip);
    long  line   = DxuiPaneMetrics::GetLinePx (m_scaler);
    long  title  = GetTitleRect().bottom;



    if (m_kind == Kind::Document)
    {
        return RECT { m_boundsDip.left, m_boundsDip.top, m_boundsDip.right, std::min (m_boundsDip.bottom, m_boundsDip.top + height) };
    }

    if (!HasStrip())
    {
        return RECT { m_boundsDip.left, m_boundsDip.bottom, m_boundsDip.right, m_boundsDip.bottom };
    }

    return RECT { m_boundsDip.left, std::max (title + line, m_boundsDip.bottom - height + line), m_boundsDip.right, m_boundsDip.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetBodyRect
//
//  Inside the pane's outline, below a document's tabs or a tool window's
//  title bar, and above a tool window's tabs.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetBodyRect() const
{
    return DxuiPaneFrame::GetBodyRect (GetFrameSpec());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::IsCloseShown
//
//  A tool window's close button, shown while its active pane can close.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::IsCloseShown() const
{
    return m_kind == Kind::ToolWindow && m_active >= 0 && (!m_canClose || m_canClose (m_active));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetTitleButtonRect
//
//  The title bar's buttons from its right end: close, when shown, then the
//  pin, then the menu, as Visual Studio orders them, each kTitleButtonDip
//  across. The close button ends one line in from the pane's right edge,
//  inside the outline, and every button runs from below the title bar's
//  outline to its bottom: 24, 30 and 36 px square at 100%, 125% and 150%.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetTitleButtonRect (TitleButton button) const
{
    RECT  title = GetTitleRect();
    long  size  = m_scaler.ToPx (kTitleButtonDip);
    long  line  = DxuiPaneMetrics::GetLinePx (m_scaler);
    long  slot  = 0;
    long  right = 0;



    if (m_kind != Kind::ToolWindow || (button == TitleButton::Close && !IsCloseShown()))
    {
        return RECT {};
    }

    slot  = (button == TitleButton::Close) ? 0 : (button == TitleButton::Pin) ? 1 : 2;
    slot -= (button != TitleButton::Close && !IsCloseShown()) ? 1 : 0;
    right = title.right - line - slot * size;

    return RECT { right - size, std::min (title.bottom, title.top + line), right, title.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetTitleButtonAt
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabGroup::GetTitleButtonAt (POINT pointDip) const
{
    static constexpr TitleButton  kButtons[] = { TitleButton::Menu, TitleButton::Pin, TitleButton::Close };



    for (TitleButton button : kButtons)
    {
        RECT  r = GetTitleButtonRect (button);

        if (pointDip.x >= r.left && pointDip.x < r.right && pointDip.y >= r.top && pointDip.y < r.bottom)
        {
            return (int) button;
        }
    }

    return -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetTabRect
//
//  Where the strip draws the tab, which is where it is hit.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetTabRect (int index) const
{
    if (index < 0 || index >= (int) m_strip.GetTabs().size() || !HasStrip())
    {
        return RECT {};
    }

    return m_strip.GetTabScreenRect (index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetNewTabRect
//
//  Past the last tab, or empty when the group shows no +.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetNewTabRect() const
{
    return HasStrip() ? m_strip.GetNewTabRect() : RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::HitTestTab
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabGroup::HitTestTab (POINT pointDip) const
{
    return HasStrip() ? m_strip.HitTest (pointDip.x, pointDip.y) : -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::IsChromeAt
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::IsChromeAt (POINT pointDip) const
{
    RECT  title = GetTitleRect();
    RECT  strip = GetStripRect();
    auto  in    = [pointDip] (const RECT & r)
    {
        return pointDip.x >= r.left && pointDip.x < r.right && pointDip.y >= r.top && pointDip.y < r.bottom;
    };



    return in (title) || in (strip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::TryGetTabButtonAt
//
//  A tab's pin is the title bar's pin and its close button the title bar's
//  close button, for that tab's pane.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::TryGetTabButtonAt (
    POINT          pointDip,
    TitleButton  & button,
    int          & index,
    RECT         & rect) const
{
    DxuiTabStrip::TabButton  found = DxuiTabStrip::TabButton::None;



    index = -1;
    rect  = {};

    if (HasStrip())
    {
        found = m_strip.GetTabButtonAt (pointDip.x, pointDip.y, index, rect);
    }

    button = (found == DxuiTabStrip::TabButton::Pin) ? TitleButton::Pin : TitleButton::Close;

    return found != DxuiTabStrip::TabButton::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetInsertIndexAt
//
////////////////////////////////////////////////////////////////////////////////

int DxuiTabGroup::GetInsertIndexAt (POINT pointDip) const
{
    if (!HasStrip())
    {
        return (int) m_tabs.size();
    }

    return m_strip.GetInsertIndexAt (pointDip.x);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SetInsertGap
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SetInsertGap (int index, int widthPx)
{
    m_strip.SetInsertGap (HasStrip() ? index : -1, widthPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetInsertGapRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetInsertGapRect() const
{
    return HasStrip() ? m_strip.GetInsertGapRect() : RECT {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::Layout
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    SetBounds (boundsDip);
    m_scaler = scaler;

    LayoutStrip();
    LayoutContent();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::LayoutStrip
//
//  The strip's bounds, then its tabs within them: a tool window's strip comes
//  and goes with its second tab. The tabs round their corners as the pane's
//  frame does, so a pane too small to round has square tabs.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::LayoutStrip()
{
    m_strip.SetCornerPx ((int) DxuiPaneFrame::GetCornerPx (m_boundsDip, DxuiPaneMetrics::GetCornerPx (m_scaler)));
    m_strip.Layout      (GetStripRect(), m_scaler);
    SyncStrip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SyncStrip
//
//  Hands the strip the tabs as they stand. Each is as wide as its title
//  measured by the renderer of the last paint, or before the first paint at
//  an average character width. A leading mark goes ahead of the title; an
//  indicator, a dot in the accent color, takes its place on a tab with none.
//  A document tab's pin and close button act as the title bar's do, for its
//  pane. A tool window's tabs have neither, as Visual Studio's have none.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SyncStrip()
{
    DxuiTabStrip::Style             style    = (m_kind == Kind::Document) ? DxuiTabStrip::Style::Document : DxuiTabStrip::Style::ToolWindow;
    bool                            hasClose = m_kind == Kind::Document && m_onCloseTab != nullptr;
    bool                            hasPin   = m_kind == Kind::Document && m_onTitleButton != nullptr;
    RECT                            strip    = GetStripRect();
    long                            x        = strip.left;
    std::vector<DxuiTabStrip::Tab>  tabs;



    for (size_t i = 0; i < m_tabs.size(); i++)
    {
        const Tab        & tab   = m_tabs[i];
        DxuiTabStrip::Tab  shown;
        long               width = 0;

        shown.label    = tab.title;
        shown.tip      = tab.tip;
        shown.closable = !m_canClose || m_canClose ((int) i);

        if (tab.leadMark.argb != 0)
        {
            shown.mark     = tab.leadMark.glyph.empty() ? std::wstring (1, s_kchBlackCircle) : tab.leadMark.glyph;
            shown.markFace = tab.leadMark.face;
            shown.markArgb = tab.leadMark.argb;
        }
        else if (tab.indicator)
        {
            shown.mark     = std::wstring (1, s_kchBlackCircle);
            shown.markArgb = (m_indicatorArgb != 0) ? m_indicatorArgb : 0xFF0078D4u;
        }

        width      = DxuiTabStrip::MeasureTabPx (m_measure, shown, style, hasClose, m_scaler);
        shown.rect = RECT { x, strip.top, x + width, strip.bottom };
        x         += width;

        tabs.push_back (std::move (shown));
    }

    m_strip.SetStyle    (style);
    m_strip.SetTabs     (std::move (tabs));
    m_strip.SetSelected (m_active);

    //  Set only between strip events, so none replaces itself as it runs.
    m_strip.SetOnClose  (hasClose ? DxuiTabStrip::CloseFn ([this] (int index) { m_pendingClose = index; }) : nullptr);
    m_strip.SetOnPin    (hasPin ? DxuiTabStrip::PinFn ([this] (int index) { m_pendingPin = index; }) : nullptr);
    m_strip.SetOnNewTab ((m_newTab && m_newTabShown && m_newTabShown (*this)) ? DxuiTabStrip::NewTabFn ([this] { m_pendingNewTab = true; }) : nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::RunPending
//
//  A close, a pin or a + the strip reported, now that it has returned.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::RunPending()
{
    int   closing = m_pendingClose;
    int   pinning = m_pendingPin;
    bool  adding  = m_pendingNewTab;



    m_pendingClose  = -1;
    m_pendingPin    = -1;
    m_pendingNewTab = false;

    if (closing >= 0 && m_onCloseTab)
    {
        m_onCloseTab (closing);
    }
    else if (pinning >= 0 && m_onTitleButton)
    {
        m_onTitleButton (TitleButton::Pin, pinning, POINT {});
    }
    else if (adding && m_newTab)
    {
        m_newTab (*this);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::LayoutContent
//
//  The active control fills the body; the others are hidden where they are.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::LayoutContent()
{
    for (int i = 0; i < (int) m_tabs.size(); i++)
    {
        IDxuiControl  * content = m_tabs[(size_t) i].content;
        bool            shown   = (i == m_active) && IsVisible();

        content->SetVisible (shown);

        if (shown)
        {
            content->Layout (GetBodyRect(), m_scaler);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::Paint
//
//  The frame's under parts -- the band, the title bar's fill and the gap
//  outside their rounded corners -- then the title bar of a tool window,
//  then the tabs, then the joins' fillets, which flare the selected tab into
//  the line over a hovered neighbor's fill, as Visual Studio draws them.
//  The outline is PaintFrame's.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    RECT                            strip  = GetStripRect();
    std::vector<DxuiPaneFramePart>  parts;
    DxuiPaneFrameColors             colors = GetFrameColors (theme);



    if (m_indicatorArgb != theme.Accent() || m_measure != &text)
    {
        m_indicatorArgb = theme.Accent();
        m_measure       = &text;
        SyncStrip();
    }

    parts = DxuiPaneFrame::Build (GetFrameSpec());
    DxuiPaneFrame::Paint (painter, parts, DxuiPaneFramePhase::Under, colors);

    if (m_kind == Kind::ToolWindow)
    {
        PaintTitle (painter, text, theme);
    }

    if (HasStrip() && strip.bottom > strip.top)
    {
        m_strip.SetSelectedFill (theme.ContentBackground());
        m_strip.Paint (painter, text, theme);
    }

    DxuiPaneFrame::Paint (painter, parts, DxuiPaneFramePhase::Joins, colors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::PaintFrame
//
//  The frame's over parts: the gap outside the pane's rounded corners where
//  its controls fill the body, and the outline around the pane and its
//  selected tab. The dock site calls it after every sibling has painted, so
//  the outline and the corner caps lie over the pane's controls whatever the
//  child order.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::PaintFrame (IDxuiPainter & painter, const IDxuiTheme & theme) const
{
    DxuiPaneFrame::Paint (painter, DxuiPaneFrame::Build (GetFrameSpec()), DxuiPaneFramePhase::Over, GetFrameColors (theme));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetFrameSpec
//
//  The frame as the group stands: its bounds, its title bar and tab band,
//  where its selected tab shows along the band, and the corners it shares
//  with its window.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneFrameSpec DxuiTabGroup::GetFrameSpec() const
{
    DxuiPaneFrameSpec  spec;
    RECT               title = GetTitleRect();
    RECT               strip = GetStripRect();



    spec.pane           = m_boundsDip;
    spec.toolWindow     = m_kind == Kind::ToolWindow;
    spec.titlePx        = title.bottom - title.top;
    spec.bandPx         = HasStrip() ? strip.bottom - strip.top : 0;
    spec.linePx         = DxuiPaneMetrics::GetLinePx (m_scaler);
    spec.cornerPx       = DxuiPaneMetrics::GetCornerPx (m_scaler);
    spec.windowCorners  = m_windowCorners;
    spec.windowCornerPx = m_scaler.ToPx (m_windowCornerDip);

    if (spec.bandPx > 0)
    {
        spec.hasSelected = m_strip.GetSelectedSpan (spec.selLeft, spec.selRight, spec.openLeft, spec.openRight);
    }

    return spec;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetFrameColors
//
//  The outline is in the focus accent while the user works in the group,
//  and in the border color otherwise.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneFrameColors DxuiTabGroup::GetFrameColors (const IDxuiTheme & theme) const
{
    DxuiPaneFrameColors  colors;



    colors.gap     = theme.DockGap();
    colors.band    = theme.PaneBand();
    colors.content = theme.ContentBackground();
    colors.outline = m_focusedLook ? theme.FocusAccent() : theme.Border();

    return colors;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::PaintTitle
//
//  The active pane's title, from the pane's text inset, and the menu, pin and
//  close buttons at the right end, each washed in a rounded square while
//  hovered and a shade darker while pressed. The title and the glyphs take
//  the inks of a selected tab: the full foreground while the user works in
//  the group, and a step dimmer otherwise. The pin and close glyphs are a
//  tab's, and the menu's chevron is 12 by 7 px at 150%, as Visual Studio's
//  are. The title bar's fill is a part of the frame, drawn before this.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::PaintTitle (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    static constexpr float        kPressedScale = 0.82f;
    static constexpr float        kMenuGlyphDip = 8.0f;
    static constexpr int          kWashInsetDip = 3;
    static constexpr TitleButton  kButtons[]    = { TitleButton::Menu, TitleButton::Pin, TitleButton::Close };
    static const wchar_t * const  kGlyphs[]     = { s_kpszMdl2ChevronDown, s_kpszMdl2Pinned, s_kpszMdl2Cancel };
    static constexpr float        kGlyphDips[]  = { kMenuGlyphDip, DxuiTabStrip::kPinGlyphDip, DxuiTabStrip::kCloseGlyphDip };
    DxuiFontHandle                font          = theme.BodyFont();
    RECT                          title         = GetTitleRect();
    float                         pad           = (float) DxuiPaneMetrics::GetTextInsetPx (m_scaler);
    float                         height        = (float) (title.bottom - title.top);
    float                         washInset     = (float) m_scaler.ToPx (kWashInsetDip);
    float                         washCorner    = (float) m_scaler.ToPx (DxuiPaneMetrics::kCornerDip);
    uint32_t                      hover         = (theme.Foreground() & 0x00FFFFFFu) | 0x14000000u;
    long                          textRight     = title.right;
    const Tab                   * active        = (m_active >= 0 && m_active < (int) m_tabs.size()) ? &m_tabs[(size_t) m_active] : nullptr;
    HRESULT                       hr            = S_OK;



    for (size_t i = 0; i < std::size (kButtons); i++)
    {
        RECT  r = GetTitleButtonRect (kButtons[i]);

        if (r.right <= r.left)
        {
            continue;
        }

        textRight = std::min (textRight, r.left);

        if (m_hoverButton == (int) kButtons[i])
        {
            painter.FillRoundedRect ((float) r.left + washInset, (float) r.top + washInset,
                                     (float) (r.right - r.left) - 2.0f * washInset, (float) (r.bottom - r.top) - 2.0f * washInset, washCorner,
                                     (m_pressButton == (int) kButtons[i]) ? DxuiColor::Darken (hover, kPressedScale) : hover);
        }

        hr = text.DrawString (kGlyphs[i], (float) r.left, (float) r.top, (float) (r.right - r.left), (float) (r.bottom - r.top),
                              DxuiTabStrip::GetGlyphInk (theme, m_focusedLook), m_scaler.ToPxf (kGlyphDips[i]), L"Segoe MDL2 Assets",
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (active == nullptr)
    {
        return;
    }

    if (active->titleExtraDip > 0)
    {
        textRight = std::min (textRight, GetTitleExtraRect().left);
    }

    hr = text.DrawString (active->title.c_str(), (float) title.left + pad, (float) title.top,
                          std::max (0.0f, (float) textRight - (float) title.left - pad), height,
                          DxuiTabStrip::GetLabelInk (theme, m_focusedLook), m_scaler.ToPxf (font.sizeDip), font.face,
                          DxuiTextHAlign::Left, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::OnMouse
//
//  The title bar's buttons act when the button comes up over the one it went
//  down on. A press elsewhere on the title bar arms a drag of the active pane,
//  reported once it passes the drag distance. Everything on the tabs is the
//  strip's: a press shows the tab at once, as Visual Studio does, and a drag
//  hands the tab to the dock.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::OnMouse (const DxuiMouseEvent & ev)
{
    POINT  p       = ev.positionDip;
    int    button  = GetTitleButtonAt (p);
    RECT   title   = GetTitleRect();
    RECT   strip   = GetStripRect();
    bool   onTitle = p.x >= title.left && p.x < title.right && p.y >= title.top && p.y < title.bottom;
    bool   onStrip = HasStrip() && p.x >= strip.left && p.x < strip.right && p.y >= strip.top && p.y < strip.bottom;
    bool   left    = ev.button == DxuiMouseButton::Left;
    int    pressed = m_pressButton;
    int    hit     = -1;
    bool   handled = false;



    switch (ev.kind)
    {
    case DxuiMouseEventKind::Leave:
        m_hoverButton = -1;
        m_strip.SetMouseHover (INT_MIN / 2, INT_MIN / 2);
        return false;

    case DxuiMouseEventKind::Move:
        if (m_titlePressed)
        {
            if (!m_titleDragged && (std::abs (p.x - m_pressedAt.x) > m_scaler.ToPx (kDragDip) || std::abs (p.y - m_pressedAt.y) > m_scaler.ToPx (kDragDip)))
            {
                m_titleDragged = true;

                if (m_onTitleDrag && m_active >= 0)
                {
                    m_onTitleDrag (m_active, p);
                }
                else if (m_onDragStart && m_active >= 0)
                {
                    m_onDragStart (m_active, p);
                }
            }

            return true;
        }

        m_hoverButton = button;

        if (m_pressButton >= 0)
        {
            return true;
        }

        return m_strip.OnMouse (ev);

    case DxuiMouseEventKind::Down:
        if (!left)
        {
            return false;
        }

        if (button >= 0)
        {
            m_pressButton = button;
            return true;
        }

        if (onTitle)
        {
            m_titlePressed = true;
            m_titleDragged = false;
            m_pressedAt    = p;
            return true;
        }

        if (!onStrip)
        {
            return false;
        }

        hit = HitTestTab (p);
        (void) m_strip.OnMouse (ev);

        if (hit >= 0 && m_strip.IsInteracting())
        {
            SetActive (hit);
        }

        return true;

    case DxuiMouseEventKind::Up:
        if (!left)
        {
            return false;
        }

        if (pressed >= 0)
        {
            m_pressButton = -1;

            if (button == pressed && m_onTitleButton)
            {
                m_onTitleButton ((TitleButton) pressed, m_active, p);
            }

            return true;
        }

        if (m_titlePressed)
        {
            m_titlePressed = false;
            m_titleDragged = false;
            return true;
        }

        handled = m_strip.OnMouse (ev);
        RunPending();
        return handled;

    case DxuiMouseEventKind::Wheel:
        return onStrip && m_strip.OnMouse (ev);

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::OnKey (const DxuiKeyEvent & ev)
{
    int  count = (int) m_tabs.size();



    if (ev.kind != DxuiKeyEventKind::Down || ev.vk != VK_TAB || !ev.ctrl || count < 2)
    {
        return false;
    }

    SetActive ((m_active + (ev.shift ? count - 1 : 1)) % count);
    return true;
}

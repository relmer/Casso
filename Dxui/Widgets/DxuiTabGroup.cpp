#include "Pch.h"

#include "Widgets/DxuiTabGroup.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Theme/IDxuiTheme.h"
#include "Theme/DxuiColor.h"
#include "Core/UnicodeSymbols.h"





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
//  DxuiTabGroup::HasStrip
//
//  A document group always shows its tabs. A tool window shows them only
//  when there is a choice to make or a + to offer: a single pane's title bar
//  says all a tab would.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiTabGroup::HasStrip() const
{
    if (m_kind == Kind::Document)
    {
        return true;
    }

    return m_tabs.size() > 1 || (m_newTab && m_newTabShown && m_newTabShown (*this));
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
//  Along a document group's top, or along a tool window's bottom below its
//  title bar; empty along the bottom when the group shows no tabs.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetStripRect() const
{
    long  height = HasStrip() ? m_scaler.ToPx (kStripDip) : 0;
    long  title  = GetTitleRect().bottom;



    if (m_kind == Kind::Document)
    {
        return RECT { m_boundsDip.left, m_boundsDip.top, m_boundsDip.right, std::min (m_boundsDip.bottom, m_boundsDip.top + height) };
    }

    return RECT { m_boundsDip.left, std::max (title, m_boundsDip.bottom - height), m_boundsDip.right, m_boundsDip.bottom };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::GetBodyRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetBodyRect() const
{
    RECT  strip = GetStripRect();



    if (m_kind == Kind::Document)
    {
        return RECT { m_boundsDip.left, strip.bottom, m_boundsDip.right, m_boundsDip.bottom };
    }

    return RECT { m_boundsDip.left, GetTitleRect().bottom, m_boundsDip.right, strip.top };
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
//  pin, then the menu, as Visual Studio orders them.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiTabGroup::GetTitleButtonRect (TitleButton button) const
{
    RECT  title = GetTitleRect();
    long  size  = m_scaler.ToPx (kTitleButtonDip);
    long  slot  = 0;
    long  right = 0;
    long  top   = title.top + ((title.bottom - title.top) - size) / 2;



    if (m_kind != Kind::ToolWindow || (button == TitleButton::Close && !IsCloseShown()))
    {
        return RECT {};
    }

    slot  = (button == TitleButton::Close) ? 0 : (button == TitleButton::Pin) ? 1 : 2;
    slot -= (button != TitleButton::Close && !IsCloseShown()) ? 1 : 0;
    right = title.right - m_scaler.ToPx (1) - slot * size;

    return RECT { right - size, top, right, top + size };
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
//  and goes with its second tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::LayoutStrip()
{
    m_strip.Layout (GetStripRect(), m_scaler);
    SyncStrip();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::SyncStrip
//
//  Hands the strip the tabs as they stand. Each is as wide as its title
//  measured by the renderer of the last paint, or before the first paint at
//  an average character width. A leading mark goes ahead of the title; an indicator, a dot in
//  the accent color, takes its place on a tab with none.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::SyncStrip()
{
    DxuiTabStrip::Style             style    = (m_kind == Kind::Document) ? DxuiTabStrip::Style::Document : DxuiTabStrip::Style::ToolWindow;
    bool                            hasClose = m_kind == Kind::Document && m_onCloseTab != nullptr;
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

    //  Set only between strip events, so neither replaces itself as it runs.
    m_strip.SetOnClose  (hasClose ? DxuiTabStrip::CloseFn ([this] (int index) { m_pendingClose = index; }) : nullptr);
    m_strip.SetOnNewTab ((m_newTab && m_newTabShown && m_newTabShown (*this)) ? DxuiTabStrip::NewTabFn ([this] { m_pendingNewTab = true; }) : nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::RunPending
//
//  A close or a + the strip reported, now that it has returned.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::RunPending()
{
    int   closing = m_pendingClose;
    bool  adding  = m_pendingNewTab;



    m_pendingClose  = -1;
    m_pendingNewTab = false;

    if (closing >= 0 && m_onCloseTab)
    {
        m_onCloseTab (closing);
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
//  The title bar of a tool window, the tabs, and, while the user is working
//  in the group, an accent border round it. The strip carries a hairline
//  along the edge it shares with the pane.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    uint32_t  frame = m_focusedLook ? theme.Accent() : theme.Border();
    RECT      strip = GetStripRect();



    if (m_indicatorArgb != theme.Accent() || m_measure != &text)
    {
        m_indicatorArgb = theme.Accent();
        m_measure       = &text;
        SyncStrip();
    }

    if (m_kind == Kind::ToolWindow)
    {
        PaintTitle (painter, text, theme);
    }

    if (HasStrip() && strip.bottom > strip.top)
    {
        m_strip.SetSelectedFill    (theme.ContentBackground());
        m_strip.SetStripFill       (theme.Background());
        m_strip.SetSelectedOutline (frame);
        m_strip.Paint (painter, text, theme);
    }

    PaintFrame (painter, frame);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::PaintFrame
//
//  A border round the pane and its title bar, in the accent color while the
//  user is working in the group. Along the tabs it runs in the strip's first
//  row -- its last, for a document -- and is broken where the selected tab
//  joins, whose own outline carries it on round the tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::PaintFrame (IDxuiPainter & painter, uint32_t argb) const
{
    float  line     = (float) std::max (1L, std::lround (m_scaler.ToPxf (1.0f)));
    RECT   strip    = GetStripRect();
    bool   hasStrip = HasStrip() && strip.bottom > strip.top;
    float  left     = (float) m_boundsDip.left;
    float  right    = (float) m_boundsDip.right;
    float  top      = (float) ((hasStrip && m_kind == Kind::Document) ? strip.bottom - (long) line : m_boundsDip.top);
    float  bottom   = (float) ((hasStrip && m_kind == Kind::ToolWindow) ? strip.top + (long) line : m_boundsDip.bottom);
    float  edgeY    = (m_kind == Kind::Document) ? top : bottom - line;
    long   joinL    = 0;
    long   joinR    = 0;



    painter.FillRect (left,         top, line, bottom - top, argb);
    painter.FillRect (right - line, top, line, bottom - top, argb);
    painter.FillRect (left, (m_kind == Kind::Document) ? bottom - line : top, right - left, line, argb);

    //  The edge along the tabs, less the selected tab's join.
    if (hasStrip && m_strip.GetJoinSpan (joinL, joinR))
    {
        painter.FillRect (left,          edgeY, std::max (0.0f, (float) joinL - left),  line, argb);
        painter.FillRect ((float) joinR, edgeY, std::max (0.0f, right - (float) joinR), line, argb);
    }
    else
    {
        painter.FillRect (left, edgeY, right - left, line, argb);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroup::PaintTitle
//
//  The active pane's title, and the menu, pin and close buttons at the right
//  end, washed while hovered and a shade darker while pressed.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiTabGroup::PaintTitle (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) const
{
    static constexpr float        kPressedScale = 0.82f;
    static constexpr float        kGlyphDip     = 10.0f;
    static constexpr TitleButton  kButtons[]    = { TitleButton::Menu, TitleButton::Pin, TitleButton::Close };
    static const wchar_t * const  kGlyphs[]     = { s_kpszMdl2ChevronDown, s_kpszMdl2Pin, s_kpszMdl2Cancel };
    DxuiFontHandle                font          = theme.BodyFont();
    RECT                          title         = GetTitleRect();
    float                         pad           = m_scaler.ToPxf ((float) kTabPadDip);
    float                         height        = (float) (title.bottom - title.top);
    uint32_t                      hover         = (theme.Foreground() & 0x00FFFFFFu) | 0x14000000u;
    long                          textRight     = title.right;
    const Tab                   * active        = (m_active >= 0 && m_active < (int) m_tabs.size()) ? &m_tabs[(size_t) m_active] : nullptr;
    HRESULT                       hr            = S_OK;



    painter.FillRect ((float) title.left, (float) title.top, (float) (title.right - title.left), height, theme.Background());

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
            painter.FillRect ((float) r.left, (float) r.top, (float) (r.right - r.left), (float) (r.bottom - r.top),
                              (m_pressButton == (int) kButtons[i]) ? DxuiColor::Darken (hover, kPressedScale) : hover);
        }

        hr = text.DrawString (kGlyphs[i], (float) r.left, (float) r.top, (float) (r.right - r.left), (float) (r.bottom - r.top),
                              theme.ForegroundMuted(), m_scaler.ToPxf (kGlyphDip), L"Segoe MDL2 Assets",
                              DxuiTextHAlign::Center, DxuiTextVAlign::Center, DxuiFontWeight::Normal, false);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (active == nullptr)
    {
        return;
    }

    hr = text.DrawString (active->title.c_str(), (float) title.left + pad, (float) title.top,
                          std::max (0.0f, (float) textRight - (float) title.left - pad), height,
                          theme.Foreground(), m_scaler.ToPxf (font.sizeDip), font.face,
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

                if (m_onDragStart && m_active >= 0)
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

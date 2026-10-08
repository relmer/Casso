#include "Pch.h"

#include "DxuiPropertySheet.h"
#include "DxuiPropertyPage.h"

#include "Render/IDxuiTextRenderer.h"
#include "Theme/DxuiTheme.h"
#include "Widgets/DxuiButton.h"
#include "Window/DxuiButtonRow.h"



static constexpr int  s_kTabStripHeightDip = 36;
static constexpr int  s_kContentPadDip     = DxuiButtonRow::kEdgePadDip;   // page inset
static constexpr int  s_kScrollbarWidthDip = 10;    // sits in the right-hand page inset
static constexpr int  s_kScrollbarInsetDip = 3;     // from the window's right edge
static constexpr int  s_kScrollMinThumbPx  = 16;
static constexpr int  s_kScrollLineDip     = 40;    // one arrow click or wheel line
static constexpr int  s_kWheelLines        = 3;     // lines per wheel notch





////////////////////////////////////////////////////////////////////////////////
//
//  OnCreate
//
//  Let the subclass add its pages, then build the tab strip (from the page
//  titles) and the OK / Cancel / Apply row. Cancel is the IDCANCEL button
//  (auto-closes); OK applies every dirty page then closes; Apply applies
//  without closing and is enabled only while a page is dirty.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::OnCreate()
{
    std::vector<DxuiTabStrip::Tab>  tabs;



    OnBuildPages();

    m_tabs = CreateChild<DxuiTabStrip>();
    BuildTabList (tabs);

    m_tabs->SetTabs    (std::move (tabs));
    m_tabs->SetSelected (0);
    // The tab strip reports a tab position; map it back to a page index since a
    // hidden page has no tab (tab position != page index once anything hides).
    m_tabs->SetOnChange ([this] (int tabIndex)
    {
        int  page = PageIndexOfTab (tabIndex);
        if (page >= 0) { SetActivePage (page); }
    });

    m_ok     = CreateChild<DxuiButton> (m_okText);
    m_cancel = CreateChild<DxuiButton> (L"Cancel");
    m_apply  = CreateChild<DxuiButton> (L"Apply");

    m_ok->SetCommandId     (IDOK);
    m_cancel->SetCommandId (IDCANCEL);
    m_apply->SetCommandId  (DxuiButtonRow::kApplyCommandId);

    // Honor a pre-Create SetApplyVisible(false): a plain [OK][Cancel] sheet.
    m_apply->SetVisible (m_applyVisible);

    // Route the row through the commit hooks. OK closes only on hr == S_OK;
    // Apply commits without closing; Cancel runs OnCancel then closes. Cancel
    // is wired explicitly (not left to the dialog auto-wire) so OnCancel fires
    // on both the button and Escape (which triggers the IDCANCEL button).
    m_ok->SetOnClick     ([this] () { if (OnOk() == S_OK) { EndDialog (IDOK); } });
    m_apply->SetOnClick  ([this] () { (void) OnApply(); RefreshApplyEnabled(); });
    m_cancel->SetOnClick ([this] () { OnCancel(); EndDialog (IDCANCEL); });

    m_scrollbar.SetOnScroll ([this] (int sbCode, int pos)
    {
        UNREFERENCED_PARAMETER (sbCode);
        SetPageScrollPos (pos);
    });

    SetActivePage      (0);
    RefreshApplyEnabled();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnApply / OnOk / OnCancel
//
//  Default commit hooks. OnApply applies every dirty page and reports S_FALSE
//  (veto -> keep open) if a page blocks; OnOk defers to OnApply; OnCancel is a
//  no-op. Subclasses override to run a cross-cutting commit / revert.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPropertySheet::OnApply()
{
    return TryApplyAllDirtyPages() ? S_OK : S_FALSE;
}


HRESULT DxuiPropertySheet::OnOk()
{
    return OnApply();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCancel
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::OnCancel()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterPage
//
//  Records a page (added via CreatePage), starts it hidden except the
//  first, routes its dirty notifications to the Apply button, and its
//  content-height changes to the scroll range.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::RegisterPage (DxuiPropertyPage * page)
{
    HRESULT  hr = S_OK;



    CBRA (page != nullptr);

    page->SetVisible                (m_pages.empty());
    page->SetOnDirtyChanged         ([this] () { RefreshApplyEnabled(); });
    page->SetOnContentHeightChanged ([this, page] () { OnPageContentHeightChanged (page); });
    page->SetOnRevealRequested      ([this, page] (const RECT & rectPx) { OnPageRevealRequested (page, rectPx); });
    m_pages.push_back (page);
    m_present.push_back (true);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetActivePage
//
//  Switches which page is showing: exactly one visible, the tab strip synced,
//  and the page notified.
//
//  Visibility is set on EVERY page rather than just the outgoing and incoming
//  ones. It costs a short loop and makes the invariant unconditional -- there
//  is no state from which two pages can both be visible, however the sheet got
//  there.
//
//  The tab index is looked up from the page index rather than assumed equal,
//  because hidden pages hold no tab and the two numberings diverge as soon as
//  one page is hidden.
//
//  OnActivated fires AFTER the visibility flip, so a page that refreshes its
//  contents on activation is already visible when it does so.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::SetActivePage (int index)
{
    HRESULT  hr        = S_OK;
    size_t   i         = 0;
    int      pageCount = 0;



    pageCount = (int) m_pages.size();
    CBRA (index >= 0 && index < pageCount);

    m_active = index;

    for (i = 0; i < m_pages.size(); ++i)
    {
        m_pages[i]->SetVisible ((int) i == index);
    }

    if (m_tabs != nullptr)
    {
        m_tabs->SetSelected (TabIndexOfPage (index));
    }

    m_pages[(size_t) index]->OnActivated();

    // Every page opens at its top. The pages share one scroll position, and
    // the previous page's offset means nothing on this one; the scroll range
    // is this page's own, since one page's content can be taller than
    // another's.
    m_scrollPosPx = 0;

    if (m_haveLayout)
    {
        LayoutPages (m_pageAreaPx, m_lastScaler);
    }

    //  THE TAB ORDER IS A SNAPSHOT AND HAS JUST GONE STALE.
    //
    //  DxuiFocusManager::Rebuild walks the visible subtree, and the only other
    //  call to it is in BeginDialogMode -- so without this the order stays
    //  whatever it was when the sheet opened: the controls of page 0, forever.
    //  Every other page's controls were absent from it, and page 0's remained
    //  in it after being hidden, which meant Tab on any other tab landed on a
    //  control the user could not see and Space then operated it. Observed as
    //  a machine dropdown opening from the Screenshots tab.
    //
    //  Focus falls back to the tab strip, which is where the user's attention
    //  already is, having just changed tabs.
    RefreshFocusOrder (m_tabs);

    Invalidate();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPageVisible / IsPageVisible
//
//  Adds or removes a page from the sheet entirely -- its tab disappears, not
//  merely its content.
//
//  This is how a page that does not apply to the current machine is dropped: a
//  Disk page on a machine with no controller should not offer a tab at all.
//
//  PRESENCE is tracked separately from the page's own visible flag, because
//  the two answer different questions. Presence means "this page exists for
//  this machine"; visibility means "this page is the one showing right now".
//  Collapsing them would make activating a page resurrect a tab that was
//  deliberately removed.
//
//  Hiding the ACTIVE page moves the selection to the first present one, so the
//  sheet is never left showing nothing.
//
//  The relayout is skipped before the first Layout, and the pending change is
//  honored when Layout first runs -- so a caller may configure page visibility
//  before the sheet has ever been laid out.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::SetPageVisible (int pageIndex, bool visible)
{
    bool  inRange = (pageIndex >= 0 && pageIndex < (int) m_present.size());
    int   first   = 0;



    // Out of range and already-in-that-state are both nothing to do. The
    // range test has to come first: it is what makes the lookup safe.
    if (inRange && m_present[(size_t) pageIndex] != visible)
    {
        m_present[(size_t) pageIndex] = visible;

        // A hidden page can neither hold a tab nor be shown.
        if (!visible)
        {
            m_pages[(size_t) pageIndex]->SetVisible (false);

            if (m_active == pageIndex)
            {
                first    = GetFirstPresentPage();
                m_active = (first >= 0) ? first : 0;
            }
        }

        // Relayout so the strip drops / regrows the tab and the active page
        // fills the content area. Before the first Layout there is nothing to
        // reflow; the pending change is honored when Layout first runs.
        if (m_haveLayout)
        {
            Layout (m_lastBoundsPx, m_lastScaler);
        }

        SetActivePage (m_active);
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPageVisible
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPropertySheet::IsPageVisible (int pageIndex) const
{
    // Short-circuit order is load-bearing: the range tests guard the lookup.
    return (pageIndex >= 0
            && pageIndex < (int) m_present.size()
            && m_present[(size_t) pageIndex]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FirstPresentPage / TabIndexOfPage / PageIndexOfTab / BuildTabList
//
//  A page is "present" when it has a tab. Because hidden pages are skipped, a
//  page index (registration order) differs from its tab position once any page
//  is hidden -- these map between the two spaces.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPropertySheet::IndexOfPage (const DxuiPropertyPage * page) const
{
    int  found = -1;
    int  i     = 0;



    for (i = 0; i < (int) m_pages.size() && found < 0; ++i)
    {
        if (m_pages[(size_t) i] == page) { found = i; }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFirstPresentPage
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPropertySheet::GetFirstPresentPage() const
{
    int  found = -1;
    int  i     = 0;



    for (i = 0; i < (int) m_pages.size() && found < 0; ++i)
    {
        if (m_present[(size_t) i]) { found = i; }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TabIndexOfPage
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPropertySheet::TabIndexOfPage (int pageIndex) const
{
    int  found = -1;
    int  tab   = 0;
    int  i     = 0;



    for (i = 0; i < (int) m_pages.size() && found < 0; ++i)
    {
        if (!m_present[(size_t) i]) { continue; }

        if (i == pageIndex) { found = tab; }
        else                { ++tab;       }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PageIndexOfTab
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPropertySheet::PageIndexOfTab (int tabIndex) const
{
    int  found = -1;
    int  tab   = 0;
    int  i     = 0;



    for (i = 0; i < (int) m_pages.size() && found < 0; ++i)
    {
        if (!m_present[(size_t) i]) { continue; }

        if (tab == tabIndex) { found = i; }
        else                 { ++tab;     }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildTabList
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::BuildTabList (std::vector<DxuiTabStrip::Tab> & out) const
{
    int  i = 0;



    out.clear();
    out.reserve (m_pages.size());
    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        DxuiTabStrip::Tab  tab;

        if (!m_present[(size_t) i]) { continue; }

        tab.label = m_pages[(size_t) i]->GetTitle();
        out.push_back (std::move (tab));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDialogTabSwitch
//
//  Ctrl+Tab / Ctrl+Shift+Tab: cycle to the next / previous page, wrapping.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPropertySheet::OnDialogTabSwitch (bool backward)
{
    std::vector<int>  present;
    int               i      = 0;
    int               pos    = 0;
    int               count  = 0;
    bool              cycled = false;



    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        if (m_present[(size_t) i]) { present.push_back (i); }
    }

    count = (int) present.size();

    // A single page has nowhere to cycle to, so leave the key unhandled and
    // let the dialog do whatever it does with an unclaimed Ctrl+Tab.
    if (count > 1)
    {
        // Cycle among present pages only (a hidden page has no tab to land on).
        for (i = 0; i < count; ++i)
        {
            if (present[(size_t) i] == m_active) { pos = i; break; }
        }

        pos = backward ? (pos - 1 + count) % count
                       : (pos + 1) % count;

        SetActivePage (present[(size_t) pos]);
        cycled = true;
    }

    return cycled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshApplyEnabled
//
//  Apply is enabled iff at least one page is dirty.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::RefreshApplyEnabled()
{
    bool  anyDirty = false;



    for (DxuiPropertyPage * page : m_pages)
    {
        if (page->IsDirty())
        {
            anyDirty = true;
            break;
        }
    }

    if (m_apply != nullptr)
    {
        m_apply->SetEnabled (anyDirty);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryApplyAllDirtyPages
//
//  Commits every dirty page in order. A page whose OnApply() returns false
//  blocks the operation: that page becomes active and the method returns
//  false (so OK does not close). A veto is a supported outcome, not an
//  error -- hence the Try name and bool rather than an HRESULT. On success
//  each committed page is marked clean and Apply is disabled.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPropertySheet::TryApplyAllDirtyPages()
{
    bool    ok = true;
    size_t  i  = 0;



    for (i = 0; i < m_pages.size(); ++i)
    {
        DxuiPropertyPage * page = m_pages[i];

        if (!page->IsDirty())
        {
            continue;
        }

        if (!page->OnApply())
        {
            SetActivePage ((int) i);
            ok = false;
            break;
        }

        page->MarkDirty (false);
    }

    if (ok)
    {
        RefreshApplyEnabled();
    }

    return ok;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutTabRects
//
//  Sizes each tab to its label: the measured label width plus kTabPadXDip
//  on both sides, rounded up to whole pixels and held to at least
//  kTabMinWidthDip. Visible tabs run edge to edge from the strip's left pad
//  and span the strip's full height; a hidden tab gets an empty rect and
//  moves nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::LayoutTabRects (
    const RECT                     & stripPx,
    const DxuiDpiScaler            & scaler,
    std::span<const DxuiSheetTab>    tabs,
    std::span<RECT>                  outRects)
{
    int  minW = scaler.ToPx (kTabMinWidthDip);
    int  padX = scaler.ToPx (kTabPadXDip);
    int  x    = stripPx.left + scaler.ToPx (s_kContentPadDip);
    int  w    = 0;
    int  i    = 0;



    for (i = 0; i < (int) tabs.size(); ++i)
    {
        const DxuiSheetTab  & tab = tabs[(size_t) i];

        outRects[(size_t) i] = {};

        if (!tab.isVisible)
        {
            continue;
        }

        w = (int) std::ceil (scaler.ToPxf (tab.labelWidthDip)) + padX * 2;
        w = std::max (w, minW);

        outRects[(size_t) i] = { x, stripPx.top, x + w, stripPx.bottom };
        x += w;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureTabLabelDip
//
//  Measures a tab label in the font DxuiTabStrip draws it in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiPropertySheet::MeasureTabLabelDip (
    IDxuiTextRenderer   & text,
    const std::wstring  & label,
    float               & outWidthDip)
{
    HRESULT  hr        = S_OK;
    float    heightDip = 0.0f;



    outWidthDip = 0.0f;

    hr = text.MeasureString (label.c_str(), DxuiTabStrip::kLabelFontDip, DxuiTheme::GetUiFace(), outWidthDip, heightDip);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MeasureTabs
//
//  One entry per page, in registration order: the page's label width (zero
//  when there is no text renderer yet, or the measure fails, which leaves
//  the tab at its minimum width) and whether the page has a tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::MeasureTabs (std::vector<DxuiSheetTab> & out) const
{
    HRESULT              hr   = S_OK;
    IDxuiTextRenderer  * text = GetTextRenderer();
    int                  i    = 0;



    out.assign (m_pages.size(), DxuiSheetTab {});

    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        DxuiSheetTab  & tab = out[(size_t) i];

        tab.isVisible = m_present[(size_t) i];

        if (text == nullptr || !tab.isVisible)
        {
            continue;
        }

        hr = MeasureTabLabelDip (*text, m_pages[(size_t) i]->GetTitle(), tab.labelWidthDip);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  The tab strip occupies a fixed top strip; the OK / Cancel / Apply row a
//  fixed bottom strip (right-aligned, registration order left-to-right);
//  the active page fills the inset remainder. The host lays the window out
//  in physical pixels below its own caption.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int   pad     = scaler.ToPx (s_kContentPadDip);
    int   tabH    = scaler.ToPx (s_kTabStripHeightDip);
    int   rowH    = scaler.ToPx (DxuiButtonRow::kRowHeightDip);
    RECT  page    = boundsPx;
    int   i       = 0;



    SetBounds (boundsPx);

    // Remember the last Layout inputs so SetPageVisible can reflow the strip
    // live (drop / regrow a tab) without waiting for the next window resize.
    m_lastBoundsPx = boundsPx;
    m_lastScaler   = scaler;
    m_haveLayout   = true;

    if (m_tabs != nullptr)
    {
        RECT                            strip   = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + tabH };
        std::vector<DxuiTabStrip::Tab>  tabs;
        std::vector<DxuiSheetTab>       measure;
        std::vector<RECT>               rects;
        int                             tab     = 0;



        // DxuiTabStrip does not lay out its own tabs -- the caller owns each
        // tab's rect. Size each to its label; only present pages get a tab.
        BuildTabList (tabs);
        MeasureTabs  (measure);
        rects.resize (measure.size());
        LayoutTabRects (strip, scaler, measure, rects);

        for (i = 0; i < (int) measure.size(); ++i)
        {
            if (!measure[(size_t) i].isVisible)
            {
                continue;
            }

            tabs[(size_t) tab].rect = rects[(size_t) i];
            ++tab;
        }

        m_tabs->SetTabs     (std::move (tabs));
        m_tabs->SetSelected (TabIndexOfPage (m_active));
        m_tabs->Layout      (strip, scaler);
        m_tabs->SetDpi      (scaler.GetDpi());
    }

    page.top     = boundsPx.top + tabH;
    page.bottom -= rowH;
    page.left   += pad;
    page.top    += pad;
    page.right  -= pad;
    page.bottom -= pad;

    LayoutPages (page, scaler);

    // Button row, right-aligned along the bottom in the canonical order
    // (OK, Cancel, Apply; Apply omitted when hidden). OK may carry a custom
    // width so a longer label ("OK (reboot)") fits without clipping. The
    // stable sort by StandardRank enforces the standard order regardless of
    // how the buttons happen to be registered.
    {
        struct Slot { DxuiButton * btn; int widthDip; int commandId; };

        int   okW    = (m_okWidthDip > 0) ? m_okWidthDip : DxuiButtonRow::kButtonWidthDip;
        Slot  all[3] = { { m_ok,     okW,                            IDOK },
                         { m_cancel, DxuiButtonRow::kButtonWidthDip,  IDCANCEL },
                         { m_apply,  DxuiButtonRow::kButtonWidthDip,  DxuiButtonRow::kApplyCommandId } };
        Slot  vis[3]   = {};
        int   visW[3]  = {};
        RECT  rects[3] = {};
        int   n        = 0;

        for (i = 0; i < 3; ++i)
        {
            if (all[i].btn == nullptr)                    { continue; }
            if (all[i].btn == m_apply && !m_applyVisible) { continue; }

            vis[n++] = all[i];
        }

        std::stable_sort (vis, vis + n, [] (const Slot & a, const Slot & b)
                          {
                              return DxuiButtonRow::GetStandardRank (a.commandId) <
                                     DxuiButtonRow::GetStandardRank (b.commandId);
                          });

        for (i = 0; i < n; ++i)
        {
            visW[i] = vis[i].widthDip;
        }

        DxuiButtonRow::LayoutRightGroup (boundsPx, scaler,
                                         std::span<const int> (visW,  (size_t) n),
                                         std::span<RECT>      (rects, (size_t) n));

        for (i = 0; i < n; ++i)
        {
            vis[i].btn->Layout (rects[i]);
            vis[i].btn->SetDpi  (scaler.GetDpi());
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutPages
//
//  Lays out every page, not just the active one: SetActivePage only toggles
//  visibility (no relayout), so a page that has never been the active page
//  at Layout time would otherwise keep default {0,0} bounds and render
//  clipped at the top-left when first shown.
//
//  A window shorter than its design height gives the pages the height they
//  would have had at that size, shifted up by the scroll position, and shows
//  them through a viewport running from the tab strip to the button row. The
//  viewport spans the page insets as well as the page, so a control near an
//  edge keeps its focus rectangle; the scrollbar takes the right-hand inset.
//
//  A page that reports a content height taller than that is given its own
//  height and scrolls the same way. A page learns its height only by being
//  laid out, so when the first pass changes the scroll range, the pages are
//  placed a second time at the corrected position.
//
//  Each page is also told how wide its rect would be at the design width,
//  for a page that stretches with a wider window.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::LayoutPages (const RECT & pageAreaPx, const DxuiDpiScaler & scaler)
{
    int   pad        = scaler.ToPx (s_kContentPadDip);
    int   barW       = scaler.ToPx (s_kScrollbarWidthDip);
    int   barInset   = scaler.ToPx (s_kScrollbarInsetDip);
    int   viewH      = pageAreaPx.bottom - pageAreaPx.top;
    int   deficit    = 0;
    int   placedPos  = 0;
    bool  wasScroll  = false;
    RECT  client     = {};



    m_pageAreaPx        = pageAreaPx;
    m_designPageWidthPx = 0;

    if (m_designHeightDip > 0 && IsCreated() && GetClientRect (GetHwnd(), &client) != FALSE)
    {
        deficit = scaler.ToPx (m_designHeightDip) - (client.bottom - client.top);
    }

    // The page area is the client less fixed insets, so at the design width
    // it is narrower by the same amount the client is.
    if (m_designWidthDip > 0 && IsCreated() && GetClientRect (GetHwnd(), &client) != FALSE)
    {
        m_designPageWidthPx = (pageAreaPx.right - pageAreaPx.left) - ((client.right - client.left) - scaler.ToPx (m_designWidthDip));
    }

    m_designContentPx = viewH + std::max (deficit, 0);
    m_viewportPx      = { m_lastBoundsPx.left,
                          pageAreaPx.top    - pad,
                          m_lastBoundsPx.right - barW - barInset,
                          pageAreaPx.bottom + pad };

    m_isLayingOutPages = true;

    UpdateScrollRange();
    placedPos = m_scrollPosPx;
    wasScroll = m_scrollable;
    PlacePages (scaler);

    UpdateScrollRange();

    if (m_scrollPosPx != placedPos || m_scrollable != wasScroll)
    {
        PlacePages (scaler);
    }

    m_isLayingOutPages = false;

    ConfigureScrollbar (scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlacePages
//
//  Lays every page out at the current scroll position, each at the design
//  content height or its own content height, whichever is taller.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::PlacePages (const DxuiDpiScaler & scaler)
{
    RECT    pageRect = m_pageAreaPx;
    size_t  i        = 0;



    pageRect.top = m_pageAreaPx.top - m_scrollPosPx;

    for (i = 0; i < m_pages.size(); ++i)
    {
        pageRect.bottom = pageRect.top + std::max (m_designContentPx, m_pages[i]->GetContentHeightPx());

        m_pages[i]->SetViewport      (m_scrollable ? &m_viewportPx : nullptr);
        m_pages[i]->SetDesignWidthPx (m_designPageWidthPx);
        m_pages[i]->Layout           (pageRect, scaler);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateScrollRange
//
//  The range is the active page's: the design content height, or the page's
//  own when that is taller. The position is clamped to it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::UpdateScrollRange()
{
    int  viewH    = m_pageAreaPx.bottom - m_pageAreaPx.top;
    int  activePx = 0;



    if (m_active >= 0 && m_active < (int) m_pages.size())
    {
        activePx = m_pages[(size_t) m_active]->GetContentHeightPx();
    }

    m_contentPx   = std::max (m_designContentPx, activePx);
    m_scrollable  = (m_contentPx > viewH);
    m_scrollPosPx = m_scrollable ? ClampScrollPos (m_scrollPosPx, m_contentPx, viewH) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigureScrollbar
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::ConfigureScrollbar (const DxuiDpiScaler & scaler)
{
    int             barW     = scaler.ToPx (s_kScrollbarWidthDip);
    int             barInset = scaler.ToPx (s_kScrollbarInsetDip);
    int             viewH    = m_pageAreaPx.bottom - m_pageAreaPx.top;
    DxuiScrollInfo  info;



    m_scrollbar.Configure (DxuiScrollbar::Orientation::Vertical, barW, s_kScrollMinThumbPx,
                           scaler.ToPx (s_kScrollLineDip));
    m_scrollbar.SetTrack  (RECT { m_lastBoundsPx.right - barW - barInset, m_viewportPx.top,
                                  m_lastBoundsPx.right - barInset,        m_viewportPx.bottom });

    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin  = 0;
    info.nMax  = m_scrollable ? m_contentPx : 0;
    info.nPage = (UINT) viewH;
    info.nPos  = m_scrollPosPx;
    m_scrollbar.SetScrollInfo (info);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPageContentHeightChanged
//
//  The active page grew or shrank outside a sheet layout: rows added, or a
//  section sliding open. The scroll range follows, and the pages are placed
//  again only when that moved the scroll position or turned scrolling on or
//  off. A change reported from inside LayoutPages is picked up there.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::OnPageContentHeightChanged (const DxuiPropertyPage * page)
{
    int   oldPos    = m_scrollPosPx;
    bool  wasScroll = m_scrollable;



    if (m_isLayingOutPages || !m_haveLayout || IndexOfPage (page) != m_active)
    {
        return;
    }

    UpdateScrollRange();

    if (m_scrollPosPx != oldPos || m_scrollable != wasScroll)
    {
        m_isLayingOutPages = true;
        PlacePages (m_lastScaler);
        m_isLayingOutPages = false;
    }

    ConfigureScrollbar (m_lastScaler);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnPageRevealRequested
//
//  The active page asked to show part of itself. Taken only outside a sheet
//  layout, since a reveal scrolls by laying the pages out again.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::OnPageRevealRequested (const DxuiPropertyPage * page, const RECT & rectPx)
{
    if (m_isLayingOutPages || !m_scrollable || IndexOfPage (page) != m_active)
    {
        return;
    }

    SetPageScrollPos (GetScrollPosToReveal (m_scrollPosPx, rectPx, m_viewportPx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetPageScrollPos
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::SetPageScrollPos (int posPx)
{
    int  viewH   = m_pageAreaPx.bottom - m_pageAreaPx.top;
    int  clamped = ClampScrollPos (posPx, m_contentPx, viewH);



    if (!m_scrollable || !m_haveLayout || clamped == m_scrollPosPx)
    {
        return;
    }

    m_scrollPosPx = clamped;
    LayoutPages (m_pageAreaPx, m_lastScaler);
    Invalidate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClampScrollPos
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPropertySheet::ClampScrollPos (int posPx, int contentPx, int viewportPx)
{
    int  maxPos = std::max (contentPx - viewportPx, 0);



    return std::clamp (posPx, 0, maxPos);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetScrollPosToReveal
//
//  Scrolling by d moves the target up by d, so a target below the viewport
//  needs d = bottom overshoot and one above it d = -(top undershoot). The top
//  edge wins for a target taller than the viewport, since that is where a
//  control's label and focus rectangle start.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPropertySheet::GetScrollPosToReveal (int posPx, const RECT & targetPx, const RECT & viewportPx)
{
    int  result = posPx;



    if (targetPx.bottom > viewportPx.bottom)
    {
        result = posPx + (targetPx.bottom - viewportPx.bottom);
    }

    if (targetPx.top - (result - posPx) < viewportPx.top)
    {
        result = posPx - (viewportPx.top - targetPx.top);
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeMaxClientSizePx
//
//  The client is the caption, the tab strip, the page inset, the content,
//  the inset again and the button row from top to bottom, and the inset,
//  the content and the inset across. The design size is the floor.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiPropertySheet::ComputeMaxClientSizePx (
    const SIZE           & designPx,
    const SIZE           & contentPx,
    int                    captionPx,
    const DxuiDpiScaler  & scaler)
{
    int  pad      = scaler.ToPx (s_kContentPadDip);
    int  tabH     = scaler.ToPx (s_kTabStripHeightDip);
    int  rowH     = scaler.ToPx (DxuiButtonRow::kRowHeightDip);
    int  contentW = pad + contentPx.cx + pad;
    int  contentH = captionPx + tabH + pad + contentPx.cy + pad + rowH;



    return SIZE { std::max (designPx.cx, (LONG) contentW), std::max (designPx.cy, (LONG) contentH) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMaxClientSizePx
//
//  Over every page with a tab, as each was last laid out. The caption is
//  whatever the host put above the bounds the sheet was laid out in.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiPropertySheet::GetMaxClientSizePx() const
{
    SIZE    designPx  = {};
    SIZE    contentPx = {};
    size_t  i         = 0;



    designPx.cx = m_designWidthDip  > 0 ? m_lastScaler.ToPx (m_designWidthDip)  : 0;
    designPx.cy = m_designHeightDip > 0 ? m_lastScaler.ToPx (m_designHeightDip) : 0;

    for (i = 0; i < m_pages.size(); ++i)
    {
        if (!m_present[i])
        {
            continue;
        }

        contentPx.cx = std::max (contentPx.cx, (LONG) m_pages[i]->GetContentWidthPx());
        contentPx.cy = std::max (contentPx.cy, (LONG) m_pages[i]->GetContentHeightPx());
    }

    return ComputeMaxClientSizePx (designPx, contentPx, m_lastBoundsPx.top, m_lastScaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetMaxClientSizePx
//
//  None until the pages have been laid out and reported their extents.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPropertySheet::TryGetMaxClientSizePx (SIZE & outSizePx) const
{
    if (!m_haveLayout)
    {
        return false;
    }

    outSizePx = GetMaxClientSizePx();
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPage / FindOwningPage
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPropertySheet::IsPage (const IDxuiControl * ctl) const
{
    return std::find (m_pages.begin(), m_pages.end(), ctl) != m_pages.end();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindOwningPage
//
////////////////////////////////////////////////////////////////////////////////

DxuiPropertyPage * DxuiPropertySheet::FindOwningPage (const IDxuiControl * ctl) const
{
    const IDxuiControl  * node  = ctl;
    DxuiPropertyPage    * found = nullptr;
    size_t                i     = 0;



    for ( ; node != nullptr && found == nullptr; node = node->GetParent())
    {
        for (i = 0; i < m_pages.size() && found == nullptr; ++i)
        {
            if (m_pages[i] == node) { found = m_pages[i]; }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
//  The DxuiPanel fan-out, except that a scrolled page paints clipped to the
//  viewport, both its shapes and its text, and the scrollbar paints last.
//  Clipping here rather than in the page covers the pages that override
//  Paint themselves.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT         hr      = S_OK;
    IDxuiControl  * child   = nullptr;
    bool            clipped = false;
    size_t          i       = 0;



    for (i = 0; i < GetChildCount(); ++i)
    {
        child = GetChild (i);

        if (child == nullptr || !child->IsVisible())
        {
            continue;
        }

        clipped = m_scrollable && IsPage (child);

        if (clipped)
        {
            painter.SetClipRect (&m_viewportPx);

            hr = text.PushClipRect ((float) m_viewportPx.left,
                                    (float) m_viewportPx.top,
                                    (float) (m_viewportPx.right  - m_viewportPx.left),
                                    (float) (m_viewportPx.bottom - m_viewportPx.top));
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        child->Paint (painter, text, theme);

        if (clipped)
        {
            hr = text.PopClipRect();
            IGNORE_RETURN_VALUE (hr, S_OK);

            painter.SetClipRect (nullptr);
        }
    }

    if (m_scrollable)
    {
        m_scrollbar.Paint (painter, theme.Foreground());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouse
//
//  The scrollbar first, then the DxuiPanel front-to-back fan-out, except that
//  a press or a wheel turn outside the viewport skips a scrolled page: its
//  controls are laid out under the tab strip and the button row there, but
//  not drawn. Moves and releases still reach it, so a slider dragged past the
//  viewport keeps tracking. A wheel turn no control takes scrolls the page.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPropertySheet::OnMouse (const DxuiMouseEvent & ev)
{
    IDxuiControl  * child     = nullptr;
    bool            consumed  = false;
    bool            hitsPoint = (ev.kind == DxuiMouseEventKind::Down || ev.kind == DxuiMouseEventKind::Wheel);
    bool            inView    = PtInRect (&m_viewportPx, ev.positionDip) != FALSE;
    size_t          i         = GetChildCount();
    int             lineStep  = m_lastScaler.ToPx (s_kScrollLineDip);



    if (m_scrollable && m_scrollbar.IsDragging())
    {
        if (ev.kind == DxuiMouseEventKind::Move) { return m_scrollbar.OnMouseMove (ev.positionDip.x, ev.positionDip.y); }
        if (ev.kind == DxuiMouseEventKind::Up)   { return m_scrollbar.OnMouseUp(); }
    }

    if (m_scrollable && ev.kind == DxuiMouseEventKind::Down && m_scrollbar.HitTest (ev.positionDip.x, ev.positionDip.y))
    {
        return m_scrollbar.OnMouseDown (ev.positionDip.x, ev.positionDip.y);
    }

    while (!consumed && i > 0)
    {
        child = GetChild (--i);

        if (child == nullptr || !child->IsVisible() || !child->IsEnabled())
        {
            continue;
        }

        if (m_scrollable && hitsPoint && !inView && IsPage (child))
        {
            continue;
        }

        consumed = child->OnMouse (ev);
    }

    if (!consumed && m_scrollable && inView && ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal)
    {
        SetPageScrollPos (m_scrollPosPx - (int) (ev.wheelDelta * (float) (s_kWheelLines * lineStep)));
        consumed = true;
    }

    return consumed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCursorForPoint
//
//  As OnMouse: outside the viewport a scrolled page has no say, or a text
//  field laid out under the tab strip would show its I-beam there.
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DxuiPropertySheet::GetCursorForPoint (POINT clientPx) const
{
    const IDxuiControl  * child  = nullptr;
    LPCWSTR               cursor = nullptr;
    bool                  inView = PtInRect (&m_viewportPx, clientPx) != FALSE;
    size_t                i      = GetChildCount();



    while (cursor == nullptr && i > 0)
    {
        child = GetChild (--i);

        if (child == nullptr || !child->IsVisible())
        {
            continue;
        }

        if (m_scrollable && !inView && IsPage (child))
        {
            continue;
        }

        cursor = child->GetCursorForPoint (clientPx);
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDialogKeyHandled
//
//  Tab or an arrow key can move focus to a control scrolled out of view.
//  Scroll just far enough to show it.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::OnDialogKeyHandled (IDxuiControl * focused)
{
    if (!m_scrollable || focused == nullptr || FindOwningPage (focused) == nullptr)
    {
        return;
    }

    SetPageScrollPos (GetScrollPosToReveal (m_scrollPosPx, focused->GetBounds(), m_viewportPx));
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetApplyVisible / SetOkText / SetOkWidthDip
//
//  Button-row customization. The button-property updates take effect
//  immediately; the row's positions reflow on the next Layout (i.e. the
//  next resize), so callers set visibility / OK width before Show and use
//  the OK relabel only within a pre-sized button.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::SetApplyVisible (bool visible)
{
    m_applyVisible = visible;
    if (m_apply != nullptr)
    {
        m_apply->SetVisible (visible);
    }

    if (IsCreated())
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOkText
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::SetOkText (std::wstring text)
{
    m_okText = std::move (text);
    if (m_ok != nullptr)
    {
        m_ok->SetLabel (m_okText);
    }

    if (IsCreated())
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOkWidthDip
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::SetOkWidthDip (int widthDip)
{
    m_okWidthDip = widthDip;
    if (IsCreated())
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutButtonRow
//
//  Pure helper: right-aligns a row of `widthsDip.size()` buttons (registration
//  order, left to right) along the bottom edge of boundsPx and writes each
//  button's pixel rect into outRects. All spacing is resolved from the shared
//  DIP constants through the scaler, so hidden-Apply / custom-width reflow is
//  identical whether it runs from Layout or a unit test.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::LayoutButtonRow (
    const RECT           & boundsPx,
    const DxuiDpiScaler  & scaler,
    std::span<const int>   widthsDip,
    std::span<RECT>        outRects)
{
    DxuiButtonRow::LayoutRightGroup (boundsPx, scaler, widthsDip, outRects);
}

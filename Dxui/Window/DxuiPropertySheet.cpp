#include "Pch.h"

#include "DxuiPropertySheet.h"
#include "DxuiPropertyPage.h"

#include "Widgets/DxuiButton.h"
#include "Window/DxuiButtonRow.h"



static constexpr int  s_kTabStripHeightDip = 36;
static constexpr int  s_kTabWidthDip       = 100;
static constexpr int  s_kContentPadDip     = DxuiButtonRow::kEdgePadDip;   // page inset




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

void DxuiPropertySheet::OnCreate ()
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

HRESULT DxuiPropertySheet::OnApply ()
{
    return ApplyAllDirtyPages() ? S_OK : S_FALSE;
}


HRESULT DxuiPropertySheet::OnOk ()
{
    return OnApply();
}


void DxuiPropertySheet::OnCancel ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  RegisterPage
//
//  Records a page (added via CreatePage), starts it hidden except the
//  first, and routes its dirty notifications to the Apply button.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::RegisterPage (DxuiPropertyPage * page)
{
    HRESULT  hr = S_OK;


    CBRA (page != nullptr);

    page->SetVisible        (m_pages.empty());
    page->SetOnDirtyChanged ([this] () { RefreshApplyEnabled(); });
    m_pages.push_back (page);
    m_present.push_back (true);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetActivePage
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::SetActivePage (int index)
{
    HRESULT  hr = S_OK;
    size_t   i  = 0;


    CBRA (index >= 0 && index < (int) m_pages.size());

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
    Invalidate();

Error:
    return;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetPageVisible / IsPageVisible
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::SetPageVisible (int pageIndex, bool visible)
{
    if (pageIndex < 0 || pageIndex >= (int) m_present.size())
    {
        return;
    }
    if (m_present[(size_t) pageIndex] == visible)
    {
        return;
    }

    m_present[(size_t) pageIndex] = visible;

    // A hidden page can neither hold a tab nor be shown.
    if (!visible)
    {
        m_pages[(size_t) pageIndex]->SetVisible (false);
        if (m_active == pageIndex)
        {
            int  first = FirstPresentPage();
            m_active = (first >= 0) ? first : 0;
        }
    }

    // Relayout so the strip drops / regrows the tab and the active page fills
    // the content area. Before the first Layout there is nothing to reflow;
    // the pending change is honored when Layout first runs.
    if (m_haveLayout)
    {
        Layout (m_lastBoundsPx, m_lastScaler);
    }
    SetActivePage (m_active);
    Invalidate();
}


bool DxuiPropertySheet::IsPageVisible (int pageIndex) const
{
    if (pageIndex < 0 || pageIndex >= (int) m_present.size())
    {
        return false;
    }
    return m_present[(size_t) pageIndex];
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
    int  i = 0;

    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        if (m_pages[(size_t) i] == page) { return i; }
    }
    return -1;
}


int DxuiPropertySheet::FirstPresentPage () const
{
    int  i = 0;

    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        if (m_present[(size_t) i]) { return i; }
    }
    return -1;
}


int DxuiPropertySheet::TabIndexOfPage (int pageIndex) const
{
    int  tab = 0;
    int  i   = 0;

    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        if (!m_present[(size_t) i]) { continue; }
        if (i == pageIndex)         { return tab; }
        ++tab;
    }
    return -1;
}


int DxuiPropertySheet::PageIndexOfTab (int tabIndex) const
{
    int  tab = 0;
    int  i   = 0;

    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        if (!m_present[(size_t) i]) { continue; }
        if (tab == tabIndex)        { return i; }
        ++tab;
    }
    return -1;
}


void DxuiPropertySheet::BuildTabList (std::vector<DxuiTabStrip::Tab> & out) const
{
    int  i = 0;

    out.clear();
    out.reserve (m_pages.size());
    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        if (!m_present[(size_t) i]) { continue; }

        DxuiTabStrip::Tab  tab;
        tab.label = m_pages[(size_t) i]->Title();
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
    int               i     = 0;
    int               pos   = 0;
    int               count = 0;


    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        if (m_present[(size_t) i]) { present.push_back (i); }
    }

    count = (int) present.size();
    if (count <= 1)
    {
        return false;
    }

    // Cycle among present pages only (a hidden page has no tab to land on).
    for (i = 0; i < count; ++i)
    {
        if (present[(size_t) i] == m_active) { pos = i; break; }
    }

    pos = backward ? (pos - 1 + count) % count
                   : (pos + 1) % count;
    SetActivePage (present[(size_t) pos]);
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshApplyEnabled
//
//  Apply is enabled iff at least one page is dirty.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPropertySheet::RefreshApplyEnabled ()
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
//  ApplyAllDirtyPages
//
//  Commits every dirty page in order. A page whose OnApply() returns false
//  blocks the operation: that page becomes active and the method returns
//  false (so OK does not close). On success each committed page is marked
//  clean and Apply is disabled.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPropertySheet::ApplyAllDirtyPages ()
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
    int   pad     = scaler.Px (s_kContentPadDip);
    int   tabH    = scaler.Px (s_kTabStripHeightDip);
    int   rowH    = scaler.Px (DxuiButtonRow::kRowHeightDip);
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
        RECT  strip = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + tabH };
        int   tabW  = scaler.Px (s_kTabWidthDip);
        int   tx    = strip.left + pad;

        // DxuiTabStrip does not lay out its own tabs -- the caller owns each
        // tab's rect. Assign uniform fixed-width tabs left-to-right (only
        // present pages contribute a tab; no text renderer is available here
        // to content-size them).
        std::vector<DxuiTabStrip::Tab>  tabs;

        BuildTabList (tabs);
        for (i = 0; i < (int) tabs.size(); ++i)
        {
            tabs[(size_t) i].rect = { tx, strip.top, tx + tabW, strip.bottom };
            tx += tabW;
        }

        m_tabs->SetTabs     (std::move (tabs));
        m_tabs->SetSelected (TabIndexOfPage (m_active));
        m_tabs->Layout      (strip, scaler);
        m_tabs->SetDpi      (scaler.Dpi());
    }

    page.top     = boundsPx.top + tabH;
    page.bottom -= rowH;
    page.left   += pad;
    page.top    += pad;
    page.right  -= pad;
    page.bottom -= pad;

    // Lay out every page, not just the active one: SetActivePage only toggles
    // visibility (no relayout), so a page that has never been the active page
    // at Layout time would otherwise keep default {0,0} bounds and render
    // clipped at the top-left when first shown.
    for (i = 0; i < (int) m_pages.size(); ++i)
    {
        m_pages[(size_t) i]->Layout (page, scaler);
    }

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
                              return DxuiButtonRow::StandardRank (a.commandId) <
                                     DxuiButtonRow::StandardRank (b.commandId);
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
            vis[i].btn->SetDpi  (scaler.Dpi());
        }
    }
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

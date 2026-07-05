#include "Pch.h"

#include "DxuiPropertySheet.h"
#include "DxuiPropertyPage.h"

#include "Widgets/DxuiButton.h"


static constexpr int  s_kTabStripHeightDip   = 36;
static constexpr int  s_kTabWidthDip         = 100;
static constexpr int  s_kButtonRowHeightDip  = 44;
static constexpr int  s_kButtonWidthDip      = 96;
static constexpr int  s_kButtonHeightDip     = 28;
static constexpr int  s_kButtonGapDip        = 8;
static constexpr int  s_kButtonRowEdgePadDip = 16;
static constexpr int  s_kContentPadDip       = 16;




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
    size_t                          i = 0;


    OnBuildPages();

    m_tabs = CreateChild<DxuiTabStrip>();
    tabs.reserve (m_pages.size());
    for (i = 0; i < m_pages.size(); ++i)
    {
        DxuiTabStrip::Tab  tab;

        tab.label = m_pages[i]->Title();
        tabs.push_back (std::move (tab));
    }

    m_tabs->SetTabs    (std::move (tabs));
    m_tabs->SetSelected (0);
    m_tabs->SetOnChange ([this] (int index) { SetActivePage (index); });

    m_ok     = CreateChild<DxuiButton> (m_okText);
    m_cancel = CreateChild<DxuiButton> (L"Cancel");
    m_apply  = CreateChild<DxuiButton> (L"Apply");

    m_ok->SetCommandId     (IDOK);
    m_cancel->SetCommandId (IDCANCEL);
    m_apply->SetCommandId  (s_kApplyCommandId);

    // Honor a pre-Create SetApplyVisible(false): a plain [OK][Cancel] sheet.
    m_apply->SetVisible (m_applyVisible);

    m_ok->SetOnClick    ([this] () { if (ApplyAllDirtyPages()) { EndDialog (IDOK); } });
    m_apply->SetOnClick ([this] () { ApplyAllDirtyPages(); RefreshApplyEnabled(); });

    SetActivePage      (0);
    RefreshApplyEnabled();
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
        m_tabs->SetSelected (index);
    }

    m_pages[(size_t) index]->OnActivated();
    Invalidate();

Error:
    return;
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
    int   rowH    = scaler.Px (s_kButtonRowHeightDip);
    RECT  page    = boundsPx;
    int   i       = 0;



    SetBounds (boundsPx);

    if (m_tabs != nullptr)
    {
        RECT  strip = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + tabH };
        int   tabW  = scaler.Px (s_kTabWidthDip);
        int   tx    = strip.left + pad;

        // DxuiTabStrip does not lay out its own tabs -- the caller owns each
        // tab's rect. Assign uniform fixed-width tabs left-to-right (no text
        // renderer is available here to content-size them).
        std::vector<DxuiTabStrip::Tab>  tabs;

        tabs.reserve (m_pages.size());
        for (i = 0; i < (int) m_pages.size(); ++i)
        {
            DxuiTabStrip::Tab  tab;

            tab.label = m_pages[(size_t) i]->Title();
            tab.rect  = { tx, strip.top, tx + tabW, strip.bottom };
            tabs.push_back (std::move (tab));
            tx += tabW;
        }

        m_tabs->SetTabs     (std::move (tabs));
        m_tabs->SetSelected (m_active);
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

    // Button row, right-aligned along the bottom. Registration order is
    // OK, Cancel, Apply (Apply omitted when hidden). OK may carry a custom
    // width so a longer label ("OK (reboot)") fits without clipping.
    {
        int           okW      = (m_okWidthDip > 0) ? m_okWidthDip : s_kButtonWidthDip;
        DxuiButton *  order[3] = { m_ok, m_cancel, m_apply };
        int           allW[3]  = { okW, s_kButtonWidthDip, s_kButtonWidthDip };
        DxuiButton *  vis[3]   = {};
        int           visW[3]  = {};
        RECT          rects[3] = {};
        int           n        = 0;

        for (i = 0; i < 3; ++i)
        {
            if (order[i] == nullptr)                    { continue; }
            if (order[i] == m_apply && !m_applyVisible) { continue; }

            vis[n]  = order[i];
            visW[n] = allW[i];
            ++n;
        }

        LayoutButtonRow (boundsPx, scaler,
                         std::span<const int> (visW,  (size_t) n),
                         std::span<RECT>      (rects, (size_t) n));

        for (i = 0; i < n; ++i)
        {
            vis[i]->Layout (rects[i]);
            vis[i]->SetDpi  (scaler.Dpi());
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
    int     pad   = scaler.Px (s_kContentPadDip);
    int     btnH  = scaler.Px (s_kButtonHeightDip);
    int     gapPx = scaler.Px (s_kButtonGapDip);
    int     edge  = scaler.Px (s_kButtonRowEdgePadDip);
    int     y     = boundsPx.bottom - pad - btnH;
    int     total = 0;
    int     x     = 0;
    size_t  i     = 0;


    for (i = 0; i < widthsDip.size(); ++i)
    {
        total += scaler.Px (widthsDip[i]);
    }
    if (!widthsDip.empty())
    {
        total += (int) (widthsDip.size() - 1) * gapPx;
    }

    x = boundsPx.right - edge - total;

    for (i = 0; i < widthsDip.size() && i < outRects.size(); ++i)
    {
        int  w = scaler.Px (widthsDip[i]);

        outRects[i] = { x, y, x + w, y + btnH };
        x += w + gapPx;
    }
}

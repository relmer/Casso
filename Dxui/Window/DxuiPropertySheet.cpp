#include "Pch.h"

#include "DxuiPropertySheet.h"
#include "DxuiPropertyPage.h"

#include "Widgets/DxuiButton.h"
#include "Window/DxuiButtonRow.h"

#include <algorithm>


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
//  OnDialogTabSwitch
//
//  Ctrl+Tab / Ctrl+Shift+Tab: cycle to the next / previous page, wrapping.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPropertySheet::OnDialogTabSwitch (bool backward)
{
    int  count = (int) m_pages.size();
    int  next  = 0;


    if (count <= 1)
    {
        return false;
    }

    next = backward ? (m_active - 1 + count) % count
                    : (m_active + 1) % count;
    SetActivePage (next);
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

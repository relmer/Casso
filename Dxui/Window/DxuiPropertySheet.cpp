#include "Pch.h"

#include "DxuiPropertySheet.h"
#include "DxuiPropertyPage.h"

#include "Widgets/DxuiButton.h"


static constexpr int  s_kTabStripHeightDip   = 36;
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

    m_ok     = CreateChild<DxuiButton> (L"OK");
    m_cancel = CreateChild<DxuiButton> (L"Cancel");
    m_apply  = CreateChild<DxuiButton> (L"Apply");

    m_ok->SetCommandId     (IDOK);
    m_cancel->SetCommandId (IDCANCEL);
    m_apply->SetCommandId  (s_kApplyCommandId);

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
    int   btnW    = scaler.Px (s_kButtonWidthDip);
    int   btnH    = scaler.Px (s_kButtonHeightDip);
    int   gapPx   = scaler.Px (s_kButtonGapDip);
    int   edgePx  = scaler.Px (s_kButtonRowEdgePadDip);
    RECT  page    = boundsPx;
    int   count   = 3;
    int   total   = (count * btnW) + ((count - 1) * gapPx);
    int   x       = boundsPx.right  - edgePx - total;
    int   y       = boundsPx.bottom - pad - btnH;
    DxuiButton *  buttons[3] = { m_ok, m_cancel, m_apply };
    int   i       = 0;



    SetBounds (boundsPx);

    if (m_tabs != nullptr)
    {
        RECT  strip = { boundsPx.left, boundsPx.top, boundsPx.right, boundsPx.top + tabH };

        m_tabs->Layout (strip, scaler);
        m_tabs->SetDpi  (scaler.Dpi());
    }

    page.top     = boundsPx.top + tabH;
    page.bottom -= rowH;
    page.left   += pad;
    page.top    += pad;
    page.right  -= pad;
    page.bottom -= pad;

    if (m_active >= 0 && m_active < (int) m_pages.size())
    {
        m_pages[(size_t) m_active]->Layout (page, scaler);
    }

    for (i = 0; i < count; ++i)
    {
        RECT  b = { x, y, x + btnW, y + btnH };

        if (buttons[i] != nullptr)
        {
            buttons[i]->Layout (b);
            buttons[i]->SetDpi  (scaler.Dpi());
        }

        x += btnW + gapPx;
    }
}

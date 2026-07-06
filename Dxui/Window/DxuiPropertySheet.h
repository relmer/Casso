#pragma once

#include "Pch.h"
#include "Window/DxuiWindow.h"
#include "Widgets/DxuiTabStrip.h"
#include "Core/DxuiDpiScaler.h"


class DxuiPropertyPage;
class DxuiButton;




////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPropertySheet
//
//  Win32 PROPSHEET analog: a DxuiWindow that stacks a DxuiTabStrip over
//  the active DxuiPropertyPage, above a standard OK / Cancel / Apply
//  button row. Selecting a tab shows that page (the others hide); Apply
//  is enabled only while some page is dirty.
//
//  Buttons follow the DxuiWindow modal contract: Cancel is IDCANCEL
//  (Escape / close-box), OK applies every dirty page then closes with
//  IDOK, Apply applies without closing. A page's OnApply() returning
//  false blocks OK / Apply (kept open on the offending page).
//
//  Subclasses override OnBuildPages() to add pages via CreatePage<T>();
//  the base then builds the tab strip + button row. Consumers show it
//  with ShowModalDialog(IDOK) (or ShowModelessDialog(IDOK) for a live
//  overlay whose backdrop must keep animating).
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPropertySheet : public DxuiWindow
{
public:
    DxuiPropertySheet  () = default;
    ~DxuiPropertySheet () override = default;

    int  ActiveIndex () const { return m_active; }
    void SetActivePage (int index);

    //
    //  Page (tab) visibility. A hidden page contributes no tab, is skipped by
    //  tab cycling, and cannot be the active page; showing it restores its tab.
    //  Toggling relays the sheet out immediately once it has been laid out
    //  (so the strip reflows live), else the change is honored at first Layout.
    //  Hiding the active page moves the selection to the first visible page.
    //  Index is a page index (registration order), stable across hide/show.
    //
    void SetPageVisible (int pageIndex, bool visible);
    bool IsPageVisible  (int pageIndex) const;

    //
    //  Button-row customization. Call before Create (config), or before
    //  Show for a live OK relabel.
    //
    //  SetApplyVisible(false) drops the Apply button, so the row is a plain
    //  right-aligned [OK][Cancel] that commits only on OK -- the Win32
    //  OK/Cancel/Apply sheet reduces to a modal dialog. SetOkText relabels
    //  OK (e.g. "OK (reboot)"); SetOkWidthDip widens it so a longer label
    //  fits without clipping -- size it for the longest label up front, then
    //  relabel at runtime with just a repaint (the row does not reflow
    //  between resizes).
    //
    void  SetApplyVisible (bool visible);
    bool  ApplyVisible    () const { return m_applyVisible; }
    void  SetOkText       (std::wstring text);
    const std::wstring &  OkText () const { return m_okText; }
    void  SetOkWidthDip   (int widthDip);
    int   OkWidthDip      () const { return m_okWidthDip; }

    //
    //  Pure layout helper (exposed for tests). Right-aligns a row of buttons
    //  with the given per-button DIP widths (registration order, left to
    //  right) along the bottom edge of boundsPx, writing each button's pixel
    //  rect into outRects[0 .. widthsDip.size()). Encapsulates the reflow so
    //  the hidden-Apply / custom-OK-width positioning is unit-testable
    //  without a window. outRects must be at least widthsDip.size() long.
    //
    static void  LayoutButtonRow (const RECT           & boundsPx,
                                  const DxuiDpiScaler  & scaler,
                                  std::span<const int>   widthsDip,
                                  std::span<RECT>        outRects);


protected:
    //
    //  Create a page of type T (T : DxuiPropertyPage) as a child, wire its
    //  dirty notification, and register it as tab `title`. Call from
    //  OnBuildPages().
    //
    template <class T, class... Args>
    T *  CreatePage (std::wstring title, Args &&... args)
    {
        T *  page = this->template CreateChild<T> (std::move (title), std::forward<Args> (args)...);

        RegisterPage (page);
        return page;
    }

    //
    //  Subclass hook: create the pages (via CreatePage). Fires once during
    //  window creation, before the tab strip / buttons are built.
    //
    virtual void  OnBuildPages () {}

    //  Registration index of a page (as returned by CreatePage), or -1.
    //  Subclasses pass this index to SetPageVisible.
    int   IndexOfPage (const DxuiPropertyPage * page) const;

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  OnCreate () override;
    bool  OnDialogTabSwitch (bool backward) override;

    //
    //  Commit hooks (Template Method). The button row calls these so a
    //  subclass can run a cross-cutting commit instead of / in addition to
    //  per-page apply. The close rule is hr == S_OK: OnOk() returning S_OK
    //  closes with IDOK; S_FALSE vetoes (keep open, no error); FAILED keeps
    //  open on an error. Defaults: OnApply applies every dirty page (S_FALSE
    //  if a page blocks), OnOk = OnApply, OnCancel is a no-op.
    //
    virtual HRESULT  OnApply  ();
    virtual HRESULT  OnOk     ();
    virtual void     OnCancel ();


private:
    void  RegisterPage        (DxuiPropertyPage * page);
    void  RefreshApplyEnabled ();
    bool  ApplyAllDirtyPages  ();

    // Present-page <-> tab-position mapping. A page is "present" when its
    // m_present flag is set; only present pages get a tab, so a page index is
    // NOT the same as its tab position once any page is hidden.
    int   FirstPresentPage    () const;
    int   TabIndexOfPage      (int pageIndex) const;
    int   PageIndexOfTab      (int tabIndex) const;
    void  BuildTabList        (std::vector<DxuiTabStrip::Tab> & out) const;

    DxuiTabStrip *                   m_tabs   = nullptr;
    DxuiButton   *                   m_ok     = nullptr;
    DxuiButton   *                   m_cancel = nullptr;
    DxuiButton   *                   m_apply  = nullptr;
    std::vector<DxuiPropertyPage *>  m_pages;
    std::vector<bool>                m_present;     // per-page: has a tab / can be active
    int                              m_active = 0;

    // Last Layout inputs, so SetPageVisible can relayout the strip live.
    RECT                             m_lastBoundsPx = {};
    DxuiDpiScaler                    m_lastScaler;
    bool                             m_haveLayout   = false;

    // Button-row customization (see SetApplyVisible / SetOkText /
    // SetOkWidthDip). Honored at OnCreate and by Layout's reflow.
    bool                             m_applyVisible = true;
    std::wstring                     m_okText       = L"OK";
    int                              m_okWidthDip   = 0;   // 0 => s_kButtonWidthDip
};

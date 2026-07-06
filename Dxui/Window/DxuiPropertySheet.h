#pragma once

#include "Pch.h"
#include "Window/DxuiWindow.h"
#include "Widgets/DxuiTabStrip.h"


class DxuiPropertyPage;
class DxuiButton;
class DxuiDpiScaler;




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

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    void  OnCreate () override;
    bool  OnDialogTabSwitch (bool backward) override;


private:
    void  RegisterPage        (DxuiPropertyPage * page);
    void  RefreshApplyEnabled ();
    bool  ApplyAllDirtyPages  ();

    DxuiTabStrip *                   m_tabs   = nullptr;
    DxuiButton   *                   m_ok     = nullptr;
    DxuiButton   *                   m_cancel = nullptr;
    DxuiButton   *                   m_apply  = nullptr;
    std::vector<DxuiPropertyPage *>  m_pages;
    int                              m_active = 0;

    // Button-row customization (see SetApplyVisible / SetOkText /
    // SetOkWidthDip). Honored at OnCreate and by Layout's reflow.
    bool                             m_applyVisible = true;
    std::wstring                     m_okText       = L"OK";
    int                              m_okWidthDip   = 0;   // 0 => s_kButtonWidthDip
};

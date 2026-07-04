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


private:
    void  RegisterPage        (DxuiPropertyPage * page);
    void  RefreshApplyEnabled ();
    bool  ApplyAllDirtyPages  ();

    static constexpr int  s_kApplyCommandId = 0x2000;   // private (not IDOK/IDCANCEL)

    DxuiTabStrip *                   m_tabs   = nullptr;
    DxuiButton   *                   m_ok     = nullptr;
    DxuiButton   *                   m_cancel = nullptr;
    DxuiButton   *                   m_apply  = nullptr;
    std::vector<DxuiPropertyPage *>  m_pages;
    int                              m_active = 0;
};

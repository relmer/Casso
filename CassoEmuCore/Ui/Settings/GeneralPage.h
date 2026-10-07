#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPage
//
//  Settings > General: app-wide options that belong to no one subsystem.
//
//      * Check for updates automatically  (DxuiCheckbox)
//
//  The checkbox saves live through its callback, so Cancel has nothing to
//  revert and the page never marks itself dirty.
//
////////////////////////////////////////////////////////////////////////////////

class GeneralPage : public DxuiPropertyPage
{
public:
    explicit GeneralPage (std::wstring title = L"General");

    // Whether the once-a-day update check runs.
    using AutoUpdateFn = std::function<void (bool enabled)>;
    void  SetOnAutoUpdateToggled (AutoUpdateFn fn) { m_onAutoUpdateToggled = std::move (fn); }
    void  SetAutoUpdateChecked   (bool checked)   { m_autoUpdateCheckbox.SetChecked (checked); }

    void  Layout                 (const RECT & rect, const DxuiDpiScaler & scaler) override;

    // Test / wiring accessors.
    DxuiCheckbox       & GetAutoUpdateCheckbox ()       { return m_autoUpdateCheckbox; }
    const DxuiCheckbox & GetAutoUpdateCheckbox () const { return m_autoUpdateCheckbox; }

private:
    static constexpr int  kRowHeightDp  = 28;
    static constexpr int  kCheckWidthDp = 360;
    static constexpr int  kPagePadDp    = 16;

    AutoUpdateFn  m_onAutoUpdateToggled;
    DxuiCheckbox  m_autoUpdateCheckbox;
};

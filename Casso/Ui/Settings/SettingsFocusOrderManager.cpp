#include "Pch.h"

#include "SettingsFocusOrderManager.h"

#include "DisplayPage.h"
#include "HardwarePage.h"
#include "MachinePage.h"
#include "ThemePage.h"

#include "../UiShell.h"


namespace
{
    // Per-tab integer mapping kept in sync with SettingsPanel::TabIndex.
    // Held as plain ints in this helper so the focus manager doesn't
    // need to leak SettingsPanel's private enum.
    constexpr int  s_kTabMachine  = 0;
    constexpr int  s_kTabHardware = 1;
    constexpr int  s_kTabTheme    = 2;
    constexpr int  s_kTabDisplay  = 3;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Bind
//
////////////////////////////////////////////////////////////////////////////////

void SettingsFocusOrderManager::Bind (
    UiShell      * uiShell,
    DxuiTabStrip * tabs,
    MachinePage  * machinePage,
    HardwarePage * hardwarePage,
    ThemePage    * themePage,
    DisplayPage  * displayPage,
    DxuiButton   * applyButton,
    DxuiButton   * cancelButton)
{
    m_uiShell      = uiShell;
    m_tabs         = tabs;
    m_machinePage  = machinePage;
    m_hardwarePage = hardwarePage;
    m_themePage    = themePage;
    m_displayPage  = displayPage;
    m_applyButton  = applyButton;
    m_cancelButton = cancelButton;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Rebuild
//
//  Re-collect the focus list from the DxuiTabStrip, the currently
//  active page, and the Apply / Cancel buttons; reset FocusManager
//  to the first entry so Tab traversal starts at the tab strip.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsFocusOrderManager::Rebuild (int activeTab)
{
    int  nextId = 0;


    m_focusSetters.clear();

    if (m_uiShell == nullptr || m_tabs == nullptr)
    {
        return;
    }

    m_focusSetters.push_back ([this] (bool f) { m_tabs->SetFocused (f); });

    switch (activeTab)
    {
        case s_kTabMachine:
            // Machine page focus order: machine, speed, WP[0], WP[1],
            // writeMode, driveAudio, mechanism. The mechanism dropdown
            // stays in the list whether or not drive audio is enabled;
            // the widget ignores keyboard activation while disabled.
            m_focusSetters.push_back ([this] (bool f) { m_machinePage->MachineDropdown().SetFocused   (f); });
            m_focusSetters.push_back ([this] (bool f) { m_machinePage->SpeedDropdown().SetFocused     (f); });
            m_focusSetters.push_back ([this] (bool f) { m_machinePage->WriteProtect(0).SetFocused     (f); });
            m_focusSetters.push_back ([this] (bool f) { m_machinePage->WriteProtect(1).SetFocused     (f); });
            m_focusSetters.push_back ([this] (bool f) { m_machinePage->WriteModeDropdown().SetFocused (f); });
            m_focusSetters.push_back ([this] (bool f) { m_machinePage->DriveAudioToggle().SetFocused  (f); });
            m_focusSetters.push_back ([this] (bool f) { m_machinePage->MechanismDropdown().SetFocused (f); });
            break;
        case s_kTabHardware:
            // Hardware page has just the DxuiTreeView as its focusable.
            m_focusSetters.push_back ([this] (bool f) { m_hardwarePage->Tree().SetFocused (f); });
            break;
        case s_kTabTheme:
            // Theme page has just the dropdown as its focusable.
            m_focusSetters.push_back ([this] (bool f) { m_themePage->ThemeDropdown().SetFocused (f); });
            break;
        case s_kTabDisplay:
            // Display page focus order matches visual layout:
            // monitor, brightness, contrast, gamma, scanline toggle +
            // intensity, bloom toggle + radius + strength, color-bleed
            // toggle + width, persistence, restore. Toggles stay in
            // the list whether or not the matching slider is enabled.
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->MonitorDropdown().SetFocused     (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->BrightnessSlider().SetFocused    (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->ContrastSlider().SetFocused      (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->GammaSlider().SetFocused         (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->ScanlinesToggle().SetFocused     (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->ScanlinesSlider().SetFocused     (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->BloomToggle().SetFocused         (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->BloomRadiusSlider().SetFocused   (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->BloomStrengthSlider().SetFocused (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->ColorBleedToggle().SetFocused    (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->ColorBleedSlider().SetFocused    (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->PersistenceSlider().SetFocused   (f); });
            m_focusSetters.push_back ([this] (bool f) { m_displayPage->RestoreButton().SetFocused       (f); });
            break;
    }

    m_focusSetters.push_back ([this] (bool f) { m_cancelButton->SetFocused (f); });
    m_focusSetters.push_back ([this] (bool f) { m_applyButton->SetFocused  (f); });

    FocusManager & focus = m_uiShell->Focus();

    focus.Clear();
    for (nextId = 0; nextId < (int) m_focusSetters.size(); ++nextId)
    {
        focus.RegisterFocusable (nextId);
    }

    SyncToWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncToWidgets
//
//  Push the FocusManager's current-id state out to all registered
//  widgets via the cached setter callbacks.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsFocusOrderManager::SyncToWidgets ()
{
    int  current = 0;
    int  i       = 0;


    if (m_uiShell == nullptr)
    {
        return;
    }

    current = m_uiShell->Focus().Current();

    for (i = 0; i < (int) m_focusSetters.size(); ++i)
    {
        m_focusSetters[(size_t) i] (i == current);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnyDropdownOpenOnActivePage
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsFocusOrderManager::AnyDropdownOpenOnActivePage (int activeTab) const
{
    if (activeTab == s_kTabMachine && m_machinePage != nullptr)
    {
        return m_machinePage->MachineDropdown().IsOpen()   ||
               m_machinePage->SpeedDropdown().IsOpen()     ||
               m_machinePage->WriteModeDropdown().IsOpen() ||
               m_machinePage->MechanismDropdown().IsOpen();
    }
    if (activeTab == s_kTabTheme && m_themePage != nullptr)
    {
        return m_themePage->ThemeDropdown().IsOpen();
    }
    if (activeTab == s_kTabDisplay && m_displayPage != nullptr)
    {
        return m_displayPage->MonitorDropdown().IsOpen();
    }
    return false;
}

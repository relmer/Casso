#pragma once

#include "Pch.h"

#include "Widgets/DxuiTabStrip.h"
#include "Widgets/DxuiButton.h"


class UiShell;
class MachinePage;
class HardwarePage;
class ThemePage;
class DisplayPage;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsFocusOrderManager
//
//  Owns the per-tab focus order for the Settings panel.
//
//  Rebuild() repopulates the focus-setter list from the tab strip,
//  the currently-active page's focusables (in visual order), and the
//  trailing Cancel / OK buttons, then resets FocusManager to id 0 so
//  Tab traversal starts at the tab strip. SyncToWidgets() pushes the
//  FocusManager's current id back out to the registered widgets.
//
//  AnyDropdownOpenOnActivePage() is the modal-input gate -- routes
//  raw mouse events through the page's open dropdown instead of the
//  panel's own pick-tab-and-dispatch logic when a dropdown is up.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsFocusOrderManager
{
public:
    void  Bind (UiShell      * uiShell,
                DxuiTabStrip * tabs,
                MachinePage  * machinePage,
                HardwarePage * hardwarePage,
                ThemePage    * themePage,
                DisplayPage  * displayPage,
                DxuiButton   * applyButton,
                DxuiButton   * cancelButton);

    void  Rebuild                     (int activeTab);
    void  SyncToWidgets               ();
    bool  AnyDropdownOpenOnActivePage (int activeTab) const;


private:
    UiShell      * m_uiShell      = nullptr;
    DxuiTabStrip * m_tabs         = nullptr;
    MachinePage  * m_machinePage  = nullptr;
    HardwarePage * m_hardwarePage = nullptr;
    ThemePage    * m_themePage    = nullptr;
    DisplayPage  * m_displayPage  = nullptr;
    DxuiButton   * m_applyButton  = nullptr;
    DxuiButton   * m_cancelButton = nullptr;

    std::vector<std::function<void (bool)>>  m_focusSetters;
};

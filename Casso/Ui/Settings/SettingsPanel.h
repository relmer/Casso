#pragma once

#include "Pch.h"

#include "DisplayPage.h"
#include "HardwarePage.h"
#include "MachinePage.h"
#include "SettingsPanelState.h"
#include "ThemePage.h"

#include "../DpiScaler.h"
#include "../DwriteTextRenderer.h"
#include "../DxUiPainter.h"
#include "../Widgets/Button.h"
#include "../Widgets/ModalScrim.h"
#include "../Widgets/TabStrip.h"

#include "Core/MachineScanner.h"


class UiShell;
class UserConfigStore;
struct GlobalUserPrefs;
class ThemeManager;
class EmulatorShell;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanel
//
//  Owner of the consolidated settings surface. Composes the four
//  setting pages (Machine / Hardware / Theme stub / Display stub)
//  behind a TabStrip plus Apply / Cancel buttons and a `ModalScrim`
//  the apply path lights for reset-required confirmation.
//
//  The panel is render- and input-active only while `IsVisible()`
//  returns true. While visible, the host shell routes mouse + key
//  events into `OnMouse*` / `OnKey` instead of the chrome / emulator.
//  The emulator never pauses (FR-041) -- the panel is non-modal with
//  respect to CPU time.
//
//  Tab pages, the modal scrim, and the apply pipeline are wired
//  through dependency injection rather than singletons so unit tests
//  can drive the same state machine without the chrome stack.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsPanel
{
public:
    SettingsPanel  ();
    ~SettingsPanel ();

    HRESULT Initialize (UiShell         & uiShell,
                        UserConfigStore & ucs,
                        GlobalUserPrefs & prefs,
                        ThemeManager    & themes,
                        EmulatorShell   & emuShell,
                        IFileSystem     & fs);

    HRESULT Show      ();
    void    Hide      ();
    bool    IsVisible () const { return m_visible; }

    // Render + input routing. The host UiShell composites Paint after
    // chrome but before debug overlays; the routing helpers consume the
    // event only when the panel is visible so the emulator and chrome
    // continue to receive mouse / key when the panel is closed.
    void    Layout    (int viewportWidthPx, int viewportHeightPx, const DpiScaler & scaler);
    void    Paint     (DxUiPainter & painter, DwriteTextRenderer & text);
    void    OnMouseMove   (int x, int y);
    void    OnLButtonDown (int x, int y);
    void    OnLButtonUp   (int x, int y);
    bool    OnKey         (WPARAM vk);

    const SettingsPanelState & State () const { return m_state; }
    SettingsPanelState       & MutableState () { return m_state; }

    static const char * SpeedRadioValue (SettingsSpeedMode m);
    static const char * ColorRadioValue (SettingsColorMode m);

private:
    enum class TabIndex
    {
        Machine  = 0,
        Hardware = 1,
        Theme    = 2,
        Display  = 3,
    };


    void  LoadCurrentMachineIntoState ();
    void  PopulateMachineList         ();
    void  OnMachineSelected           (const std::string & machineName);
    void  OnApplyClicked              ();
    void  OnCancelClicked             ();
    void  CommitApply                 ();


    UiShell         * m_uiShell   = nullptr;
    UserConfigStore * m_ucs       = nullptr;
    GlobalUserPrefs * m_prefs     = nullptr;
    ThemeManager    * m_themes    = nullptr;
    EmulatorShell   * m_emuShell  = nullptr;
    IFileSystem     * m_fs        = nullptr;

    SettingsPanelState  m_state;
    bool                m_visible = false;

    TabStrip            m_tabs;
    MachinePage         m_machinePage;
    HardwarePage        m_hardwarePage;
    ThemePage           m_themePage;
    DisplayPage         m_displayPage;
    ModalScrim          m_scrim;
    Button              m_applyButton;
    Button              m_cancelButton;

    RECT                m_viewport     = {};
    RECT                m_panelRect    = {};
    RECT                m_captionRect  = {};
    int                 m_activeTab    = (int) TabIndex::Machine;
};

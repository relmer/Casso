#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiToggle.h"
#include "Widgets/DxuiSlider.h"
#include "Widgets/DxuiIconButton.h"
#include "Widgets/DxuiButton.h"


class IDxuiTheme;
class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage
//
//  Top page of the settings panel. Hosts every per-machine
//  user-mutable knob that isn't a hardware row:
//
//      * Machine selector (DxuiDropdown) -- the outermost control;
//        switching machines reloads the rest of the panel.
//      * Speed mode    (DxuiDropdown: authentic / 2x / max)
//      * Color mode    (DxuiDropdown: color / green / amber / white)
//      * Write protect (one DxuiCheckbox per drive: D1 / D2)
//      * Write mode    (DxuiDropdown: buffer+flush / copy-on-write)
//      * Drive audio   (DxuiToggle: floppy sound on/off)
//      * Mechanism     (DxuiDropdown: shugart / alps)
//
//  All change events route into the supplied `SettingsPanelState`
//  through the small setter API; widgets read their initial state
//  on `Rebuild`.
//
////////////////////////////////////////////////////////////////////////////////

class MachinePage : public DxuiPropertyPage
{
public:
    MachinePage ();

    using MachineSelectFn = std::function<void (const std::string & machineName)>;

    void  SetState              (SettingsPanelState * state);
    void  SetMachineList        (std::vector<std::string>  machineIds,
                                 std::vector<std::wstring> displayNames,
                                 int                       activeIndex);
    void  SetOnMachineSelected  (MachineSelectFn fn) { m_onMachineSelected = std::move (fn); }

    // Drive-audio preview hook. Invoked when a play button is clicked;
    // (drive 0/1, kind 0=motor 1=head 2=door, centered = play the test
    // drive panned to centre so the volume is judged without bias).
    using TestSoundFn = std::function<void (int drive, int kind, bool centered)>;
    void  SetOnTestSound        (TestSoundFn fn) { m_onTestSound = std::move (fn); }

    // Routes every owned dropdown's popup menu through the supplied
    // DxuiHwndSource's popup-host pool so the menu HWND escapes the
    // page's clipping bounds. Pass nullptr to revert to the legacy
    // in-panel PaintMenu path.
    void  SetPopupHost          (DxuiHwndSource * host);

    void  Layout                (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild               ();

    // Test accessors. Also surface mutable widget refs so the panel
    // can inline focus-setter lambdas and dropdown-open queries that
    // previously lived in CollectFocusables / AnyDropdownOpen shims.
    DxuiDropdown          & MachineDropdown      () { return m_machineDropdown; }
    DxuiDropdown          & SpeedDropdown        () { return m_speed; }
    DxuiDropdown          & WriteModeDropdown    () { return m_writeMode; }
    DxuiDropdown          & MechanismDropdown    () { return m_mechanism; }
    DxuiToggle            & DriveAudioToggle     () { return m_driveAudio; }
    DxuiCheckbox          & WriteProtect         (int drive) { return m_writeProtect[(size_t) drive]; }

    const DxuiToggle      & DriveAudioToggle     () const { return m_driveAudio; }
    const DxuiCheckbox    & WriteProtect         (int drive) const { return m_writeProtect[(size_t) drive]; }
    const DxuiDropdown    & SpeedDropdown        () const { return m_speed; }
    const DxuiDropdown    & WriteModeDropdown    () const { return m_writeMode; }
    const DxuiDropdown    & MechanismDropdown    () const { return m_mechanism; }
    const std::vector<std::string> & Machines () const { return m_machines; }
    int               ActiveMachineIndex     () const { return m_activeMachineIndex; }
    const DxuiDropdown  & MachineDropdown    () const { return m_machineDropdown; }

    // Friendly display name of the machine the dropdown currently shows
    // (e.g. "Apple ][") for the FR-131 restart notice. Empty if none.
    std::wstring SelectedMachineDisplayName () const
    {
        int  idx = m_machineDropdown.SelectedIndex();
        const std::vector<std::wstring> & items = m_machineDropdown.Items();
        return (idx >= 0 && idx < (int) items.size()) ? items[(size_t) idx] : std::wstring();
    }

private:
    void  ApplyDriveAudioChildEnabled (bool enabled);
    void  ConfigureVolumeSlider       (DxuiSlider & slider, const RECT & rect);
    void  ConfigurePanSlider          (DxuiSlider & slider, const RECT & rect);
    void  ResetDriveAudioToDefaults   ();

    SettingsPanelState         * m_state              = nullptr;
    std::vector<std::string>     m_machines;
    int                          m_activeMachineIndex = -1;
    MachineSelectFn              m_onMachineSelected;

    DxuiLabel                        m_machineLabel;
    DxuiLabel                        m_speedLabel;
    DxuiLabel                        m_wpLabel;
    DxuiLabel                        m_writeModeLabel;
    DxuiLabel                        m_audioLabel;
    DxuiLabel                        m_mechLabel;
    DxuiLabel                        m_motorLabel;
    DxuiLabel                        m_headLabel;
    DxuiLabel                        m_doorLabel;
    DxuiLabel                        m_panOneLabel;
    DxuiLabel                        m_panTwoLabel;

    DxuiDropdown                     m_machineDropdown;
    DxuiDropdown                     m_speed;
    DxuiDropdown                     m_writeMode;
    DxuiDropdown                     m_mechanism;
    DxuiToggle                       m_driveAudio;
    std::array<DxuiCheckbox, 2>      m_writeProtect;
    DxuiSlider                       m_motorVol;
    DxuiSlider                       m_headVol;
    DxuiSlider                       m_doorVol;
    DxuiSlider                       m_panOne;
    DxuiSlider                       m_panTwo;
    DxuiIconButton                   m_motorPlay;
    DxuiIconButton                   m_headPlay;
    DxuiIconButton                   m_doorPlay;
    DxuiIconButton                   m_panOnePlay;
    DxuiIconButton                   m_panTwoPlay;
    DxuiButton                       m_reset;
    TestSoundFn                      m_onTestSound;
};

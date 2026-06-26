#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "../DpiScaler.h"
#include "../Widgets/Button.h"
#include "../Widgets/Dropdown.h"
#include "../Widgets/Checkbox.h"
#include "../Widgets/IconButton.h"
#include "../Widgets/Label.h"
#include "../Widgets/Slider.h"
#include "../Widgets/Toggle.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachinePage
//
//  Top page of the settings panel. Hosts every per-machine
//  user-mutable knob that isn't a hardware row:
//
//      * Machine selector (Dropdown) -- the outermost control;
//        switching machines reloads the rest of the panel.
//      * Speed mode    (Dropdown: authentic / 2x / max)
//      * Color mode    (Dropdown: color / green / amber / white)
//      * Write protect (one Checkbox per drive: D1 / D2)
//      * Write mode    (Dropdown: buffer+flush / copy-on-write)
//      * Drive audio   (Toggle: floppy sound on/off)
//      * Mechanism     (Dropdown: shugart / alps)
//      * Motor / Head / Door volume (Sliders: 0-100%)
//      * Drive 1 / Drive 2 pan      (Sliders: Left..Center..Right)
//
//  All change events route into the supplied `SettingsPanelState`
//  through the small setter API; widgets read their initial state
//  on `Rebuild`.
//
////////////////////////////////////////////////////////////////////////////////

class MachinePage
{
public:
    using MachineSelectFn = std::function<void (const std::string & machineName)>;

    // Auditions a drive sound. drive = 0/1; kind 0=motor, 1=head, 2=door.
    // When `centered` the sound is balanced at the midpoint (volume
    // previews); otherwise it plays at the drive's dialed pan (pan
    // previews). The host syncs the engine before triggering.
    using TestSoundFn = std::function<void (int drive, int kind, bool centered)>;

    void  SetState              (SettingsPanelState * state);
    void  SetMachineList        (std::vector<std::string>  machineIds,
                                 std::vector<std::wstring> displayNames,
                                 int                       activeIndex);
    void  SetOnMachineSelected  (MachineSelectFn fn) { m_onMachineSelected = std::move (fn); }
    void  SetOnTestSound        (TestSoundFn fn)     { m_onTestSound = std::move (fn); }

    void  Layout                (const RECT & rect, const DpiScaler & scaler);
    void  Rebuild               ();

    void  OnLButtonDown         (int x, int y);
    void  OnLButtonUp           (int x, int y);
    void  OnMouseMove           (int x, int y);
    void  OnMouseHover          (int x, int y);
    bool  OnKey                 (WPARAM vk);
    void  Paint                 (DxUiPainter & painter, DwriteTextRenderer & text, const ChromeTheme & theme);

    void  CollectFocusables (std::vector<std::function<void (bool)>> & out);
    bool  AnyDropdownOpen   () const;

    // Test accessors.
    const Toggle      & DriveAudioToggle     () const { return m_driveAudio; }
    const Checkbox    & WriteProtect         (int drive) const { return m_writeProtect[(size_t) drive]; }
    const Dropdown    & SpeedDropdown        () const { return m_speed; }
    const Dropdown    & WriteModeDropdown() const { return m_writeMode; }
    const Dropdown    & MechanismDropdown    () const { return m_mechanism; }
    const std::vector<std::string> & Machines () const { return m_machines; }
    int               ActiveMachineIndex     () const { return m_activeMachineIndex; }
    const Dropdown  & MachineDropdown() const { return m_machineDropdown; }

private:
    static RECT MakeRect  (int l, int t, int w, int h);

    void  ApplyDriveAudioChildEnabled (bool enabled);
    void  ConfigureVolumeSlider       (Slider & slider, const RECT & rect);
    void  ConfigurePanSlider          (Slider & slider, const RECT & rect);
    void  ResetDriveAudioToDefaults   ();

    SettingsPanelState         * m_state              = nullptr;
    std::vector<std::string>     m_machines;
    int                          m_activeMachineIndex = -1;
    MachineSelectFn              m_onMachineSelected;
    TestSoundFn                  m_onTestSound;

    Label                        m_machineLabel;
    Label                        m_speedLabel;
    Label                        m_wpLabel;
    Label                        m_writeModeLabel;
    Label                        m_audioLabel;
    Label                        m_mechLabel;
    Label                        m_motorLabel;
    Label                        m_headLabel;
    Label                        m_doorLabel;
    Label                        m_panOneLabel;
    Label                        m_panTwoLabel;

    Dropdown                     m_machineDropdown;
    Dropdown                     m_speed;
    Dropdown                     m_writeMode;
    Dropdown                     m_mechanism;
    Toggle                       m_driveAudio;
    Slider                       m_motorVol;
    Slider                       m_headVol;
    Slider                       m_doorVol;
    Slider                       m_panOne;
    Slider                       m_panTwo;
    IconButton                   m_motorPlay;
    IconButton                   m_headPlay;
    IconButton                   m_doorPlay;
    IconButton                   m_panOnePlay;
    IconButton                   m_panTwoPlay;
    Button                       m_reset;
    std::array<Checkbox, 2>      m_writeProtect;
};

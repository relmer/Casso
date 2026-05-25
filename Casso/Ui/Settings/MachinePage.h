#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "../DpiScaler.h"
#include "../Widgets/Dropdown.h"
#include "../Widgets/Checkbox.h"
#include "../Widgets/Label.h"
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
//      * Drive audio   (Toggle: floppy sound on/off)
//      * Mechanism     (Dropdown: shugart / alps)
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

    void  SetState              (SettingsPanelState * state);
    void  SetMachineList        (std::vector<std::string> machines, int activeIndex);
    void  SetOnMachineSelected  (MachineSelectFn fn) { m_onMachineSelected = std::move (fn); }

    void  Layout                (const RECT & rect, const DpiScaler & scaler);
    void  Rebuild               ();

    void  OnLButtonDown         (int x, int y);
    void  OnLButtonUp           (int x, int y);
    void  OnMouseHover          (int x, int y);
    bool  OnKey                 (WPARAM vk);
    void  Paint                 (DxUiPainter & painter, DwriteTextRenderer & text) const;

    // Test accessors.
    const Toggle      & DriveAudioToggle     () const { return m_driveAudio; }
    const Checkbox    & WriteProtect         (int drive) const { return m_writeProtect[(size_t) drive]; }
    const Dropdown    & SpeedDropdown        () const { return m_speed; }
    const Dropdown    & ColorDropdown        () const { return m_color; }
    const Dropdown    & MechanismDropdown    () const { return m_mechanism; }
    const std::vector<std::string> & Machines () const { return m_machines; }
    int               ActiveMachineIndex     () const { return m_activeMachineIndex; }
    const Dropdown  & MachineDropdown() const { return m_machineDropdown; }

private:
    SettingsPanelState         * m_state              = nullptr;
    std::vector<std::string>     m_machines;
    int                          m_activeMachineIndex = -1;
    MachineSelectFn              m_onMachineSelected;

    Label                        m_machineLabel;
    Label                        m_speedLabel;
    Label                        m_colorLabel;
    Label                        m_wpLabel;
    Label                        m_audioLabel;
    Label                        m_mechLabel;

    Dropdown                     m_machineDropdown;
    Dropdown                     m_speed;
    Dropdown                     m_color;
    Dropdown                     m_mechanism;
    Toggle                       m_driveAudio;
    std::array<Checkbox, 2>      m_writeProtect;
};

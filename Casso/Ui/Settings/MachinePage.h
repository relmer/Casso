#pragma once

#include "Pch.h"

#include "SettingsPanelState.h"

#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiToggle.h"





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

class MachinePage
{
public:
    using MachineSelectFn = std::function<void (const std::string & machineName)>;

    void  SetState              (SettingsPanelState * state);
    void  SetMachineList        (std::vector<std::string>  machineIds,
                                 std::vector<std::wstring> displayNames,
                                 int                       activeIndex);
    void  SetOnMachineSelected  (MachineSelectFn fn) { m_onMachineSelected = std::move (fn); }

    void  Layout                (const RECT & rect, const DxuiDpiScaler & scaler);
    void  Rebuild               ();

    void  OnLButtonDown         (int x, int y);
    void  OnLButtonUp           (int x, int y);
    void  OnMouseHover          (int x, int y);
    bool  OnKey                 (WPARAM vk);
    void  Paint                 (DxuiPainter & painter, DxuiTextRenderer & text) const;

    void  CollectFocusables (std::vector<std::function<void (bool)>> & out);
    bool  AnyDropdownOpen   () const;

    // Test accessors.
    const DxuiToggle      & DriveAudioToggle     () const { return m_driveAudio; }
    const DxuiCheckbox    & WriteProtect         (int drive) const { return m_writeProtect[(size_t) drive]; }
    const DxuiDropdown    & SpeedDropdown        () const { return m_speed; }
    const DxuiDropdown    & WriteModeDropdown() const { return m_writeMode; }
    const DxuiDropdown    & MechanismDropdown    () const { return m_mechanism; }
    const std::vector<std::string> & Machines () const { return m_machines; }
    int               ActiveMachineIndex     () const { return m_activeMachineIndex; }
    const DxuiDropdown  & MachineDropdown() const { return m_machineDropdown; }

private:
    void  ApplyMechanismEnabled (bool enabled);

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

    DxuiDropdown                     m_machineDropdown;
    DxuiDropdown                     m_speed;
    DxuiDropdown                     m_writeMode;
    DxuiDropdown                     m_mechanism;
    DxuiToggle                       m_driveAudio;
    std::array<DxuiCheckbox, 2>      m_writeProtect;
};

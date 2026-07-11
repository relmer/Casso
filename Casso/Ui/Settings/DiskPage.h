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
//  DiskPage
//
//  Settings page for the Disk ][ controller's per-drive user knobs, split
//  out of MachinePage (GH #84) so drive configuration lives on its own tab
//  that only exists when the machine actually has a Disk ][ controller:
//
//      * Write protect (one DxuiCheckbox per drive: D1 / D2)
//      * Write mode    (DxuiDropdown: buffer+flush / copy-on-write)
//      * Drive audio   (DxuiToggle: floppy sound on/off) + its children:
//          - Mechanism  (DxuiDropdown: shugart / alps)
//          - Motor / head / door volume sliders + audition play buttons
//          - Drive 1 / 2 pan sliders + audition play buttons
//          - Restore defaults
//
//  Change events route into the supplied SettingsPanelState through its
//  setter API; widgets read their initial state on Rebuild.
//
////////////////////////////////////////////////////////////////////////////////

class DiskPage : public DxuiPropertyPage
{
public:
    explicit DiskPage (std::wstring title = L"Disk");

    void  SetState              (SettingsPanelState * state);

    // Drive-audio preview hook. Invoked when a play button is clicked;
    // (drive 0/1, kind 0=motor 1=head 2=door, centered = play the test
    // sound panned to centre so the volume is judged without bias).
    using TestSoundFn = std::function<void (int drive, int kind, bool centered)>;
    void  SetOnTestSound        (TestSoundFn fn) { m_onTestSound = std::move (fn); }

    // Routes every owned dropdown's popup menu through the supplied
    // DxuiHwndSource's popup-host pool so the menu HWND escapes the page's
    // clipping bounds. Pass nullptr to revert to the in-panel PaintMenu path.
    void  SetPopupHost          (DxuiHwndSource * host);

    void  Layout                (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild               ();

    // Test / wiring accessors.
    DxuiDropdown          & WriteModeDropdown    () { return m_writeMode; }
    DxuiDropdown          & MechanismDropdown    () { return m_mechanism; }
    DxuiToggle            & DriveAudioToggle     () { return m_driveAudio; }
    DxuiCheckbox          & WriteProtect         (int drive) { return m_writeProtect[(size_t) drive]; }

    const DxuiToggle      & DriveAudioToggle     () const { return m_driveAudio; }
    const DxuiCheckbox    & WriteProtect         (int drive) const { return m_writeProtect[(size_t) drive]; }
    const DxuiDropdown    & WriteModeDropdown    () const { return m_writeMode; }
    const DxuiDropdown    & MechanismDropdown    () const { return m_mechanism; }

private:
    void  ApplyDriveAudioChildEnabled (bool enabled);
    void  ConfigureVolumeSlider       (DxuiSlider & slider, const RECT & rect);
    void  ConfigurePanSlider          (DxuiSlider & slider, const RECT & rect);
    void  ResetDriveAudioToDefaults   ();

    SettingsPanelState         * m_state = nullptr;
    TestSoundFn                  m_onTestSound;

    DxuiLabel                        m_wpLabel;
    DxuiLabel                        m_writeModeLabel;
    DxuiLabel                        m_mockingboardLabel;
    DxuiLabel                        m_audioLabel;
    DxuiLabel                        m_mechLabel;
    DxuiLabel                        m_motorLabel;
    DxuiLabel                        m_headLabel;
    DxuiLabel                        m_doorLabel;
    DxuiLabel                        m_panOneLabel;
    DxuiLabel                        m_panTwoLabel;

    DxuiDropdown                     m_writeMode;
    DxuiDropdown                     m_mechanism;
    DxuiToggle                       m_mockingboard;
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
};

#pragma once

#include "Pch.h"

#include "../../Config/GlobalUserPrefs.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiToggle.h"
#include "Widgets/DxuiSlider.h"


class DxuiHwndSource;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage
//
//  Settings > Printing (FR-011): the host print-service preferences, shared by
//  every emulated machine (host print services are host resources, like the
//  keyboard). Edits write straight into GlobalUserPrefs; the sheet's apply
//  controller persists them on OK and reverts them on Cancel. None have a live
//  effect -- they bind at the next delivery.
//
//      * Resolution     (DxuiDropdown: 288 / 576 dpi, FR-028)
//      * Dot style      (DxuiDropdown: ink / plain, FR-027)
//
//  Plus the ImageWriter II mechanical-sound knobs (FR-034), which likewise
//  write straight into GlobalUserPrefs and bind when the printer next sounds.
//  A master toggle (on by default) enables the whole group; its children
//  disable + dim when it is off:
//
//      * Printer sound  (DxuiToggle: master enable) + its children:
//          - Volume     (DxuiSlider: 0..100 %)
//          - Manual pan (DxuiCheckbox: pin the stereo pan) + its child:
//              - Pan    (DxuiSlider: Left .. Center .. Right)
//
//  The delivery destination is no longer a preference: the preview's Print /
//  Save buttons (and the File menu's Copy) choose it per action, and Save
//  always prompts for the PNG path, so a stored destination + folder are gone.
//
////////////////////////////////////////////////////////////////////////////////

class PrintingPage : public DxuiPropertyPage
{
public:
    explicit PrintingPage (std::wstring title = L"Printing");

    // Backing store; seeds the controls and wires their change callbacks.
    void  SetPrefs             (GlobalUserPrefs * prefs);

    void  SetPopupHost         (DxuiHwndSource * host);

    void  Layout               (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild              ();

    // Test / wiring accessors.
    DxuiDropdown       & ResolutionDropdown  ()       { return m_dpi;         }
    DxuiDropdown       & DotStyleDropdown    ()       { return m_dotStyle;     }
    const DxuiDropdown & ResolutionDropdown  () const { return m_dpi;         }
    const DxuiDropdown & DotStyleDropdown    () const { return m_dotStyle;     }
    DxuiToggle         & SoundsToggle        ()       { return m_soundsToggle; }
    DxuiSlider         & VolumeSlider        ()       { return m_volume;       }
    DxuiCheckbox       & PanOverrideCheckbox ()       { return m_panOverride;  }
    DxuiSlider         & PanSlider           ()       { return m_pan;          }

private:
    static RECT  MakeRect        (int l, int t, int w, int h);
    static int   DotStyleToIndex (const std::string & token);

    void  ConfigureVolumeSlider (DxuiSlider & slider, const RECT & rect);
    void  ConfigurePanSlider    (DxuiSlider & slider, const RECT & rect);

    // Enable / dim the printer-sound children from the current prefs: the
    // volume + manual-pan controls follow the master toggle, and the pan
    // slider additionally follows the manual-pan checkbox.
    void  ApplyEnabledState     ();

    GlobalUserPrefs *  m_prefs = nullptr;

    DxuiLabel     m_dpiLabel;
    DxuiDropdown  m_dpi;
    DxuiLabel     m_styleLabel;
    DxuiDropdown  m_dotStyle;

    DxuiLabel     m_audioLabel;
    DxuiToggle    m_soundsToggle;
    DxuiLabel     m_volumeLabel;
    DxuiSlider    m_volume;
    DxuiCheckbox  m_panOverride;
    DxuiLabel     m_panLabel;
    DxuiSlider    m_pan;
};

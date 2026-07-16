#pragma once

#include "Pch.h"

#include "../../Config/GlobalUserPrefs.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiCheckbox.h"
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
//  write straight into GlobalUserPrefs and bind when the printer next sounds:
//
//      * Volume         (DxuiSlider: 0..100 %)
//      * Mute           (DxuiCheckbox: silence the printer bus)
//      * Manual pan     (DxuiCheckbox: pin the stereo pan) + its child:
//          - Pan        (DxuiSlider: Left .. Center .. Right)
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
    DxuiSlider         & VolumeSlider        ()       { return m_volume;       }
    DxuiCheckbox       & MuteCheckbox        ()       { return m_mute;         }
    DxuiCheckbox       & PanOverrideCheckbox ()       { return m_panOverride;  }
    DxuiSlider         & PanSlider           ()       { return m_pan;          }

private:
    void  ConfigureVolumeSlider (DxuiSlider & slider, const RECT & rect);
    void  ConfigurePanSlider    (DxuiSlider & slider, const RECT & rect);
    void  ApplyPanEnabled       (bool enabled);

    GlobalUserPrefs *  m_prefs = nullptr;

    DxuiLabel     m_dpiLabel;
    DxuiDropdown  m_dpi;
    DxuiLabel     m_styleLabel;
    DxuiDropdown  m_dotStyle;

    DxuiLabel     m_audioLabel;
    DxuiLabel     m_volumeLabel;
    DxuiSlider    m_volume;
    DxuiCheckbox  m_mute;
    DxuiCheckbox  m_panOverride;
    DxuiLabel     m_panLabel;
    DxuiSlider    m_pan;
};

#pragma once

#include "Pch.h"

#include "Config/GlobalUserPrefs.h"
#include "Core/MachineConfig.h"



class DxuiHwndSource;
class SettingsPanelState;





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
//  The one machine setting comes first: the connector the ImageWriter II is
//  attached to (DxuiComboBox), staged in SettingsPanelState like the Machine
//  tab's hardware. A change rebuilds the machine on OK. The banner above it
//  is set from the staged choice.
//
//      * Resolution     (DxuiComboBox: 288 / 576 dpi, FR-028)
//      * Dot style      (DxuiComboBox: ink / plain, FR-027)
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
//  A "Restore defaults" button (matching DiskPage's) puts every control on the
//  page back to the GlobalUserPrefs field defaults; Cancel still reverts it
//  like any other edit, OK persists it.
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

    // The staged machine, whose printer connector the page edits.
    void  SetState             (SettingsPanelState * state);

    // Reports a change the connector's choice made on another tab, such as
    // the Super Serial Card's mode.
    void  SetOnNotice          (std::function<void (const std::wstring &)> fn) { m_onNotice = std::move (fn); }

    // Picks up a change made elsewhere in the sheet that adds or removes a
    // connector, such as a card turned on in the Machine tab. Called each
    // dialog tick; rebuilds the list only when it differs.
    void  Sync                 ();

    void  SetPopupHost         (DxuiHwndSource * host);

    // Pure helpers: a connector as a choice in the list, and the banner for
    // the staged connector. Exposed for unit tests.
    static std::wstring  GetConnectionLabel (const PrinterConnection & connection);
    static std::wstring  GetBannerText      (const PrinterConnection & connection, const std::wstring & machineName);

    void  Layout               (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild              ();

    // Test / wiring accessors.
    DxuiComboBox       & GetConnectionDropdown  ()       { return m_connection;   }
    const std::wstring & GetBannerTextShown     () const { return m_bannerText;   }
    DxuiComboBox       & GetResolutionDropdown  ()       { return m_dpi;         }
    DxuiComboBox       & GetDotStyleDropdown    ()       { return m_dotStyle;     }
    const DxuiComboBox & GetResolutionDropdown  () const { return m_dpi;         }
    const DxuiComboBox & GetDotStyleDropdown    () const { return m_dotStyle;     }
    DxuiToggle         & GetSoundsToggle        ()       { return m_soundsToggle; }
    DxuiSlider         & GetVolumeSlider        ()       { return m_volume;       }
    DxuiCheckbox       & GetPanOverrideCheckbox ()       { return m_panOverride;  }
    DxuiSlider         & GetPanSlider           ()       { return m_pan;          }
    DxuiButton         & ResetButton            ()       { return m_reset;        }

private:
    static RECT  MakeRect        (int l, int t, int w, int h);
    static int   DotStyleToIndex (const std::string & token);

    void  ConfigureVolumeSlider (DxuiSlider & slider, const RECT & rect);
    void  ConfigurePanSlider    (DxuiSlider & slider, const RECT & rect);

    // Enable / dim the printer-sound children from the current prefs: the
    // volume + manual-pan controls follow the master toggle, and the pan
    // slider additionally follows the manual-pan checkbox.
    void  ApplyEnabledState     ();

    // Put every pref on the page back to the GlobalUserPrefs field defaults
    // and re-sync the widgets (explicitly -- NOT via Rebuild, which would
    // replace this button's own OnClick closure while it is executing).
    void  ResetToDefaults       ();

    void  SyncConnection        ();

    GlobalUserPrefs *     m_prefs = nullptr;
    SettingsPanelState *  m_state = nullptr;
    std::function<void (const std::wstring &)>  m_onNotice;

    DxuiInfoBanner  m_printerBanner;
    std::wstring    m_bannerText;

    DxuiLabel                       m_connectionLabel;
    DxuiComboBox                    m_connection;
    std::vector<PrinterConnection>  m_choices;

    // The last layout, for laying out again when the banner's text changes
    // its height.
    bool           m_hasLayout = false;
    RECT           m_lastRect  = {};
    DxuiDpiScaler  m_lastScaler;

    DxuiLabel     m_dpiLabel;
    DxuiComboBox  m_dpi;
    DxuiLabel     m_styleLabel;
    DxuiComboBox  m_dotStyle;

    DxuiLabel     m_audioLabel;
    DxuiToggle    m_soundsToggle;
    DxuiLabel     m_volumeLabel;
    DxuiSlider    m_volume;
    DxuiCheckbox  m_panOverride;
    DxuiLabel     m_panLabel;
    DxuiSlider    m_pan;

    DxuiButton    m_reset;
};

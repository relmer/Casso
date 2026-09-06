#pragma once

#include "Pch.h"

#include "../../Config/GlobalUserPrefs.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiDropdown.h"
#include "Widgets/DxuiInfoBanner.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiRadio.h"
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
    explicit PrintingPage (std::wstring title = L"Printing and Screenshots");

    // Backing store; seeds the controls and wires their change callbacks.
    void  SetPrefs             (GlobalUserPrefs * prefs);

    // The current machine's printer summary, shown in the info banner at the top
    // of the page (e.g. "Emulating an Apple ImageWriter II ..." or "No printer is
    // connected to this <machine>."). Set by the sheet when it opens the page.
    void  SetPrinterInfo       (const std::wstring & message) { m_printerBanner.SetText (message); }

    void  SetPopupHost         (DxuiHwndSource * host);

    void  Layout               (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild              ();

    // Test / wiring accessors.
    DxuiDropdown       & GetResolutionDropdown  ()       { return m_dpi;         }
    DxuiDropdown       & GetDotStyleDropdown    ()       { return m_dotStyle;     }
    const DxuiDropdown & GetResolutionDropdown  () const { return m_dpi;         }
    const DxuiDropdown & GetDotStyleDropdown    () const { return m_dotStyle;     }
    DxuiToggle         & GetSoundsToggle        ()       { return m_soundsToggle; }
    DxuiSlider         & GetVolumeSlider        ()       { return m_volume;       }
    DxuiCheckbox       & GetPanOverrideCheckbox ()       { return m_panOverride;  }
    DxuiSlider         & GetPanSlider           ()       { return m_pan;          }
    DxuiButton         & ResetButton            ()       { return m_reset;        }
    DxuiRadioGroup     & GetCaptureModeRadios   ()       { return m_captureMode;  }

    // Capture-mode token <-> radio index. The radio order is scene / crt /
    // raw, default first; the mapping is explicit rather than a cast so that
    // reordering the radios cannot silently repoint a stored setting.
    //
    // Public because it IS the logic on this page -- the rest is widget
    // plumbing Dxui already tests -- and a mapping that cannot be pinned
    // without a device and a window is a mapping nothing pins.
    static int          CaptureModeToIndex (const std::string & token);
    static const char * IndexToCaptureMode (int index);

    // What the folder row shows. An empty preference means "the default", so
    // the row shows the default's real path rather than nothing -- a blank
    // field would read as unset when it is in fact working.
    //
    // Long paths keep their last two components behind an ellipsis: the tail
    // is what tells one configured folder from another, and the head is the
    // part every path on the machine has in common.
    static std::wstring FolderForDisplay (const std::string & configured,
                                          const std::wstring & defaultFolder,
                                          size_t               maxChars);

    // Folder actions the page cannot perform itself -- both open shell UI, so
    // the shell installs them.
    using FolderFn = std::function<void ()>;
    void  SetOnBrowseFolder (FolderFn fn) { m_onBrowseFolder = std::move (fn); }
    void  SetOnOpenFolder   (FolderFn fn) { m_onOpenFolder   = std::move (fn); }

    // The default destination, so the folder row can show it when no folder
    // has been configured. Set by the sheet when it opens the page.
    void  SetDefaultScreenshotFolder (const std::wstring & path) { m_defaultShotFolder = path; }

    DxuiToggle         & GetSaveFileToggle      ()       { return m_saveFile;     }
    DxuiButton         & GetBrowseFolderButton  ()       { return m_browseFolder; }

private:
    static RECT  MakeRect        (int l, int t, int w, int h);
    static int   DotStyleToIndex (const std::string & token);

    void  ConfigureVolumeSlider (DxuiSlider & slider, const RECT & rect);
    void  ConfigurePanSlider    (DxuiSlider & slider, const RECT & rect);

    // Enable / dim the printer-sound children from the current prefs: the
    // volume + manual-pan controls follow the master toggle, and the pan
    // slider additionally follows the manual-pan checkbox.
    void  ApplyEnabledState     ();

    // Puts the current destination on the folder row. Called from Layout and
    // Rebuild both, since the sheet supplies the default and the prefs after
    // the first layout has already run.
    void  RefreshFolderText     ();

    // Put every pref on the page back to the GlobalUserPrefs field defaults
    // and re-sync the widgets (explicitly -- NOT via Rebuild, which would
    // replace this button's own OnClick closure while it is executing).
    void  ResetToDefaults       ();

    GlobalUserPrefs *  m_prefs = nullptr;

    DxuiInfoBanner  m_printerBanner;

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

    DxuiButton    m_reset;

    //  Screenshots. Second section on the page: printing stays first so a
    //  user navigating to printer settings still lands on them.
    DxuiLabel       m_screenshotHeading;
    DxuiLabel       m_captureModeLabel;
    DxuiRadioGroup  m_captureMode;

    //  Saving, and where. The folder row is a CHILD of the toggle: with
    //  saving off there is no file to have a folder for, so the row dims
    //  rather than sitting there inviting a change that does nothing.
    DxuiLabel       m_saveFileLabel;
    DxuiToggle      m_saveFile;
    DxuiLabel       m_folderLabel;
    DxuiLabel       m_folderPath;
    DxuiButton      m_browseFolder;
    DxuiButton      m_openFolder;

    FolderFn        m_onBrowseFolder;
    FolderFn        m_onOpenFolder;
    std::wstring    m_defaultShotFolder;
};

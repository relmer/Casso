#pragma once

#include "Pch.h"

#include "../../Config/GlobalUserPrefs.h"

#include "Window/DxuiPropertyPage.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiCheckbox.h"
#include "Widgets/DxuiLabel.h"
#include "Widgets/DxuiRadio.h"


class DxuiHwndSource;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotsPage
//
//  Settings > Screenshots: what the Screenshot command captures, and what it
//  does with it. Global prefs, shared by every emulated machine -- these
//  describe the host and the user's habits, nothing about the hardware.
//
//      * Capture       (DxuiRadioGroup: scene / crt / raw, default first)
//      * Save a file   (DxuiCheckbox) + its child:
//          - Folder    (the destination, as a link that opens it)
//
//  RADIOS RATHER THAN A DROPDOWN, and described. The three modes are not
//  self-explanatory from a one-word label -- a user has no prior name for the
//  difference between the scene and the picture -- so each option carries a
//  line saying what it contains, which a dropdown has nowhere to put.
//
//  Its own tab rather than a section under Printing. The two subjects are
//  related -- both are things Casso emits to the host -- but the described
//  radios are tall, and measured, the pair did not fit one page.
//
////////////////////////////////////////////////////////////////////////////////

class ScreenshotsPage : public DxuiPropertyPage
{
public:
    explicit ScreenshotsPage (std::wstring title = L"Screenshots");

    void  SetPrefs      (GlobalUserPrefs * prefs);
    void  SetPopupHost  (DxuiHwndSource * host);

    // The default destination, so the folder row can show it when no folder
    // has been configured. Set by the sheet when it opens the page.
    void  SetDefaultFolder (const std::wstring & path) { m_defaultFolder = path; }

    // Folder actions the page cannot perform itself -- both open shell UI, so
    // the shell installs them.
    using FolderFn = std::function<void ()>;
    void  SetOnBrowseFolder (FolderFn fn) { m_onBrowseFolder = std::move (fn); }
    void  SetOnOpenFolder   (FolderFn fn) { m_onOpenFolder   = std::move (fn); }

    void  Layout        (const RECT & rect, const DxuiDpiScaler & scaler) override;
    void  Rebuild       ();

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
    // NO TRUNCATION HERE. Fitting the path to the row is measured in pixels by
    // the link itself at paint time (DxuiElide::PathHead), because only the
    // renderer knows how wide a string actually is -- a character budget is
    // off by up to a factor of three across a proportional face.
    static std::wstring FolderForDisplay (const std::string &  configured,
                                          const std::wstring & defaultFolder);

    // Test / wiring accessors.
    DxuiRadioGroup & GetCaptureModeRadios  () { return m_captureMode;  }
    DxuiCheckbox   & GetSaveFileCheckbox   () { return m_saveFile;     }
    DxuiButton     & GetBrowseFolderButton () { return m_browseFolder; }
    DxuiButton     & GetFolderLink         () { return m_folderLink;   }

private:
    static RECT  MakeRect (int l, int t, int w, int h);

    // Enable / dim the folder row from the current prefs: with saving off
    // there is no file to have a folder for.
    void  ApplyEnabledState  ();

    // Puts the current destination on the folder row. Called from Layout and
    // Rebuild both, since the sheet supplies the default and the prefs after
    // the first layout has already run.
    void  RefreshFolderText  ();

    GlobalUserPrefs *  m_prefs = nullptr;

    DxuiLabel       m_captureLabel;
    DxuiRadioGroup  m_captureMode;

    DxuiCheckbox    m_saveFile;
    DxuiLabel       m_folderLabel;

    // The destination as a LINK rather than static text: it is a place, and a
    // place shown to a user is one they will want to open.
    DxuiButton      m_folderLink;
    DxuiButton      m_browseFolder;

    FolderFn        m_onBrowseFolder;
    FolderFn        m_onOpenFolder;
    std::wstring    m_defaultFolder;
};

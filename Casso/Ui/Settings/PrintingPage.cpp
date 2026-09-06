#include "Pch.h"

#include "PrintingPage.h"




// Layout metrics (DIP).
static constexpr int    s_kRowHeightDp     = 28;
static constexpr int    s_kLabelWidthDp    = 130;
static constexpr int    s_kDropdownWidthDp = 220;
static constexpr int    s_kCheckWidthDp    = 140;
static constexpr int    s_kResetWidthDp    = 130;   // matches DiskPage's restore button
static constexpr int    s_kChildIndentDp   = 18;    // one nesting step (matches DxuiTreeView)
static constexpr int    s_kSectionGapDp    = 14;
static constexpr int    s_kPagePadDp       = 16;

// A described radio row: one line of label over one of description, which
// needs roughly twice a plain row rather than the 28 an undescribed one takes.
static constexpr int    s_kCaptureOptionHeightDp = 34;

// The screenshots section runs tighter than the printing section above it:
// the page carries both, and measured, the ordinary 14 DIP gap put the folder
// row through the OK / Cancel bar. The rows are still a full row-height apart,
// so it reads as a denser group rather than a cramped one.
static constexpr int    s_kShotRowGapDp          = 8;

// The folder row shares one line between the path and its two buttons, since
// three rows do not fit in what the page has left.
static constexpr int    s_kFolderPathWidthDp     = 200;
static constexpr int    s_kFolderButtonWidthDp   = 88;
static constexpr int    s_kFolderButtonGapDp     = 8;
static constexpr size_t s_kFolderPathMaxChars    = 30;





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::MakeRect
//
////////////////////////////////////////////////////////////////////////////////

RECT PrintingPage::MakeRect (int l, int t, int w, int h)
{
    RECT  rc = { l, t, l + w, t + h };



    return rc;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::CaptureModeToIndex
//
//  Stored token -> radio index, in the radio order: scene / crt / raw.
//
//  Spelled out rather than cast from the enum. The enum's declaration order
//  and the radio's display order are two different things that happen to
//  agree today, and a cast would quietly turn a change to either one into a
//  wrong setting rather than a compile error.
//
////////////////////////////////////////////////////////////////////////////////

int PrintingPage::CaptureModeToIndex (const std::string & token)
{
    int   index = 0;



    if (token == "crt")
    {
        index = 1;
    }
    else if (token == "raw")
    {
        index = 2;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::IndexToCaptureMode
//
//  The inverse. An index outside the set resolves to the default rather than
//  writing a token nothing can parse.
//
////////////////////////////////////////////////////////////////////////////////

const char * PrintingPage::IndexToCaptureMode (int index)
{
    const char *   token = "scene";



    if (index == 1)
    {
        token = "crt";
    }
    else if (index == 2)
    {
        token = "raw";
    }

    return token;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::DotStyleToIndex
//
//  Dot-style token -> dropdown index (index 1 == "plain", else 0 == "ink").
//
////////////////////////////////////////////////////////////////////////////////

int PrintingPage::DotStyleToIndex (const std::string & token)
{
    return token == "plain" ? 1 : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::PrintingPage
//
//  Registers each member widget in the page's child tree (non-owning Adopt),
//  like DiskPage; Layout positions them and Rebuild wires their callbacks.
//
////////////////////////////////////////////////////////////////////////////////

PrintingPage::PrintingPage (std::wstring title)
    : DxuiPropertyPage (std::move (title))
{
    Adopt (m_printerBanner);
    Adopt (m_dpiLabel);
    Adopt (m_dpi);
    Adopt (m_styleLabel);
    Adopt (m_dotStyle);
    Adopt (m_audioLabel);
    Adopt (m_soundsToggle);
    Adopt (m_volumeLabel);
    Adopt (m_volume);
    Adopt (m_panOverride);
    Adopt (m_panLabel);
    Adopt (m_pan);
    Adopt (m_reset);
    Adopt (m_screenshotHeading);
    Adopt (m_captureModeLabel);
    Adopt (m_captureMode);
    Adopt (m_saveFileLabel);
    Adopt (m_saveFile);
    Adopt (m_folderLabel);
    Adopt (m_folderPath);
    Adopt (m_browseFolder);
    Adopt (m_openFolder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::RefreshFolderText
//
//  Puts the current destination on the folder row.
//
//  Called from BOTH Layout and Rebuild, which is the point: the sheet supplies
//  the default folder and the prefs after the page has already laid out once,
//  so a Layout-only assignment leaves the row blank until something else
//  provokes a re-layout -- and a blank row reads as a broken setting rather
//  than an unset one.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::RefreshFolderText()
{
    m_folderPath.SetText (FolderForDisplay (m_prefs != nullptr ? m_prefs->screenshotFolder : string(),
                                            m_defaultShotFolder,
                                            s_kFolderPathMaxChars));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::FolderForDisplay
//
//  What the folder row says.
//
//  An unset preference means "the default", and the row shows the default's
//  real path rather than an empty field -- blank reads as broken when the
//  feature is in fact working, and the user cannot tell where their files are
//  going without leaving the page.
//
//  A path too long for the row keeps its LAST components. The tail is what
//  distinguishes one configured folder from another; the head is what every
//  path on the machine has in common, so it is the half worth losing.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring PrintingPage::FolderForDisplay (const std::string &  configured,
                                             const std::wstring & defaultFolder,
                                             size_t               maxChars)
{
    std::wstring   path;
    size_t         cut  = std::wstring::npos;



    if (configured.empty())
    {
        path = defaultFolder;
    }
    else
    {
        path.assign (configured.begin(), configured.end());
    }

    if (path.length() <= maxChars || maxChars < 8)
    {
        return path;
    }

    //  Walk back from the end to a separator, so the ellipsis lands on a
    //  component boundary rather than mid-name.
    cut = path.rfind (L'\\', path.length() - 1);

    if (cut != std::wstring::npos && cut > 0)
    {
        cut = path.rfind (L'\\', cut - 1);
    }

    if (cut == std::wstring::npos || (path.length() - cut) > maxChars)
    {
        return L"..." + path.substr (path.length() - (maxChars - 3));
    }

    return L"..." + path.substr (cut);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::SetPrefs
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::SetPrefs (GlobalUserPrefs * prefs)
{
    m_prefs = prefs;
    Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::SetPopupHost
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::SetPopupHost (DxuiHwndSource * host)
{
    m_dpi.SetPopupHost      (host);
    m_dotStyle.SetPopupHost (host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::Layout
//
//  Lays out the Printing page: a status banner across the top, then the
//  labeled option rows below it.
//
//  The banner's height is MEASURED, not fixed, and the running y advances by
//  whatever it comes back as. Its message wraps, so a long one -- "no printer
//  card in this machine", say -- pushes every control below it down rather
//  than being clipped to a guessed height.
//
//  Which is why it comes first: reflow only works downward, so anything whose
//  height depends on content has to be laid out before what follows it.
//
//  Every control shares one x column so the page reads as an aligned form,
//  matching the other settings pages.
//
//  Dropdown items are set here beside the geometry, since the widest item and
//  the dropdown's width are the same design decision.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::Layout (const RECT & rect, const DxuiDpiScaler & scaler)
{
    UINT dpi         = scaler.GetDpi();
    int  pad         = scaler.ToPx (s_kPagePadDp);
    int  rowHeight   = scaler.ToPx (s_kRowHeightDp);
    int  labelWidth  = scaler.ToPx (s_kLabelWidthDp);
    int  dropWidth   = scaler.ToPx (s_kDropdownWidthDp);
    int  checkWidth  = scaler.ToPx (s_kCheckWidthDp);
    int  resetWidth  = scaler.ToPx (s_kResetWidthDp);
    int  childIndent = scaler.ToPx (s_kChildIndentDp);
    int  sectionGap  = scaler.ToPx (s_kSectionGapDp);
    int  shotGap     = scaler.ToPx (s_kShotRowGapDp);
    int  x           = rect.left + pad;
    int  y           = rect.top  + pad;
    int  controlsX   = x + labelWidth;



    // Printer info banner across the top: as wide as the page minus the control
    // margin, its height driven by how far the message wraps -- so a long message
    // pushes every control below it down (reflow) instead of clipping.
    int    bannerWidth  = (rect.right - rect.left) - pad * 2;
    int    bannerHeight = (int) std::ceil (m_printerBanner.GetPreferredHeightPx ((float) bannerWidth, scaler));

    m_printerBanner.SetRect (MakeRect (x, y, bannerWidth, bannerHeight));
    m_printerBanner.SetDpi  (dpi);
    y += bannerHeight + sectionGap;

    m_dpiLabel.SetRect  (MakeRect (x, y, labelWidth, rowHeight));
    m_dpiLabel.SetText  (L"Output resolution:");
    m_dpi.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_dpi.SetItems ({ L"288 dpi (draft)", L"576 dpi (high)" });
    y += rowHeight + sectionGap;

    m_styleLabel.SetRect  (MakeRect (x, y, labelWidth, rowHeight));
    m_styleLabel.SetText  (L"Dot style:");
    m_dotStyle.SetRect  (MakeRect (controlsX, y, dropWidth, rowHeight));
    m_dotStyle.SetItems ({ L"Ink (round dots)", L"Plain (square)" });
    y += rowHeight + sectionGap;

    // Printer sound: a master toggle whose children (volume, manual pan) indent
    // one step; the pan slider indents a second step under the manual-pan
    // checkbox. Labels indent; the controls stay in one column.
    m_audioLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_audioLabel.SetText (L"Printer sound:");
    m_soundsToggle.SetRect (MakeRect (controlsX, y, checkWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_volumeLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
    m_volumeLabel.SetText (L"Volume:");
    ConfigureVolumeSlider (m_volume, MakeRect (controlsX, y, dropWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_panOverride.SetRect  (MakeRect (x + childIndent, y, labelWidth + dropWidth - childIndent, rowHeight));
    m_panOverride.SetLabel (L"Manual stereo pan");
    y += rowHeight + sectionGap;

    m_panLabel.SetRect (MakeRect (x + childIndent * 2, y, labelWidth - childIndent * 2, rowHeight));
    m_panLabel.SetText (L"Pan:");
    ConfigurePanSlider (m_pan, MakeRect (controlsX, y, dropWidth, rowHeight));
    y += rowHeight + sectionGap;

    m_reset.SetLabel (L"Restore defaults");
    m_reset.Layout   (MakeRect (controlsX, y, resetWidth, rowHeight));
    y += rowHeight + sectionGap;

    //  SCREENSHOTS. Second section, below printing, on one page because both
    //  answer the same question: where what Casso emits ends up on the host.
    m_screenshotHeading.SetRect (MakeRect (x, y, bannerWidth, rowHeight));
    m_screenshotHeading.SetText (L"Screenshots");
    y += rowHeight + shotGap;

    m_captureModeLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_captureModeLabel.SetText (L"Capture:");

    //  RADIOS, NOT A DROPDOWN, and described. The three modes are not
    //  self-explanatory from a one-word label -- a user has no prior name for
    //  the difference between the scene and the picture -- so each option
    //  carries a line saying what it actually contains. Described options need
    //  a taller row than a plain one, which the widget splits rather than
    //  grows into.
    {
        int   optionH  = scaler.ToPx (s_kCaptureOptionHeightDp);
        int   optionW  = (rect.right - rect.left) - (controlsX - rect.left) - pad;
        int   optionY  = y;

        std::vector<DxuiRadioOption>  options;

        auto  addOption = [&] (const wchar_t * label, const wchar_t * description)
        {
            DxuiRadioOption   opt;

            opt.rect        = MakeRect (controlsX, optionY, optionW, optionH);
            opt.label       = label;
            opt.description = description;
            options.push_back (opt);
            optionY += optionH;
        };

        addOption (L"Scene",   L"The desk as it looks: monitor, glass and drives.");
        addOption (L"Picture", L"The screen with its CRT effects, nothing around it.");
        addOption (L"Raw",     L"The unprocessed screen, always 560x384.");

        m_captureMode.SetOptions (options);
        m_captureMode.SetBounds  (MakeRect (controlsX, y, optionW, optionH * 3));

        y = optionY + shotGap;
    }

    m_saveFileLabel.SetRect (MakeRect (x, y, labelWidth, rowHeight));
    m_saveFileLabel.SetText (L"Save a file:");
    m_saveFile.SetRect      (MakeRect (controlsX, y, checkWidth, rowHeight));
    y += rowHeight + shotGap;

    //  The folder row: label, the path, then the two actions. One row rather
    //  than three because the page has about a hundred DIP left below the
    //  radios and three rows do not fit in it.
    {
        int   pathW   = scaler.ToPx (s_kFolderPathWidthDp);
        int   btnW    = scaler.ToPx (s_kFolderButtonWidthDp);
        int   gap     = scaler.ToPx (s_kFolderButtonGapDp);

        m_folderLabel.SetRect (MakeRect (x + childIndent, y, labelWidth - childIndent, rowHeight));
        m_folderLabel.SetText (L"Folder:");

        m_folderPath.SetRect (MakeRect (controlsX, y, pathW, rowHeight));
        RefreshFolderText();

        m_browseFolder.SetLabel (L"Browse...");
        m_browseFolder.Layout   (MakeRect (controlsX + pathW + gap, y, btnW, rowHeight));

        m_openFolder.SetLabel (L"Open");
        m_openFolder.Layout   (MakeRect (controlsX + pathW + gap * 2 + btnW, y, btnW, rowHeight));

        y += rowHeight + shotGap;
    }

    m_dpiLabel.SetDpi      (dpi);
    m_dpi.SetDpi           (dpi);
    m_styleLabel.SetDpi    (dpi);
    m_dotStyle.SetDpi      (dpi);
    m_audioLabel.SetDpi    (dpi);
    m_soundsToggle.SetDpi  (dpi);
    m_volumeLabel.SetDpi   (dpi);
    m_volume.SetDpi        (dpi);
    m_panOverride.SetDpi   (dpi);
    m_panLabel.SetDpi      (dpi);
    m_pan.SetDpi           (dpi);
    m_reset.SetDpi         (dpi);
    m_screenshotHeading.SetDpi (dpi);
    m_captureModeLabel.SetDpi  (dpi);
    m_captureMode.SetDpi       (dpi);
    m_saveFileLabel.SetDpi     (dpi);
    m_saveFile.SetDpi          (dpi);
    m_folderLabel.SetDpi       (dpi);
    m_folderPath.SetDpi        (dpi);
    m_browseFolder.SetDpi      (dpi);
    m_openFolder.SetDpi        (dpi);

    DxuiPanel::SetBounds (rect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::Rebuild
//
//  Syncs widgets to GlobalUserPrefs and wires each change back into it. Edits
//  have no live effect; the apply controller persists / reverts them.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::Rebuild()
{
    GlobalUserPrefs *  prefs = m_prefs;



    if (prefs == nullptr)
    {
        return;
    }

    m_dpi.SetSelected      (prefs->printOutputDpi == 576 ? 1 : 0);
    m_dotStyle.SetSelected (DotStyleToIndex (prefs->printDotStyle));
    m_soundsToggle.SetChecked (prefs->printerAudioEnabled);
    m_volume.SetValue      (prefs->printerAudioVolume * 100.0f);
    m_panOverride.SetChecked (prefs->printerAudioPanOverride);
    m_pan.SetValue         (prefs->printerAudioPan * 100.0f);
    m_captureMode.SetSelected (CaptureModeToIndex (prefs->screenshotMode));
    ApplyEnabledState      ();

    m_saveFile.SetChecked (prefs->screenshotSaveFile);
    RefreshFolderText();

    m_captureMode.SetOnChange ([this, prefs] (int idx)
    {
        prefs->screenshotMode = IndexToCaptureMode (idx);
        MarkDirty();
    });

    m_saveFile.SetOnChange ([this, prefs] (bool checked)
    {
        prefs->screenshotSaveFile = checked;
        ApplyEnabledState();
        MarkDirty();
    });

    m_browseFolder.SetOnClick ([this] ()
    {
        if (m_onBrowseFolder)
        {
            m_onBrowseFolder();
        }
    });

    m_openFolder.SetOnClick ([this] ()
    {
        if (m_onOpenFolder)
        {
            m_onOpenFolder();
        }
    });

    m_dpi.SetSelect ([this, prefs] (int idx)
    {
        prefs->printOutputDpi = (idx == 1) ? 576 : 288;
        MarkDirty();
    });

    m_dotStyle.SetSelect ([this, prefs] (int idx)
    {
        prefs->printDotStyle = (idx == 1) ? "plain" : "ink";
        MarkDirty();
    });

    m_soundsToggle.SetOnChange ([this, prefs] (bool checked)
    {
        prefs->printerAudioEnabled = checked;
        ApplyEnabledState();
        MarkDirty();
    });

    m_volume.SetOnChange ([this, prefs] (float v)
    {
        prefs->printerAudioVolume = v / 100.0f;
        MarkDirty();
    });

    m_panOverride.SetOnChange ([this, prefs] (bool checked)
    {
        prefs->printerAudioPanOverride = checked;
        ApplyEnabledState();
        MarkDirty();
    });

    m_pan.SetOnChange ([this, prefs] (float v)
    {
        prefs->printerAudioPan = v / 100.0f;
        MarkDirty();
    });

    m_reset.SetOnClick ([this] { ResetToDefaults(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::ResetToDefaults
//
//  Puts every pref on the page back to the GlobalUserPrefs field defaults (a
//  default-constructed instance IS the authority -- no duplicated constants to
//  drift) and re-syncs the widgets explicitly. Deliberately does NOT call
//  Rebuild: that would replace this button's own OnClick closure while it is
//  still executing. Cancel reverts the reset like any other edit; OK persists.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::ResetToDefaults()
{
    GlobalUserPrefs   defaults;



    if (m_prefs == nullptr)
    {
        return;
    }

    m_prefs->printOutputDpi          = defaults.printOutputDpi;
    m_prefs->printDotStyle           = defaults.printDotStyle;
    m_prefs->printerAudioEnabled     = defaults.printerAudioEnabled;
    m_prefs->printerAudioVolume      = defaults.printerAudioVolume;
    m_prefs->printerAudioPanOverride = defaults.printerAudioPanOverride;
    m_prefs->printerAudioPan         = defaults.printerAudioPan;

    m_dpi.SetSelected         (m_prefs->printOutputDpi == 576 ? 1 : 0);
    m_dotStyle.SetSelected    (DotStyleToIndex (m_prefs->printDotStyle));
    m_soundsToggle.SetChecked (m_prefs->printerAudioEnabled);
    m_volume.SetValue         (m_prefs->printerAudioVolume * 100.0f);
    m_panOverride.SetChecked  (m_prefs->printerAudioPanOverride);
    m_pan.SetValue            (m_prefs->printerAudioPan * 100.0f);

    ApplyEnabledState();
    MarkDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::ConfigureVolumeSlider
//
//  0-100% linear volume slider with a "%" readout (matches DiskPage).
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::ConfigureVolumeSlider (DxuiSlider & slider, const RECT & rect)
{
    constexpr float  s_kVolumeMax = 100.0f;



    slider.SetRect      (rect);
    slider.SetRange     (0.0f, s_kVolumeMax);
    slider.SetStep      (1.0f);
    slider.SetSuffix    (L"%");
    slider.SetDecimalPlaces (0);
    slider.SetShowTicks (true);
    slider.SetTickInterval (10.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::ConfigurePanSlider
//
//  Bipolar Left..Center..Right pan slider (matches DiskPage's pan sliders).
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::ConfigurePanSlider (DxuiSlider & slider, const RECT & rect)
{
    constexpr float  s_kPanMax = 100.0f;



    slider.SetRect      (rect);
    slider.SetRange     (-s_kPanMax, s_kPanMax);
    slider.SetStep      (5.0f);
    slider.SetShowTicks (true);
    slider.SetTickInterval (25.0f);
    slider.SetCenterOriginFill (true);
    slider.SetValueFormatter ([] (float v) -> std::wstring
    {
        std::wstring  result;
        int           pct = (int) std::lround (v);

        if (pct == 0)
        {
            result = L"Center";
        }
        else if (pct < 0)
        {
            result = std::to_wstring (-pct) + L"% L";
        }
        else
        {
            result = std::to_wstring (pct) + L"% R";
        }

        return result;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintingPage::ApplyEnabledState
//
//  Cascade the two nested gates: the master "Printer sound" toggle enables the
//  volume slider + manual-pan checkbox, and the pan slider is additionally
//  gated by the manual-pan checkbox (it auto-follows the preview when off).
//  Disabled controls dim their labels to match.
//
////////////////////////////////////////////////////////////////////////////////

void PrintingPage::ApplyEnabledState()
{
    bool  soundsOn = (m_prefs != nullptr) && m_prefs->printerAudioEnabled;
    bool  panLive  = soundsOn && (m_prefs != nullptr) && m_prefs->printerAudioPanOverride;



    DxuiTextRole  childRole = soundsOn ? DxuiTextRole::Body : DxuiTextRole::Disabled;
    DxuiTextRole  panRole   = panLive  ? DxuiTextRole::Body : DxuiTextRole::Disabled;

    m_volume.SetEnabled      (soundsOn);
    m_panOverride.SetEnabled (soundsOn);
    m_pan.SetEnabled         (panLive);

    m_volumeLabel.SetTextRole (childRole);
    m_panLabel.SetTextRole    (panRole);

    //  With saving off there is no file to have a folder for, so the folder
    //  row dims rather than sitting there inviting a change that does nothing.
    {
        bool          savingOn   = (m_prefs != nullptr) && m_prefs->screenshotSaveFile;
        DxuiTextRole  folderRole = savingOn ? DxuiTextRole::Body : DxuiTextRole::Disabled;

        m_browseFolder.SetEnabled (savingOn);
        m_openFolder.SetEnabled   (savingOn);

        m_folderLabel.SetTextRole (folderRole);
        m_folderPath.SetTextRole  (folderRole);
    }
}

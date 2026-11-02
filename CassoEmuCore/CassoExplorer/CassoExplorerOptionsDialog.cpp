#include "Pch.h"

#include "CassoExplorer/CassoExplorerOptionsDialog.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Ui/Settings/DisplayPage.h"
#include "CassoExplorer/Window/ExplorerTextStyles.h"
#include "CassoExplorer/Window/ExplorerMetrics.h"




static constexpr int  s_kSheetWidthDip  = 720;   // Casso's own Settings sheet's width, which the Display page is laid out for
static constexpr int  s_kSheetHeightDip = 640;
static constexpr int  s_kSheetMinHeight = 320;





struct FullSendFormat
{
    const wchar_t  * label;
    const char     * extension;
};

static constexpr FullSendFormat  s_kFullSendFormats[] =
{
    { L"Automatic: .po for ProDOS disks, .dsk for others",                 ""     },
    { L".dsk: DOS-order sectors",                                         ".dsk" },
    { L".do: DOS-order sectors",                                          ".do"  },
    { L".po: ProDOS-order sectors",                                       ".po"  },
    { L".woz: WOZ 2 bit stream, as Applesauce and current emulators use", ".woz" },
    { L".nib: Nibbles, 6,656 bytes a track, the common nibble format",    ".nib" },
    { L".nb2: Nibbles, 6,384 bytes a track, for emulators that read .nb2", ".nb2" },
};





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerGeneralPage::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerGeneralPage::Init (DxuiLabel & previewNote, DxuiCheckbox & autoPreview, DxuiCheckbox & registration)
{
    m_previewNote = &previewNote;
    m_autoPreview = &autoPreview;
    m_register    = &registration;

    Adopt (previewNote);
    Adopt (autoPreview);
    Adopt (registration);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerGeneralPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerGeneralPage::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  previewEnd   = boundsDip.top + scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int  noteTop      = previewEnd + scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int  noteBottom   = noteTop + scaler.ToPx (kNoteHeightDip);
    int  registerTop  = noteBottom + scaler.ToPx (ExplorerMetrics::kStackGapDip * 4);
    int  registerEnd  = registerTop + scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);



    SetBounds (boundsDip);

    m_autoPreview->Layout (RECT { boundsDip.left, boundsDip.top, boundsDip.right, previewEnd }, scaler);
    m_previewNote->Layout (RECT { boundsDip.left + scaler.ToPx (kNoteIndentDip), noteTop, boundsDip.right, noteBottom }, scaler);
    m_register->Layout    (RECT { boundsDip.left, registerTop, boundsDip.right, registerEnd }, scaler);

    SetContentHeightPx (registerEnd - boundsDip.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerGeneralPage::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerGeneralPage::OnMouse (const DxuiMouseEvent & ev)
{
    return m_autoPreview->OnMouse (ev) || m_register->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerHostNamesPage::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerHostNamesPage::Init (DxuiLabel & namingNote, DxuiRadioGroup & naming)
{
    m_namingNote = &namingNote;
    m_naming     = &naming;

    Adopt (namingNote);
    Adopt (naming);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerHostNamesPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerHostNamesPage::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    std::vector<DxuiRadioOption>  options    = m_naming->GetOptions();
    int                           noteBottom = boundsDip.top + scaler.ToPx (kNoteHeightDip);
    int                           optionH    = scaler.ToPx (kOptionLabelHeightDip + kOptionDescHeightDip + kOptionGapDip);
    int                           optionsTop = noteBottom + scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int                           optionsEnd = optionsTop + optionH * (int) options.size();



    SetBounds (boundsDip);

    //  One option under another, each its name over an example of a name.
    for (size_t i = 0; i < options.size(); i++)
    {
        options[i].rect = RECT { boundsDip.left, optionsTop + optionH * (int) i, boundsDip.right, optionsTop + optionH * (int) (i + 1) };
    }

    m_naming->SetOptions (std::move (options));

    m_namingNote->Layout (RECT { boundsDip.left, boundsDip.top, boundsDip.right, noteBottom }, scaler);
    m_naming->Layout     (RECT { boundsDip.left, optionsTop, boundsDip.right, optionsEnd }, scaler);

    SetContentHeightPx (optionsEnd - boundsDip.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerHostNamesPage::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerHostNamesPage::OnMouse (const DxuiMouseEvent & ev)
{
    return m_naming->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerFullSendPage::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerFullSendPage::Init (DxuiLabel & note, DxuiLabel & folderLabel, DxuiLabel & folder, DxuiButton & browse, DxuiLabel & prefixLabel, DxuiTextInput & prefix, DxuiLabel & formatLabel, DxuiRadioGroup & format)
{
    m_folderLabel = &folderLabel;
    m_formatLabel = &formatLabel;
    m_prefixLabel = &prefixLabel;
    m_prefix = &prefix;
    m_note   = &note;
    m_folder = &folder;
    m_browse = &browse;
    m_format = &format;

    Adopt (note);
    Adopt (folderLabel);
    Adopt (formatLabel);
    Adopt (folder);
    Adopt (browse);
    Adopt (prefixLabel);
    Adopt (prefix);
    Adopt (format);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerFullSendPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerFullSendPage::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int                           noteEnd        = boundsDip.top + scaler.ToPx (kNoteHeightDip);
    int                           labelH         = scaler.ToPx (kLabelHeightDip);
    int                           folderLabelTop = noteEnd + scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int                           folderTop      = folderLabelTop + labelH;
    int                           folderEnd      = folderTop + scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int                           browseX        = boundsDip.right - scaler.ToPx (kBrowseWidthDip);
    int                           prefixLabelTop = folderEnd + scaler.ToPx (ExplorerMetrics::kStackGapDip * 2);
    int                           prefixTop      = prefixLabelTop + labelH;
    int                           prefixEnd      = prefixTop + scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int                           formatLabelTop = prefixEnd + scaler.ToPx (ExplorerMetrics::kStackGapDip * 2);
    int                           formatTop      = formatLabelTop + labelH;
    int                           optionH        = scaler.ToPx (kOptionHeightDip);
    int                           formatEnd      = formatTop + optionH * kOptionCount;
    std::vector<DxuiRadioOption>  options        = m_format->GetOptions();



    SetBounds (boundsDip);

    for (size_t i = 0; i < options.size(); i++)
    {
        options[i].rect = RECT { boundsDip.left, formatTop + optionH * (int) i, boundsDip.right, formatTop + optionH * (int) (i + 1) };
    }

    m_format->SetOptions (std::move (options));

    m_note->Layout   (RECT { boundsDip.left, boundsDip.top, boundsDip.right, noteEnd }, scaler);
    m_folderLabel->Layout (RECT { boundsDip.left, folderLabelTop, boundsDip.right, folderTop }, scaler);
    m_formatLabel->Layout (RECT { boundsDip.left, formatLabelTop, boundsDip.right, formatTop }, scaler);
    m_folder->Layout (RECT { boundsDip.left, folderTop, browseX - scaler.ToPx (ExplorerMetrics::kStackGapDip), folderEnd }, scaler);
    m_browse->SetDpi (scaler.GetDpi());
    m_browse->Layout (RECT { browseX, folderTop, boundsDip.right, folderEnd });
    m_prefixLabel->Layout (RECT { boundsDip.left, prefixLabelTop, boundsDip.right, prefixTop }, scaler);
    m_prefix->Layout (RECT { boundsDip.left, prefixTop, browseX - scaler.ToPx (ExplorerMetrics::kStackGapDip), prefixEnd }, scaler);
    m_format->Layout (RECT { boundsDip.left, formatTop, boundsDip.right, formatEnd }, scaler);

    SetContentHeightPx (formatEnd - boundsDip.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerFullSendPage::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerFullSendPage::OnMouse (const DxuiMouseEvent & ev)
{
    return m_browse->OnMouse (ev) || m_prefix->OnMouse (ev) || m_format->OnMouse (ev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerVerifyPage::Init
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerVerifyPage::Init (DxuiLabel & intro, std::array<Row, 3> & rows, DxuiLabel & overrideHead, DxuiLabel & overrideNote, DxuiLabel & limitHead, DxuiLabel & limitNote)
{
    m_intro        = &intro;
    m_rows         = &rows;
    m_overrideHead = &overrideHead;
    m_overrideNote = &overrideNote;
    m_limitHead    = &limitHead;
    m_limitNote    = &limitNote;

    Adopt (intro);

    for (Row & row : rows)
    {
        Adopt (row.label);
        Adopt (row.toggle);
        Adopt (row.state);
        Adopt (row.note);
    }

    Adopt (overrideHead);
    Adopt (overrideNote);
    Adopt (limitHead);
    Adopt (limitNote);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerVerifyPage::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerVerifyPage::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int  gap     = scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int  y       = boundsDip.top + scaler.ToPx (kIntroHeightDip);
    int  rowH    = scaler.ToPx (kRowHeightDip);
    int  noteH   = scaler.ToPx (kNoteHeightDip);
    int  headH   = scaler.ToPx (kHeadHeightDip);
    int  toggleX = boundsDip.left + scaler.ToPx (kToggleColDip);
    int  toggleW = scaler.ToPx (kToggleWidthDip);
    int  stateW  = scaler.ToPx (kStateWidthDip);



    SetBounds (boundsDip);

    m_intro->Layout (RECT { boundsDip.left, boundsDip.top, boundsDip.right, y }, scaler);

    for (Row & row : *m_rows)
    {
        noteH = scaler.ToPx (&row == &m_rows->back() ? kShortNoteDip : kNoteHeightDip);

        row.label.Layout (RECT { boundsDip.left, y, toggleX, y + rowH }, scaler);
        row.toggle.SetDpi (scaler.GetDpi());
        row.toggle.Layout (RECT { toggleX, y, toggleX + toggleW, y + rowH }, scaler);
        row.state.Layout (RECT { toggleX + toggleW + gap, y, toggleX + toggleW + gap + stateW, y + rowH }, scaler);

        y += rowH;

        row.note.Layout (RECT { boundsDip.left, y, boundsDip.right, y + noteH }, scaler);

        y += noteH + gap;
    }

    m_overrideHead->Layout (RECT { boundsDip.left, y, boundsDip.right, y + headH }, scaler);
    y += headH;
    m_overrideNote->Layout (RECT { boundsDip.left, y, boundsDip.right, y + scaler.ToPx (kOverrideDip) }, scaler);
    y += scaler.ToPx (kOverrideDip) + gap;
    m_limitHead->Layout (RECT { boundsDip.left, y, boundsDip.right, y + headH }, scaler);
    y += headH;
    m_limitNote->Layout (RECT { boundsDip.left, y, boundsDip.right, y + scaler.ToPx (kLimitDip) }, scaler);
    y += scaler.ToPx (kLimitDip);

    SetContentHeightPx (y - boundsDip.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerVerifyPage::OnMouse
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerVerifyPage::OnMouse (const DxuiMouseEvent & ev)
{
    for (Row & row : *m_rows)
    {
        if (row.toggle.OnMouse (ev))
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::GetNamingOptions
//
//  Each style's name, the file name of a binary program called GAME that loads
//  at $0803 in that style, and how the style stores the type and address.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiRadioOption> CassoExplorerOptionsDialog::GetNamingOptions()
{
    return { DxuiRadioOption { {}, L"Descriptive", L"The file type and load address are added to the base filename.", L"GAME.Binary.$0803.bin" },
             DxuiRadioOption { {}, L"CiderPress",  L"A # and six hex digits end the file name: two for the file type (06 is Binary) and four for the load address (0803).", L"GAME#060803" },
             DxuiRadioOption { {}, L"AppleSingle", L"The metadata is stored in the AppleSingle container file, along with the file's data.", L"GAME.as" } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::OnBuildPages
//
//  The General page, the host file name page, then Casso's Display page, which the dialog wires
//  to its own copy of the choices rather than to Casso's settings.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::OnBuildPages()
{
    m_naming.SetOptions  (GetNamingOptions());
    m_naming.SetSelected (m_choices.hostNaming);

    m_namingNote.SetText      (L"Apple files include metadata with the file's type (Applesoft BASIC, TEXT, ...) and, in the case of executable "
                               L"programs, the load address for that program. When an Apple file is copied to the PC, this metadata must be stored "
                               L"along with it so that when the file is copied back to an Apple disk, those properties can be restored.\n\n"
                               L"Casso Explorer can read any of these formats when copying files into an Apple disk. The setting below controls "
                               L"which format is used when Casso Explorer copies files from an Apple disk onto the PC.");
    ExplorerTextStyles::Apply (m_namingNote, ExplorerTextStyles::kFormNote);
    m_namingNote.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    //  Writes the user's file types only when OK is pressed with it changed.
    m_register.SetLabel   (L"Open disk images in Casso (.dsk, .do, .po, .woz, .nib, .nb2)");
    m_register.SetChecked (m_choices.registered);
    m_register.SetEnabled (!m_choices.packaged);

    m_previewNote.SetText      (L"When a file is selected, the preview pane opens if the file is of a known type, and closes if it isn't. "
                                L"Turn the pane on from the toolbar to keep it open for every file.");
    ExplorerTextStyles::Apply (m_previewNote, ExplorerTextStyles::kFormNote);
    m_previewNote.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_autoPreview.SetLabel   (L"Automatically preview known file types");
    m_autoPreview.SetChecked (m_choices.autoPreview);

    m_general = CreatePage<CassoExplorerGeneralPage> (L"General");
    m_general->Init (m_previewNote, m_autoPreview, m_register);

    m_hostNames = CreatePage<CassoExplorerHostNamesPage> (L"Host file names");
    m_hostNames->Init (m_namingNote, m_naming);

    WireFullSend();

    m_fullSend = CreatePage<CassoExplorerFullSendPage> (L"Full Send");
    m_fullSend->Init (m_fullSendNote, m_fullSendFolderLabel, m_fullSendFolder, m_fullSendBrowse, m_fullSendPrefixLabel, m_fullSendPrefix, m_fullSendFormatLabel, m_fullSendFormat);

    WireVerify();

    m_verifyPage = CreatePage<CassoExplorerVerifyPage> (L"Verified transfers");
    m_verifyPage->Init (m_verifyIntro, m_verifyRows, m_verifyOverrideHead, m_verifyOverrideNote, m_verifyLimitHead, m_verifyLimitNote);

    m_displayPage = CreatePage<DisplayPage> (L"Display");
    m_displayPage->SetPopupHost     (GetPopupHost());
    m_displayPage->SetCustomMonitor (m_choices.customArgb);
    m_displayPage->SetIntro         (L"These settings apply to previews of Hi-Res and Double Hi-Res pictures. Text, programs "
                                     L"and other files are previewed without them.");

    //  Seeded once the pages are laid out: the page sets its lists and its
    //  sliders' ranges as it lays itself out.
    WireDisplay();
    WireColorPicker();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::WireFullSend
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::WireFullSend()
{
    std::vector<DxuiRadioOption>  options;
    int                           selected = 0;



    for (size_t i = 0; i < std::size (s_kFullSendFormats); i++)
    {
        DxuiRadioOption  option;

        option.label = s_kFullSendFormats[i].label;
        options.push_back (option);

        selected = m_choices.fullSendExtension == s_kFullSendFormats[i].extension ? (int) i : selected;
    }

    m_fullSendFormat.SetOptions  (options);
    m_fullSendFormat.SetSelected (selected);

    m_fullSendNote.SetText      (L"Full Send archives a stack of floppies. Each disk you put in the Apple's drives is saved here as a disk image, "
                                 L"with no steps on the PC between disks. Just keep feeding the drives on the Apple. A list of what was sent is kept beside the images.");
    ExplorerTextStyles::Apply (m_fullSendNote, ExplorerTextStyles::kFormNote);
    m_fullSendNote.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);

    m_fullSendFolderLabel.SetText (L"Save images to");
    ExplorerTextStyles::Apply (m_fullSendFolderLabel, ExplorerTextStyles::kFormLabel);

    m_fullSendFormatLabel.SetText (L"Image type");
    ExplorerTextStyles::Apply (m_fullSendFormatLabel, ExplorerTextStyles::kFormLabel);

    m_fullSendPrefixLabel.SetText (L"File name prefix");
    ExplorerTextStyles::Apply (m_fullSendPrefixLabel, ExplorerTextStyles::kFormLabel);

    m_fullSendPrefix.SetTheme        (m_theme);
    m_fullSendPrefix.SetHwnd         (GetHwnd());
    m_fullSendPrefix.SetTextRenderer (GetTextRenderer());
    m_fullSendPrefix.SetPlaceholder  (L"Goes before each image's number, such as Box 3-");
    m_fullSendPrefix.SetText         (m_choices.fullSendPrefix);

    m_fullSendFolder.SetText (m_choices.fullSendFolder);

    m_fullSendBrowse.SetLabel   (L"Browse...");
    m_fullSendBrowse.SetOnClick ([this] ()
    {
        if (m_display.pickFolder)
        {
            m_display.pickFolder (GetHwnd(), m_choices.fullSendFolder);
            m_fullSendFolder.SetText (m_choices.fullSendFolder);
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::WireVerify
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::WireVerify()
{
    static constexpr const wchar_t *  kLabels[] = { L"Verify disks read from the Apple", L"Verify disks written to the Apple", L"Verify automatically when a read fails" };
    static constexpr const wchar_t *  kNotes[]  =
    {
        L"Each track is read three times, and the sectors are compared. If the reads disagree, the track is read up to nine times to find a consensus "
        L"for the correct data. A sector with read errors is accepted once two good reads match. A sector with only one good read is saved as read, but it is "
        L"listed as unreliable in the .unreadable.txt file beside the image, because no second read confirmed its data. A sector with no good read is "
        L"saved as zeros and listed in the same file.",
        L"Every track is already read back for validation by default. With this additional verification, a track that fails is rewritten up to twice "
        L"more, and the whole disk is compared with the image at the end. That catches a track damaged by a later write, for example on a misaligned drive.",
        L"A track with read errors is read again with the verification steps above. Verification happens only for that track.",
    };
    const bool  values[] = { m_choices.verifyReads, m_choices.verifyWrites, m_choices.autoVerify };



    for (DxuiLabel * label : { &m_verifyIntro, &m_verifyOverrideNote, &m_verifyLimitNote })
    {
        ExplorerTextStyles::Apply (*label, ExplorerTextStyles::kFormNote);
        label->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    }

    m_verifyIntro.SetText (L"Old disks can read differently from one pass to the next. Each sector has a checksum, but it is not a rigorous test and misses many "
                           L"errors, so a read can pass it with the wrong data. Verified transfers read and write disks more than once to catch a damaged copy. "
                           L"Verification applies to whole disks: Full Send, Create image and Write image. The trade-off for this additional rigor is speed; "
                           L"additional checks take a bit longer.");

    for (size_t i = 0; i < m_verifyRows.size(); i++)
    {
        CassoExplorerVerifyPage::Row  & row = m_verifyRows[i];

        row.label.SetText (kLabels[i]);
        ExplorerTextStyles::Apply (row.label, ExplorerTextStyles::kFormLabel);

        row.toggle.SetLabelVisible (false);
        row.toggle.SetChecked (values[i]);
        row.state.SetText (values[i] ? L"On" : L"Off");
        ExplorerTextStyles::Apply (row.state, ExplorerTextStyles::kFormLabel);
        row.toggle.SetOnChange ([&row] (bool on) { row.state.SetText (on ? L"On" : L"Off"); });

        row.note.SetText (kNotes[i]);
        ExplorerTextStyles::Apply (row.note, ExplorerTextStyles::kFormNote);
        row.note.SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Top);
    }

    m_verifyOverrideHead.SetText (L"Verify for specific connections, or for a single transfer");
    ExplorerTextStyles::Apply (m_verifyOverrideHead, ExplorerTextStyles::kFormLabel);
    m_verifyOverrideNote.SetText (L"A connection's settings can override these options. A drive's menu has Create verified image and Write image and verify for that transfer. "
                                  L"In Full Send on the Apple, press V at any time to turn verification on or off. Tracks already read aren't read again. "
                                  L"The Apple's screen always indicates whether verification is on.");

    m_verifyLimitHead.SetText (L"What it can't do");
    ExplorerTextStyles::Apply (m_verifyLimitHead, ExplorerTextStyles::kFormLabel);
    m_verifyLimitNote.SetText (L"A verified image has sectors that read consistently, but it may not run. Some copy protection schemes store data between tracks or sectors, "
                               L"where sector reads can't capture it. A disk that is likely copy-protected is read once and labeled, without retries. "
                               L"Use a flux tool such as Applesauce for those.");
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::GetOverrides
//
//  The changed effects of the selected monitor, made when first changed.
//
////////////////////////////////////////////////////////////////////////////////

CrtOverrides & CassoExplorerOptionsDialog::GetOverrides()
{
    return m_choices.crtOverrides[CassoExplorerPrefs::kMonitorNames[m_choices.monitor]];
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::SeedDisplay
//
//  The page's values for the selected monitor, each from the monitor, the
//  theme or the user, and labeled with where it came from.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::SeedDisplay()
{
    auto                        found    = m_choices.crtOverrides.find (CassoExplorerPrefs::kMonitorNames[m_choices.monitor]);
    CrtResolved                 resolved = CrtResolver::Resolve (CrtPresets::GetPreset (PicturePreview::GetPresetIndex ((PicturePreview::Monitor) m_choices.monitor)), m_display.themeDefaults,
                                                                 (found != m_choices.crtOverrides.end()) ? found->second : CrtOverrides());
    GlobalUserPrefsCrtSnapshot  snap;
    DisplayDefaultsHint         hint;



    snap.brightness         = resolved.values.brightness;
    snap.contrast           = resolved.values.contrast;
    snap.gamma              = resolved.values.gamma;
    snap.persistence        = resolved.values.persistence;
    snap.scanlinesEnabled   = resolved.values.scanlinesEnabled;
    snap.scanlinesIntensity = resolved.values.scanlinesIntensity;
    snap.bloomEnabled       = resolved.values.bloomEnabled;
    snap.bloomRadius        = resolved.values.bloomRadius;
    snap.bloomStrength      = resolved.values.bloomStrength;
    snap.colorBleedEnabled  = resolved.values.colorBleedEnabled;
    snap.colorBleedWidth    = resolved.values.colorBleedWidth;

    std::copy (std::begin (resolved.source), std::end (resolved.source), std::begin (hint.source));

    m_displayPage->SetMonitorIndex (m_choices.monitor);
    m_displayPage->SetInitialCrt   (snap);
    m_displayPage->SetDefaultsHint (hint);
    m_displayPage->Rebuild();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::WireDisplay
//
//  Each control changes the selected monitor, or one of its effects, and the
//  preview is drawn again with it. Text color is Casso's color monitor's
//  setting for text, which a picture never uses.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::WireDisplay()
{
    m_displayPage->SetOnMonitorChange ([this] (int index)
    {
        if (index >= 0 && index < (int) std::size (CassoExplorerPrefs::kMonitorNames) && index != m_choices.monitor)
        {
            m_choices.monitor = index;
            SeedDisplay();
            PreviewChoices();
        }
    });

    m_displayPage->SetOnBrightnessChange    ([this] (float value) { GetOverrides().brightness         = value / 100.0f; PreviewChoices(); });
    m_displayPage->SetOnContrastChange      ([this] (float value) { GetOverrides().contrast           = value / 100.0f; PreviewChoices(); });
    m_displayPage->SetOnGammaChange         ([this] (float value) { GetOverrides().gamma              = value; PreviewChoices(); });
    m_displayPage->SetOnPersistenceChange   ([this] (float value) { GetOverrides().persistence        = value / 100.0f; PreviewChoices(); });
    m_displayPage->SetOnScanlinesEnChange   ([this] (bool  value) { GetOverrides().scanlinesEnabled   = value; PreviewChoices(); });
    m_displayPage->SetOnScanlinesIntChange  ([this] (float value) { GetOverrides().scanlinesIntensity = value / 100.0f; PreviewChoices(); });
    m_displayPage->SetOnBloomEnChange       ([this] (bool  value) { GetOverrides().bloomEnabled       = value; PreviewChoices(); });
    m_displayPage->SetOnBloomRadiusChange   ([this] (float value) { GetOverrides().bloomRadius        = value; PreviewChoices(); });
    m_displayPage->SetOnBloomStrengthChange ([this] (float value) { GetOverrides().bloomStrength      = value / 100.0f; PreviewChoices(); });
    m_displayPage->SetOnColorBleedEnChange  ([this] (bool  value) { GetOverrides().colorBleedEnabled  = value; PreviewChoices(); });
    m_displayPage->SetOnColorBleedWChange   ([this] (float value) { GetOverrides().colorBleedWidth    = value; PreviewChoices(); });

    m_displayPage->SetOnRestoreDefaults ([this]()
    {
        m_choices.crtOverrides.erase (CassoExplorerPrefs::kMonitorNames[m_choices.monitor]);
        SeedDisplay();
        PreviewChoices();
    });

    m_displayPage->SetOnCustomColor ([this]()
    {
        m_colorPicker.SetHwnd (GetHwnd());
        m_colorPicker.Open    (m_choices.customArgb);
        Invalidate();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::WireColorPicker
//
//  Each color is drawn in the preview as it is picked, as Casso draws its
//  text color; the picker's Cancel returns the color it opened with.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::WireColorPicker()
{
    m_colorPicker.SetTitle    (L"Custom monitor color");
    m_colorPicker.SetOnChange ([this] (uint32_t argb) { SetCustomColor (argb); });
    m_colorPicker.SetOnClose  ([this] (bool, uint32_t argb)
    {
        SetCustomColor (argb);
        Invalidate();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::SetCustomColor
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::SetCustomColor (uint32_t argb)
{
    m_choices.customArgb = argb;
    m_displayPage->SetCustomMonitor (argb);
    PreviewChoices();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::PreviewChoices
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::PreviewChoices()
{
    if (m_display.preview)
    {
        m_display.preview (m_choices);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::OnOk
//
////////////////////////////////////////////////////////////////////////////////

HRESULT CassoExplorerOptionsDialog::OnOk()
{
    m_choices.registered  = m_register.IsChecked();
    m_choices.autoPreview = m_autoPreview.IsChecked();
    m_choices.fullSendExtension = s_kFullSendFormats[(size_t) m_fullSendFormat.GetSelected()].extension;
    m_choices.fullSendPrefix    = m_fullSendPrefix.GetText();
    m_choices.verifyReads       = m_verifyRows[0].toggle.IsChecked();
    m_choices.verifyWrites      = m_verifyRows[1].toggle.IsChecked();
    m_choices.autoVerify        = m_verifyRows[2].toggle.IsChecked();
    m_choices.hostNaming = m_naming.GetSelected();
    m_confirmed          = true;

    return DxuiPropertySheet::OnOk();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::OnCancel
//
//  The preview drawn as it was before the dialog opened.
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::OnCancel()
{
    if (m_display.preview)
    {
        m_display.preview (m_original);
    }

    DxuiPropertySheet::OnCancel();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::Layout
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    DxuiPropertySheet::Layout (boundsPx, scaler);
    m_colorPicker.Layout (boundsPx, scaler);

    //  The hex field places a clicked caret, and extends a drag, through it.
    m_colorPicker.SetTextRenderer (GetTextRenderer());
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::HasModalOverlay
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerOptionsDialog::HasModalOverlay() const
{
    return m_colorPicker.IsOpen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::PaintModalOverlay
//
////////////////////////////////////////////////////////////////////////////////

void CassoExplorerOptionsDialog::PaintModalOverlay (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_colorPicker.IsOpen())
    {
        m_colorPicker.Paint (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::OnOverlayMouse
//
//  Every mouse event goes to an open picker, the kinds the picker does not
//  handle included, and none goes to the page behind it.
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerOptionsDialog::OnOverlayMouse (const DxuiMouseEvent & ev)
{
    bool  isOpen = m_colorPicker.IsOpen();



    if (isOpen)
    {
        switch (ev.kind)
        {
            case DxuiMouseEventKind::Down:  m_colorPicker.OnLButtonDown (ev.positionDip.x, ev.positionDip.y); break;
            case DxuiMouseEventKind::Up:    m_colorPicker.OnLButtonUp   (ev.positionDip.x, ev.positionDip.y); break;
            case DxuiMouseEventKind::Move:  m_colorPicker.OnMouseHover  (ev.positionDip.x, ev.positionDip.y);
                                            m_colorPicker.OnMouseMove   (ev.positionDip.x, ev.positionDip.y); break;
            default:                        break;
        }

        Invalidate();
    }

    return isOpen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::OnOverlayChar
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerOptionsDialog::OnOverlayChar (wchar_t ch)
{
    bool  handled = m_colorPicker.IsOpen() && m_colorPicker.OnChar (ch);



    if (handled)
    {
        Invalidate();
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::OnOverlayKey
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerOptionsDialog::OnOverlayKey (WPARAM vk)
{
    bool  handled = m_colorPicker.IsOpen() && m_colorPicker.OnKey (vk);



    if (handled)
    {
        Invalidate();
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::GetCursorForPoint
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR CassoExplorerOptionsDialog::GetCursorForPoint (POINT clientPx) const
{
    return m_colorPicker.IsOpen() ? m_colorPicker.GetCursorForPoint (clientPx) : DxuiPropertySheet::GetCursorForPoint (clientPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerOptionsDialog::Ask
//
////////////////////////////////////////////////////////////////////////////////

bool CassoExplorerOptionsDialog::Ask (HWND owner, const IDxuiTheme * theme, const DisplayHost & display, Choices & inOutChoices)
{
    HRESULT                     hr     = S_OK;
    CassoExplorerOptionsDialog  dialog;
    DxuiWindow::CreateParams    params;



    dialog.m_theme    = theme;
    dialog.m_display  = display;
    dialog.m_original = inOutChoices;
    dialog.m_choices  = inOutChoices;

    //  OK and Cancel alone: every page commits on OK.
    dialog.SetApplyVisible   (false);
    dialog.SetDesignWidthDip (s_kSheetWidthDip);

    params.title                    = L"Options";
    params.hInstance                = GetModuleHandleW (nullptr);
    params.ownerHwnd                = owner;
    params.initialSizeDip           = { s_kSheetWidthDip, s_kSheetHeightDip };
    params.minSizeDip               = { s_kSheetWidthDip, s_kSheetMinHeight };
    params.fitToWorkArea            = true;
    params.resizable                = true;
    params.insetContentBelowCaption = true;
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.placement                = DxuiWindowPlacement::CenteredOnOwner;

    hr = dialog.Create (params);

    if (FAILED (hr))
    {
        return false;
    }

    dialog.SetTheme (theme);
    dialog.SeedDisplay();
    dialog.ShowModalDialog (IDOK);

    if (dialog.m_confirmed)
    {
        inOutChoices = dialog.m_choices;
    }

    return dialog.m_confirmed;
}

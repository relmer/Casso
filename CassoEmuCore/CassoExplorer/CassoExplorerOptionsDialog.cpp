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
    int  noteBottom   = boundsDip.top + scaler.ToPx (kNoteHeightDip);
    int  previewTop   = noteBottom + scaler.ToPx (ExplorerMetrics::kStackGapDip);
    int  previewEnd   = previewTop + scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);
    int  registerTop  = previewEnd + scaler.ToPx (ExplorerMetrics::kStackGapDip * 3);
    int  registerEnd  = registerTop + scaler.ToPx (ExplorerMetrics::kStackFieldHeightDip);



    SetBounds (boundsDip);

    m_previewNote->Layout (RECT { boundsDip.left, boundsDip.top, boundsDip.right, noteBottom }, scaler);
    m_autoPreview->Layout (RECT { boundsDip.left, previewTop, boundsDip.right, previewEnd }, scaler);
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
